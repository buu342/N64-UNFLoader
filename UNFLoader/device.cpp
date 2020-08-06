/***************************************************************
                           device.cpp
                               
Passes flashcart communication to more specific functions
***************************************************************/

#include <sys/stat.h>
#include "main.h"
#include "helper.h"
#include "debug.h"
#include "device.h"
#include "device_64drive.h"
#include "device_everdrive3.h"
#include "device_everdrive7.h"


/*********************************
        Function Pointers
*********************************/

void (*funcPointer_open)(ftdi_context_t*);
void (*funcPointer_sendrom)(ftdi_context_t*, FILE *file, u32 size);
void (*funcPointer_close)(ftdi_context_t*);


/*********************************
             Globals
*********************************/

static ftdi_context_t local_usb = {0, };


/*==============================
    device_find
    Finds the flashcart plugged in to USB
    @param The cart to check for (CART_NONE for automatic checking)
==============================*/

void device_find(int automode)
{
    int i;
    int found = -1;
    ftdi_context_t *cart = &local_usb;

    // Initialize FTD
    if (automode == CART_NONE)
        pdprint("Attempting flashcart autodetection.\n", CRDEF_PROGRAM);
    testcommand(FT_CreateDeviceInfoList(&cart->devices), "Error: USB Device not ready.\n");

    // Check if the device exists
    if(cart->devices == 0)
        terminate("Error: No devices found.\n");

    // Allocate storage and get device info list
    cart->dev_info = (FT_DEVICE_LIST_INFO_NODE*) malloc( sizeof(FT_DEVICE_LIST_INFO_NODE) *cart->devices);
    FT_GetDeviceInfoList(cart->dev_info, &cart->devices);

    // Search the devices
    for(i=0; (unsigned)i < cart->devices; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if ((automode == CART_NONE || automode == CART_64DRIVE1) && device_test_64drive1(cart, i))
        {
            device_set_64drive1(cart, i);
            if (automode == CART_NONE) 
                pdprint_replace("64Drive HW1 autodetected!\n", CRDEF_PROGRAM);
            break;
        }

        // Look for 64drive HW2 (FT232H Synchronous FIFO mode)
        if ((automode == CART_NONE || automode == CART_64DRIVE2) &&device_test_64drive2(cart, i))
        {
            device_set_64drive2(cart, i);
            if (automode == CART_NONE) 
                pdprint_replace("64Drive HW2 autodetected!\n", CRDEF_PROGRAM);
            break;
        }

        // Look for EverDrive 3.0
        if ((automode == CART_NONE || automode == CART_EVERDRIVE3) && device_test_everdrive3(cart, i))
        {
            device_set_everdrive3(cart, i);
            if (automode == CART_NONE)
                pdprint_replace("EverDrive 3.0 autodetected!\n", CRDEF_PROGRAM);
            break;
        }

        // Look for EverDrive X7
        if ((automode == CART_NONE || automode == CART_EVERDRIVE7) && device_test_everdrive7(cart, i))
        {
            device_set_everdrive7(cart, i);
            if (automode == CART_NONE)
                pdprint_replace("EverDrive X7 autodetected!\n", CRDEF_PROGRAM);
            break;
        }
    }

    // Finish
    free(cart->dev_info);
    if (cart->carttype == CART_NONE)
    {
        if (automode == CART_NONE)
            terminate("Error: No flashcart detected.\n");
        else
            terminate("Error: Requested flashcart not found.\n");
    }
}


/*==============================
    device_set_64drive1
    Marks the cart as being 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_64drive1(ftdi_context_t* cart, int index)
{
    // Set cart settings
    cart->device_index = index;
    cart->synchronous = 0;
    cart->carttype = CART_64DRIVE1;

    // Set function pointers
    funcPointer_open = &device_open_64drive;
    funcPointer_sendrom = &device_sendrom_64drive;
    funcPointer_close = &device_close_64drive;
}


/*==============================
    device_set_64drive2
    Marks the cart as being 64Drive HW2
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_64drive2(ftdi_context_t* cart, int index)
{
    // Do exactly the same as device_set_64drive1
    device_set_64drive1(cart, index);

    // But modify the important cart settings
    cart->synchronous = 1;
    cart->carttype = CART_64DRIVE2;
}


/*==============================
    device_set_everdrive3
    Marks the cart as being EverDrive 3.0
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_everdrive3(ftdi_context_t* cart, int index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_EVERDRIVE3;

    // Set function pointers
    funcPointer_open = &device_open_everdrive3;
    funcPointer_sendrom = &device_sendrom_everdrive3;
    funcPointer_close = &device_close_everdrive3;
}


/*==============================
    device_set_everdrive7
    Marks the cart as being EverDrive X7
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_everdrive7(ftdi_context_t* cart, int index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_EVERDRIVE7;

    // Set function pointers
    funcPointer_open = &device_open_everdrive7;
    funcPointer_sendrom = &device_sendrom_everdrive7;
    funcPointer_close = &device_close_everdrive7;
}


/*==============================
    device_open
    Calls the function to open the flashcart
==============================*/

