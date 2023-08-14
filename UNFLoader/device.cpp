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
#ifndef LINUX
    #include <shlwapi.h>
#endif
#include <atomic>
#ifdef _WIN64
#pragma comment(lib, "Include/FTD2XX_x64.lib")
#else
#pragma comment(lib, "Include/FTD2XX.lib")
#endif


/*********************************
        Function Pointers
*********************************/

DeviceError (*funcPointer_open)(CartDevice*);
DeviceError (*funcPointer_sendrom)(CartDevice*, byte* rom, uint32_t size);
DeviceError (*funcPointer_testdebug)(CartDevice*);
uint32_t    (*funcPointer_rompadding)(uint32_t romsize);
bool        (*funcPointer_explicitcic)(byte* bootcode);
uint32_t    (*funcPointer_maxromsize)();
DeviceError (*funcPointer_senddata)(CartDevice*, USBDataType datatype, byte* data, uint32_t size);
DeviceError (*funcPointer_receivedata)(CartDevice*, uint32_t* dataheader, byte** buff);
DeviceError (*funcPointer_close)(CartDevice*);


/*********************************
        Function Prototypes
*********************************/

static void device_set_64drive1(CartDevice* cart);
static void device_set_64drive2(CartDevice* cart);
static void device_set_everdrive(CartDevice* cart);
static void device_set_sc64(CartDevice* cart);


/*********************************
             Globals
*********************************/

// File
static char* local_rompath  = NULL;

// Cart
static CartDevice local_cart;

// Upload
std::atomic<bool> local_uploadcancelled (false);
std::atomic<float> local_uploadprogress (0.0f);


/*==============================
    device_initialize
    Initializes the device data structures
==============================*/

void device_initialize()
{
    memset(&local_cart, 0, sizeof(CartDevice));
    local_cart.carttype = CART_NONE;
    local_cart.cictype  = CIC_NONE;
    local_cart.savetype = SAVE_NONE;
    local_cart.protocol = PROTOCOL_VERSION1;
}


/*==============================
    device_find
    Finds the flashcart plugged in to USB
    @return The DeviceError enum
==============================*/

DeviceError device_find()
{
    // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
    if ((local_cart.carttype == CART_NONE || local_cart.carttype == CART_64DRIVE1))
    {
        DeviceError err = device_test_64drive1(&local_cart);
        if (err == DEVICEERR_OK)
            device_set_64drive1(&local_cart);
        else if (err != DEVICEERR_NOTCART)
            return err;
    }

    // Look for 64drive HW2 (FT2232H Asynchronous FIFO mode)
    if ((local_cart.carttype == CART_NONE || local_cart.carttype == CART_64DRIVE2))
    {
        DeviceError err = device_test_64drive2(&local_cart);
        if (err == DEVICEERR_OK)
            device_set_64drive2(&local_cart);
        else if (err != DEVICEERR_NOTCART)
            return err;
    }

    // Look for an EverDrive
    if ((local_cart.carttype == CART_NONE || local_cart.carttype == CART_EVERDRIVE))
    {
        DeviceError err = device_test_everdrive(&local_cart);
        if (err == DEVICEERR_OK)
            device_set_everdrive(&local_cart);
        else if (err != DEVICEERR_NOTCART)
            return err;
    }

    // Look for SC64
    if ((local_cart.carttype == CART_NONE || local_cart.carttype == CART_SC64))
    {
        DeviceError err = device_test_sc64(&local_cart);
        if (err == DEVICEERR_OK)
            device_set_sc64(&local_cart);
        else if (err != DEVICEERR_NOTCART)
            return err;
    }

    // Finish
    if (local_cart.carttype == CART_NONE)
        return DEVICEERR_CARTFINDFAIL;
    return DEVICEERR_OK;
}


