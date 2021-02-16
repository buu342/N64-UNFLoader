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
#include "device_everdrive.h"
#include "device_sc64.h"


/*********************************
        Function Pointers
*********************************/

void (*funcPointer_open)(ftdi_context_t*);
void (*funcPointer_sendrom)(ftdi_context_t*, FILE *file, u32 size);
void (*funcPointer_senddata)(ftdi_context_t*, int datatype, char *data, u32 size);
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
    ftdi_context_t *cart = &local_usb;

    // Initialize FTD
    if (automode == CART_NONE)
        pdprint("Attempting flashcart autodetection.\n", CRDEF_PROGRAM);
    testcommand(FT_CreateDeviceInfoList(&cart->devices), "USB Device not ready.");

    // Check if the device exists
    if (cart->devices == 0)
        terminate("No devices found.");

    // Allocate storage and get device info list
    cart->dev_info = (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*cart->devices);
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
        if ((automode == CART_NONE || automode == CART_64DRIVE2) && device_test_64drive2(cart, i))
        {
            device_set_64drive2(cart, i);
            if (automode == CART_NONE) 
                pdprint_replace("64Drive HW2 autodetected!\n", CRDEF_PROGRAM);
            break;
        }

        // Look for an EverDrive
        if ((automode == CART_NONE || automode == CART_EVERDRIVE) && device_test_everdrive(cart, i))
        {
            device_set_everdrive(cart, i);
            if (automode == CART_NONE)
                pdprint_replace("EverDrive autodetected!\n", CRDEF_PROGRAM);
            break;
        }

        // Look for SummerCart64
        if ((automode == CART_NONE || automode == CART_SC64) && device_test_sc64(cart, i))
        {
            device_set_sc64(cart, i);
            if (automode == CART_NONE)
                pdprint_replace("SummerCart64 autodetected!\n", CRDEF_PROGRAM);
            break;
        }
    }

    // Finish
    free(cart->dev_info);
    if (cart->carttype == CART_NONE)
    {
        if (automode == CART_NONE)
            terminate("No flashcart detected.");
        else
            terminate("Requested flashcart not found.");
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
    funcPointer_senddata = &device_senddata_64drive;
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
    device_set_everdrive
    Marks the cart as being EverDrive
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_everdrive(ftdi_context_t* cart, int index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_EVERDRIVE;

    // Set function pointers
    funcPointer_open = &device_open_everdrive;
    funcPointer_sendrom = &device_sendrom_everdrive;
    funcPointer_senddata = &device_senddata_everdrive;
    funcPointer_close = &device_close_everdrive;
}


/*==============================
    device_set_sc64
    Marks the cart as being SummerCart64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

void device_set_sc64(ftdi_context_t* cart, int index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_SC64;

    // Set function pointers
    funcPointer_open = &device_open_sc64;
    funcPointer_sendrom = &device_sendrom_sc64;
    funcPointer_senddata = &device_senddata_sc64;
    funcPointer_close = &device_close_sc64;
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
    Opens the ROM and calls the function to send it to the flashcart
    @param A string with the path to the ROM
==============================*/

void device_sendrom(char* rompath)
{
    bool escignore = false;
    bool resend = false;
    FILE *file;
    int  filesize = 0; // I could use finfo, but it doesn't work in WinXP (more info below)
    time_t lastmod = 0;
    struct stat finfo;

    for ( ; ; )
    {
        unsigned char rom_header[4];

        // Open the ROM and get info about it 
        file = fopen(rompath, "rb");
        if (file == NULL)
        {
            device_close();
            terminate("Unable to open file '%s'.\n", rompath);
        }
	    stat(rompath, &finfo);
        global_filename = rompath;

        // Read the ROM header to check if its byteswapped
        fread(rom_header, 4, 1, file);
        if (!(rom_header[0] == 0x80 && rom_header[1] == 0x37 && rom_header[2] == 0x12 && rom_header[3] == 0x40))
            global_z64 = true;

        // Get the filesize and reset the position
        // Workaround for https://stackoverflow.com/questions/32452777/visual-c-2015-express-stat-not-working-on-windows-xp
        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // If the file was not modified
        if (!resend && lastmod != 0 && lastmod == finfo.st_mtime)
        {
            int count = 0;

            // Close the file and wait for three seconds
            fclose(file);
            while (count < 30)
            {
                int ch;
                #ifndef LINUX
                    Sleep(100);
                #else
                    usleep(100);
                #endif
                count++;
                ch = getch();

                // Disable ESC ignore if the key was released
                if (ch == CH_ESCAPE && escignore)
                    escignore = false;

                // Check if ESC was pressed
                if (ch == CH_ESCAPE && !escignore)
                {
                    pdprint("\nExiting listen mode.\n", CRDEF_PROGRAM);
                    return;
                }

                // Check if R was pressed
                if (ch == 'r')
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
        if (filesize < 1052672)
            pdprint("ROM is smaller than 1MB, it might not boot properly.\n", CRDEF_PROGRAM);

        // Send the ROM
        funcPointer_sendrom(&local_usb, file, filesize);

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
    device_senddata
    Sends data to the flashcart via USB
    @param The data to send
    @param The number of bytes in the data
    @returns 1 if success, 0 if failure
==============================*/

void device_senddata(int datatype, char* data, u32 size)
{
    funcPointer_senddata(&local_usb, datatype, data, size);
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