void device_open()
{
    funcPointer_open(&local_usb);
    pdprint("USB connection opened.\n", CRDEF_PROGRAM);
}


/*==============================
    device_sendrom
    Opens the ROm and calls the function to send it to the flashcart
==============================*/

void device_sendrom(char* rompath)
{
    bool escignore = false;
    bool resend = false;
    time_t  lastmod = 0;
    struct  stat finfo;
    errno_t err;
    FILE    *file;

    for ( ; ; )
    {
        unsigned char rom_header[4];

        // Open the ROM and get info about it 
        err = fopen_s(&file, rompath, "rb");
        if (err != 0)
        {
            device_close();
            terminate("Unable to open file '%s'.\n", rompath);
        }
	    stat(rompath, &finfo);

        // Read the ROM header to check if its byteswapped
        fread(rom_header, 4, 1, file);
        if (!(rom_header[0] == 0x80 && rom_header[1] == 0x37 && rom_header[2] == 0x12 && rom_header[3] == 0x40))
            global_z64 = true;
        fseek(file, 0, SEEK_SET);

        // If the file was not modified
        if (!resend && lastmod != 0 && lastmod == finfo.st_mtime)
        {
            int count = 0;

            // Close the file and wait for three seconds
            fclose(file);
            while (count < 30)
            {
                Sleep(100);
                count++;

                // Disable ESC ignore if the key was released
                if (!GetAsyncKeyState(VK_ESCAPE) && escignore)
                    escignore = false;

                // Check if ESC was pressed
                if (GetAsyncKeyState(VK_ESCAPE) && !escignore)
                {
                    pdprint("\nExiting listen mode.\n", CRDEF_PROGRAM);
                    return;
                }

                // Check if R was pressed
                if (GetAsyncKeyState(0x52))
                {
                    resend = true;
                    pdprint("\nReuploading ROM by request.\n", CRDEF_PROGRAM);
                    break;
                }
            }

            // Restart the while loop
            if (!resend)
                clearerr(file);
            continue;
        }
        else if (lastmod != 0)
        {
            pdprint("\nFile change detected. Reuploading ROM.\n", CRDEF_PROGRAM);
            lastmod = finfo.st_mtime;
        }

        // Initialize lastmod
        if (lastmod == 0)
            lastmod = finfo.st_mtime;

        // Complain if the ROM is too small
        if (finfo.st_size < 1052672)
            pdprint("ROM is smaller than 1MB, it might not boot properly.\n", CRDEF_PROGRAM); 

        // Send the ROM
        funcPointer_sendrom(&local_usb, file, finfo.st_size);

        // Close the file pipe
        fclose(file);

        // Start Debug Mode
        if (global_debugmode) 
        {
            debug_main(&local_usb);
            escignore = true;
        }

        // Break out of the while loop if not in listen mode
        if (!global_listenmode)
            break;

        // Print that we're waiting for changes
        pdprint("Waiting for file changes. Press ESC to stop. Press R to resend.\n", CRDEF_INPUT);
        resend = false;
    }
}


/*==============================
    device_isopen
    Checks if the device is open
    @returns Whether the device is open
==============================*/

bool device_isopen()
{
    ftdi_context_t* cart = &local_usb;
    return (cart->handle != NULL);
}


/*==============================
    device_getcarttype
    Returns the type of the connected cart
    @returns The cart type
==============================*/

DWORD device_getcarttype()
{
    ftdi_context_t* cart = &local_usb;
    return cart->carttype;
}


/*==============================
    device_close
    Calls the function to close the flashcart
==============================*/

void device_close()
{
    // Should never happen, but just in case...
    if (local_usb.handle == NULL)
        return;

    // Close the device
    funcPointer_close(&local_usb);
    pdprint("USB connection closed.\n", CRDEF_PROGRAM);
}