/*==============================
    device_set_64drive1
    Marks the cart as being 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_64drive1(CartDevice* cart)
{
    // Set cart settings
    cart->carttype = CART_64DRIVE1;

    // Set function pointers
    funcPointer_open = &device_open_64drive;
    funcPointer_maxromsize = &device_maxromsize_64drive;
    funcPointer_rompadding = &device_rompadding_64drive;
    funcPointer_explicitcic = &device_explicitcic_64drive1;
    funcPointer_sendrom = &device_sendrom_64drive;
    funcPointer_testdebug = &device_testdebug_64drive;
    funcPointer_senddata = &device_senddata_64drive;
    funcPointer_receivedata = &device_receivedata_64drive;
    funcPointer_close = &device_close_64drive;
}


/*==============================
    device_set_64drive2
    Marks the cart as being 64Drive HW2
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_64drive2(CartDevice* cart)
{
    // Do exactly the same as device_set_64drive1
    device_set_64drive1(cart);

    // Now the 64Drive specific changes
    funcPointer_explicitcic = &device_explicitcic_64drive2;
    cart->carttype = CART_64DRIVE2;
}


/*==============================
    device_set_everdrive
    Marks the cart as being EverDrive
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_everdrive(CartDevice* cart)
{
    // Set cart settings
    cart->carttype = CART_EVERDRIVE;

    // Set function pointers
    funcPointer_open = &device_open_everdrive;
    funcPointer_maxromsize = &device_maxromsize_everdrive;
    funcPointer_rompadding = &device_rompadding_everdrive;
    funcPointer_explicitcic = &device_explicitcic_everdrive;
    funcPointer_sendrom = &device_sendrom_everdrive;
    funcPointer_testdebug = &device_testdebug_everdrive;
    funcPointer_senddata = &device_senddata_everdrive;
    funcPointer_receivedata = &device_receivedata_everdrive;
    funcPointer_close = &device_close_everdrive;
}


/*==============================
    device_set_sc64
    Marks the cart as being SC64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

static void device_set_sc64(CartDevice* cart)
{
    // Set cart settings
    cart->carttype = CART_SC64;

    // Set function pointers
    funcPointer_open = &device_open_sc64;
    funcPointer_maxromsize = &device_maxromsize_sc64;
    funcPointer_rompadding = &device_rompadding_sc64;
    funcPointer_explicitcic = &device_explicitcic_sc64;
    funcPointer_sendrom = &device_sendrom_sc64;
    funcPointer_testdebug = &device_testdebug_sc64;
    funcPointer_senddata = &device_senddata_sc64;
    funcPointer_receivedata = &device_receivedata_sc64;
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
    return (local_cart.structure != NULL);
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
    device_rompadding
    Calculates the correct ROM size for uploading
    @param  The current ROM size
    @return The correct ROM size for uploading.
==============================*/

uint32_t device_rompadding(uint32_t romsize)
{
    return funcPointer_rompadding(romsize);
}


/*==============================
    device_explicitcic
    Checks whether the flashcart requires
    that the CIC be set explicitly, and sets
    it if so.
    @param Whether the CIC was changed.
==============================*/

bool device_explicitcic()
{
    CICType oldcic = local_cart.cictype;
    FILE* fp = fopen(local_rompath, "rb");
    byte* bootcode = (byte*) malloc(4032);

    // Check fopen/malloc worked
    if (fp == NULL || bootcode == NULL)
        return false;

    // Read the bootcode
    fseek(fp, 0x40, SEEK_SET);
    if (fread(bootcode, 1, 4032, fp) != 4032)
        return false;
    fseek(fp, 0, SEEK_SET);

    // Check the CIC
    funcPointer_explicitcic(bootcode);
    free(bootcode);
    fclose(fp);
    return oldcic != local_cart.cictype;
}

/*==============================
    device_sendrom
    Opens the ROM and calls the function to send it to the flashcart
    @param The ROM FILE pointer
    @param The size of the ROM in bytes
==============================*/

