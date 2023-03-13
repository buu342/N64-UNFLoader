/***************************************************************
                           device.cpp

Passes flashcart communication to more specific functions
***************************************************************/

#include "device.h"
#include "device_64drive.h"
#include "device_everdrive.h"
#include "device_sc64.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#pragma comment(lib, "Include/FTD2XX.lib")


/*********************************
        Function Pointers
*********************************/

DeviceError (*funcPointer_open)(FTDIDevice*);
DeviceError (*funcPointer_sendrom)(FTDIDevice*, uint8_t* rom, uint32_t size);
bool        (*funcPointer_testdebug)(FTDIDevice*);
bool        (*funcPointer_shouldpadrom)();
uint32_t    (*funcPointer_maxromsize)();
DeviceError (*funcPointer_senddata)(FTDIDevice*, int datatype, char *data, uint32_t size);
DeviceError (*funcPointer_close)(FTDIDevice*);

static void device_set_64drive1(FTDIDevice* cart, uint32_t index);
static void device_set_64drive2(FTDIDevice* cart, uint32_t index);
static void device_set_everdrive(FTDIDevice* cart, uint32_t index);
static void device_set_sc64(FTDIDevice* cart, uint32_t index);


/*********************************
             Globals
*********************************/

// File
static char*    local_rompath  = NULL;

// Cart
static CartType local_carttype = CART_NONE;
static FTDIDevice local_cart;


/*==============================
    device_find
    Finds the flashcart plugged in to USB
    @return The DeviceError enum
==============================*/

DeviceError device_find()
{
    memset(&local_cart, 0, sizeof(FTDIDevice));

    // Initialize FTD
    if (FT_CreateDeviceInfoList(&local_cart.device_count) != FT_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (local_cart.device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    local_cart.device_info = (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*local_cart.device_count);
    FT_GetDeviceInfoList(local_cart.device_info, &local_cart.device_count);

    // Search the devices
    for (uint32_t i=0; i<local_cart.device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if ((local_carttype == CART_NONE || local_carttype == CART_64DRIVE1) && device_test_64drive1(&local_cart, i))
        {
            device_set_64drive1(&local_cart, i);
            break;
        }

        // Look for 64drive HW2 (FT232H Synchronous FIFO mode)
        if ((local_carttype == CART_NONE || local_carttype == CART_64DRIVE2) && device_test_64drive2(&local_cart, i))
        {
            device_set_64drive2(&local_cart, i);
            break;
        }

        // Look for an EverDrive
        if ((local_carttype == CART_NONE || local_carttype == CART_EVERDRIVE))
        {
            DeviceError deverr = device_test_everdrive(&local_cart, i);
            if (deverr == DEVICEERR_OK)
                device_set_everdrive(&local_cart, i);
            else if (deverr != DEVICEERR_NOTCART)
                return deverr;
            break;
        }

        // Look for SC64
        if ((local_carttype == CART_NONE || local_carttype == CART_SC64) && device_test_sc64(&local_cart, i))
        {
            device_set_sc64(&local_cart, i);
            break;
        }
    }

    // Finish
    free(local_cart.device_info);
    if (local_cart.carttype == CART_NONE)
        return DEVICEERR_CARTFINDFAIL;
    local_carttype = local_cart.carttype;
    return DEVICEERR_OK;
}


/*==============================
    device_set_64drive1
    Marks the cart as being 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_64drive1(FTDIDevice* cart, uint32_t index)
{
    // Set cart settings
    cart->device_index = index;
    cart->synchronous = false;
    cart->carttype = CART_64DRIVE1;

    // Set function pointers
    funcPointer_open = &device_open_64drive;
    funcPointer_maxromsize = &device_maxromsize_64drive;
    funcPointer_sendrom = &device_sendrom_64drive;
    funcPointer_shouldpadrom = &device_shouldpadrom_64drive;
    /*
    funcPointer_testdebug = &device_testdebug_64drive;
    funcPointer_senddata = &device_senddata_64drive;
    */
    funcPointer_close = &device_close_64drive;
}


/*==============================
    device_set_64drive2
    Marks the cart as being 64Drive HW2
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_64drive2(FTDIDevice* cart, uint32_t index)
{
    // Do exactly the same as device_set_64drive1
    device_set_64drive1(cart, index);

    // But modify the important cart settings
    cart->synchronous = true;
    cart->carttype = CART_64DRIVE2;
}


/*==============================
    device_set_everdrive
    Marks the cart as being EverDrive
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_everdrive(FTDIDevice* cart, uint32_t index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_EVERDRIVE;

    // Set function pointers
    funcPointer_open = &device_open_everdrive;
    funcPointer_maxromsize = &device_maxromsize_everdrive;
    funcPointer_shouldpadrom = &device_shouldpadrom_everdrive;
    /*
    funcPointer_sendrom = &device_sendrom_everdrive;
    funcPointer_testdebug = &device_testdebug_everdrive;
    funcPointer_senddata = &device_senddata_everdrive;
    */
    funcPointer_close = &device_close_everdrive;
}


/*==============================
    device_set_sc64
    Marks the cart as being SC64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_sc64(FTDIDevice* cart, uint32_t index)
{
    // Set cart settings
    cart->device_index = index;
    cart->carttype = CART_SC64;

    // Set function pointers
    funcPointer_open = &device_open_sc64;
    funcPointer_maxromsize = &device_maxromsize_sc64;
    funcPointer_shouldpadrom = &device_shouldpadrom_sc64;
    /*
    funcPointer_sendrom = &device_sendrom_sc64;
    funcPointer_testdebug = &device_testdebug_sc64;
    funcPointer_senddata = &device_senddata_sc64;
    */
    funcPointer_close = &device_close_sc64;
}


