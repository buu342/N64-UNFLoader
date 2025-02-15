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

bool device_setrom(const char* path)
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
    local_rompath = (char*)path;
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
    ipl2checksum
    Compute the IPL2 checksum of a bootcode
    @param  The bootcode
    @return The 48-bit checksum
==============================*/

uint64_t ipl2checksum(uint8_t seed, byte *rom)
{
    auto rotl = [](uint32_t value, uint32_t shift) -> uint32_t 
    {
        return (value << shift) | (value >> (-((int32_t)shift)&31));
    };
    auto rotr = [](uint32_t value, uint32_t shift) -> uint32_t
    {
        return (value >> shift) | (value << (-((int32_t)shift)&31));
    };

    auto csum = [](uint32_t a0, uint32_t a1, uint32_t a2) -> uint32_t
    {
        if (a1 == 0) 
            a1 = a2;
        uint64_t prod = (uint64_t)a0 * (uint64_t)a1;
        uint32_t hi = (uint32_t)(prod >> 32);
        uint32_t lo = (uint32_t)prod;
        uint32_t diff = hi - lo;
        return diff ? diff : a0;
    };

    // Create the initialization data
    uint32_t init = 0x6c078965 * (seed & 0xff) + 1;
    uint32_t data = (rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3];
    rom += 4;
    init ^= data;

    // Copy to the state
    uint32_t state[16];
    for(auto &s : state) 
        s = init;

    uint32_t dataNext = data, dataLast;
    uint32_t loop = 0;
    while(1)
    {
        loop++;
        dataLast = data;
        data = dataNext;

        state[0] += csum(1007 - loop, data, loop);
        state[1]  = csum(state[1], data, loop);
        state[2] ^= data;
        state[3] += csum(data + 5, 0x6c078965, loop);
        state[9]  = (dataLast < data) ? csum(state[9], data, loop) : state[9] + data;
        state[4] += rotr(data, dataLast & 0x1f);
        state[7]  = csum(state[7], rotl(data, dataLast & 0x1f), loop);
        state[6]  = (data < state[6]) ? (state[3] + state[6]) ^ (data + loop) : (state[4] + data) ^ state[6];
        state[5] += rotl(data, dataLast >> 27);
        state[8]  = csum(state[8], rotr(data, dataLast >> 27), loop);

        if (loop == 1008)
            break;

        dataNext   = (rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3];
        rom += 4;
        state[15]  = csum(csum(state[15], rotl(data, dataLast  >> 27), loop), rotl(dataNext, data  >> 27), loop);
        state[14]  = csum(csum(state[14], rotr(data, dataLast & 0x1f), loop), rotr(dataNext, data & 0x1f), loop);
        state[13] += rotr(data, data & 0x1f) + rotr(dataNext, dataNext & 0x1f);
        state[10]  = csum(state[10] + data, dataNext, loop);
        state[11]  = csum(state[11] ^ data, dataNext, loop);
        state[12] += state[8] ^ data;
    }

    uint32_t buf[4];
    for(auto &b : buf) 
        b = state[0];

    for(loop = 0; loop < 16; loop++)
    {
        data = state[loop];
        uint32_t tmp = buf[0] + rotr(data, data & 0x1f);
        buf[0] = tmp;
        buf[1] = data < tmp ? buf[1]+data : csum(buf[1], data, loop);

        tmp = (data & 0x02) >> 1;
        uint32_t tmp2 = data & 0x01;
        buf[2] = tmp == tmp2 ? buf[2]+data : csum(buf[2], data, loop);
        buf[3] = tmp2 == 1 ? buf[3]^data : csum(buf[3], data, loop);
    }

    uint64_t checksum = (uint64_t)csum(buf[0], buf[1], 16) << 32;
    checksum |= buf[3] ^ buf[2];
    return checksum & 0xffffffffffffull;
}


/*==============================
    cic_from_bootcode
    Returns a CIC value from the bootcode
    @param  The bootcode
    @return The global_cictype value
==============================*/

CICType cic_from_bootcode(byte *bootcode)
{
    switch (ipl2checksum(0x3F, bootcode))
    {
        case 0x45cc73ee317aull: return CIC_6101;
        case 0x44160ec5d9afull: return CIC_7102;
        case 0xa536c0f1d859ull: return CIC_6102;
    }
    switch (ipl2checksum(0x78, bootcode))
    {
        case 0x586fd4709867ull: return CIC_X103;
    }
    switch (ipl2checksum(0x91, bootcode))
    {
        case 0x8618a45bc2d3ull: return CIC_X105;
    }
    switch (ipl2checksum(0x85, bootcode))
    {
        case 0x2bbad4e6eb74ull: return CIC_X106;
    }
    switch (ipl2checksum(0xdd, bootcode))
    {
        case 0x32b294e2ab90ull: return CIC_8303;
        // case 0x6ee8d9e84970ull: return CIC_8401;
        // case 0x083c6c77e0b1ull: return CIC_5167;
        // case 0x05ba2ef0a5f1ull: return CIC_DDUS;
    }
    return CIC_NONE;
}