DeviceError device_sendrom(FILE* rom, uint32_t filesize)
{
    bool is_v64 = false;
    uint32_t originalsize = filesize;
    byte* rom_buffer;
    DeviceError err;
    
    // Initialize upload checker globals
    local_uploadcancelled = false;
    local_uploadprogress = 0.0f;

    // Pad the ROM if necessary
    filesize = funcPointer_rompadding(filesize);

    // Check we managed to malloc
    rom_buffer = (byte*) malloc(sizeof(byte)*filesize);
    if (rom_buffer == NULL)
        return DEVICEERR_MALLOCFAIL;

    // Read the ROM into a buffer
    fseek(rom, 0, SEEK_SET);
    if (fread(rom_buffer, 1, originalsize, rom) != originalsize)
        return DEVICEERR_FILEREADFAIL;
    fseek(rom, 0, SEEK_SET);

    // Check if we have a V64 ROM
    if (!(rom_buffer[0] == 0x80 && rom_buffer[1] == 0x37 && rom_buffer[2] == 0x12 && rom_buffer[3] == 0x40))
        is_v64 = true;

    // Byteswap if it's a V64 ROM
    if (is_v64)
        for (uint32_t i=0; i<filesize; i+=2)
            SWAP(rom_buffer[i], rom_buffer[i+1]);

    // Upload the ROM
    err = funcPointer_sendrom(&local_cart, rom_buffer, filesize);
    free(rom_buffer);
    if (err != DEVICEERR_OK)
        device_cancelupload();
    return err;
}


/*==============================
    device_testdebug
    Checks whether this cart can use debug mode
    @return The device error, or OK
==============================*/

DeviceError device_testdebug()
{
    return funcPointer_testdebug(&local_cart);
}


/*==============================
    device_senddata
    Sends data to the connected flashcart
    @param  The datatype that is being sent
    @param  A buffer containing said data
    @param  The size of the data
    @return The device error, or OK
==============================*/

DeviceError device_senddata(USBDataType datatype, byte* data, uint32_t size)
{
    return funcPointer_senddata(&local_cart, datatype, data, size);
}


/*==============================
    device_receivedata
    Receives data from the connected flashcart
    @param  A pointer to an 32-bit value where
            the received data header will be
            stored.
    @param  A pointer to a byte buffer pointer
            where the data will be malloc'ed into.
    @return The device error, or OK
==============================*/

DeviceError device_receivedata(uint32_t* dataheader, byte** buff)
{
    return funcPointer_receivedata(&local_cart, dataheader, buff);
}


/*==============================
    device_close
    Calls the function to close the flashcart
    @param The device error, or OK
==============================*/

DeviceError device_close()
{
    DeviceError err;

    // Should never happen, but just in case...
    if (local_cart.structure == NULL)
        return DEVICEERR_OK;

    // Close the device
    err = funcPointer_close(&local_cart);
    local_cart.structure = NULL;
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
    #ifndef LINUX
        if (PathIsDirectoryA(path))
            return false;
    #else
        struct stat path_stat;
        stat(path, &path_stat);
        if (!S_ISREG(path_stat.st_mode))
            return false;
    #endif
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
    local_cart.carttype = cart;
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
    return local_cart.carttype;
}


/*==============================
    device_getcic
    Gets the current CIC
    @param The configured CIC
==============================*/

CICType device_getcic()
{
    return local_cart.cictype;
}


/*==============================
    device_getsave
    Gets the current save type
    @param The configured save type
==============================*/

SaveType device_getsave()
{
    return local_cart.savetype;
}


/*==============================
    device_cancelupload
    Enables the cancel upload flag
==============================*/

void device_cancelupload()
{
    local_uploadcancelled = true;
}


/*==============================
    device_uploadcancelled
    Checks whether the cancel upload flag
    has been set
    @return Whether the upload was cancelled
==============================*/

bool device_uploadcancelled()
{
    return local_uploadcancelled.load();
}


/*==============================
    device_setuploadprogress
    Sets the upload progress
    @param The upload progress: a value
           from 0 to 100.
==============================*/

void device_setuploadprogress(float progress)
{
    local_uploadprogress = progress;
}


/*==============================
    device_getuploadprogress
    Returns the current upload progress
    @return The upload progress from 0
            to 100.
==============================*/

float device_getuploadprogress()
{
    return local_uploadprogress.load();
}


/*==============================
    device_setprotocol
    Sets the communication protocol version
    @param The protocol version
==============================*/

void device_setprotocol(ProtocolVer version)
{
    local_cart.protocol = version;
}


/*==============================
    device_getprotocol
    Gets the communication protocol version
    @return The protocol version
==============================*/

ProtocolVer device_getprotocol()
{
    return local_cart.protocol;
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

uint32_t romhash(byte *buff, uint32_t len) 
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