/*==============================
    device_open
    Calls the function to open the flashcart
    @param The device error, or OK
==============================*/

DeviceError device_open()
{
    return funcPointer_open(&local_cart);
}


/*==============================
    device_isopen
    Checks if the device is open
    @returns Whether the device is open
==============================*/

bool device_isopen()
{
    return (local_cart.handle != NULL);
}


/*==============================
    device_maxromsize
    Gets the max ROM size that 
    the flashcart supports
    @return The max ROM size
==============================*/

uint32_t device_getmaxromsize()
{
    return funcPointer_maxromsize();
}


/*==============================
    device_sendrom
    Opens the ROM and calls the function to send it to the flashcart
    @param The ROM FILE pointer
    @param The size of the ROM in bytes
==============================*/

DeviceError device_sendrom(FILE* rom, uint32_t filesize)
{
    bool is_z64 = false;
    uint8_t* rom_buffer;
    DeviceError err;

    // Pad the ROM if necessary
    if (funcPointer_shouldpadrom())
        filesize = calc_padsize(filesize/(1024*1024))*1024*1024;

    // Check we managed to malloc
    rom_buffer = (uint8_t*) malloc(sizeof(uint8_t)*filesize);
    if (rom_buffer == NULL)
        return DEVICEERR_MALLOCFAIL;

    // Read the ROM into a buffer
    fread(rom_buffer, 1, filesize, rom);

    // Check if we have a Z64 ROM
    if (!(rom_buffer[0] == 0x80 && rom_buffer[1] == 0x37 && rom_buffer[2] == 0x12 && rom_buffer[3] == 0x40))
        is_z64 = true;
    fseek(rom, 0, SEEK_SET);

    // Byteswap if it's a Z64 ROM
    if (is_z64)
        for (uint32_t i=0; i<filesize; i+=2)
            SWAP(rom_buffer[i], rom_buffer[i+1]);

    // Upload the ROM
    err = funcPointer_sendrom(&local_cart, rom_buffer, filesize);
    free(rom_buffer);
    return err;
}


/*==============================
    device_testdebug
    Checks whether this cart can use debug mode
    @param A pointer to the cart context
    @returns true if this cart can use debug mode, false otherwise
==============================*/
/*
bool device_testdebug()
{
    return funcPointer_testdebug(&local_cart);
}
*/

/*==============================
    device_close
    Calls the function to close the flashcart
    @param The device error, or OK
==============================*/

DeviceError device_close()
{
    DeviceError err;

    // Should never happen, but just in case...
    if (local_cart.handle == NULL)
        return DEVICEERR_OK;

    // Close the device
    err = funcPointer_close(&local_cart);
    local_cart.handle = NULL;
    return err;
}


/*==============================
    device_setrom
    Sets the path of the ROM to load
    @param  The path to the ROM
    @return True if successful, false otherwise
==============================*/

bool device_setrom(char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    if (!S_ISREG(path_stat.st_mode))
        return false;
    local_rompath = path;
    return true;
}


/*==============================
    device_setcart
    Forces a flashcart
    @param The cart to set to
==============================*/

void device_setcart(CartType cart)
{
    local_carttype = cart;
}


/*==============================
    device_setcic
    Forces a CIC
    @param The cic to set
==============================*/

void device_setcic(CICType cic)
{
    local_cart.cictype = cic;
}


/*==============================
    device_setsave
    Forces a save type
    @param The savetype to set
==============================*/

void device_setsave(SaveType save)
{
    local_cart.savetype = save;
}


/*==============================
    device_getrom
    Gets the path of the ROM to load
    @return A string with the file path
            of the ROM.
==============================*/

char* device_getrom()
{
    return local_rompath;
}


/*==============================
    device_getcart
    Gets the current flashcart
    @return The connected cart type
==============================*/

CartType device_getcart()
{
    return local_carttype;
}


/*==============================
    swap_endian
    Swaps the endianess of the data
    @param  The data to swap the endianess of
    @return The data with endianess swapped
==============================*/

uint32_t swap_endian(uint32_t val)
{
    return ((val << 24)) | 
           ((val << 8) & 0x00ff0000) |
           ((val >> 8) & 0x0000ff00) | 
           ((val >> 24));
}


/*==============================
    calc_padsize
    Returns the correct size a ROM should be. Code taken from:
    https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    @param  The current ROM filesize
    @return The correct ROM filesize
==============================*/

uint32_t calc_padsize(uint32_t size)
{
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
    return size;
}


/*==============================
    romhash
    Returns an int with a simple hash of the inputted data
    @param  The data to hash
    @param  The size of the data
    @return The hash number
==============================*/

uint32_t romhash(uint8_t *buff, uint32_t len) 
{
    uint32_t i;
    uint32_t hash=0;
    for (i=0; i<len; i++)
        hash += buff[i];
    return hash;
}

/*==============================
    cic_from_hash
    Returns a CIC value from the hash number
    @param  The hash number
    @return The global_cictype value
==============================*/

CICType cic_from_hash(uint32_t hash)
{
    switch (hash)
    {
        case 0x033A27: return CIC_6101;
        case 0x034044: return CIC_6102;
        case 0x03421E: return CIC_7102;
        case 0x0357D0: return CIC_X103;
        case 0x047A81: return CIC_X105;
        case 0x0371CC: return CIC_X106;
        case 0x02ABB7: return CIC_5101;
        case 0x04F90E: return CIC_8303;
    }
    return CIC_NONE;
}