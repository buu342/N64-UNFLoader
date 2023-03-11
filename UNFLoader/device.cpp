/***************************************************************
                           device.cpp

Passes flashcart communication to more specific functions
***************************************************************/

#include "device.h"
#include "device_64drive.h"
#include "device_everdrive.h"
#include "device_sc64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "Include/FTD2XX.lib")


/*********************************
        Function Pointers
*********************************/

void (*funcPointer_open)(FTDIDevice*);
void (*funcPointer_sendrom)(FTDIDevice*, FILE *file, uint32_t size);
bool (*funcPointer_testdebug)(FTDIDevice*);
void (*funcPointer_senddata)(FTDIDevice*, int datatype, char *data, uint32_t size);
void (*funcPointer_close)(FTDIDevice*);

static void device_set_64drive1(FTDIDevice* cart, uint32_t index);
static void device_set_64drive2(FTDIDevice* cart, uint32_t index);
static void device_set_everdrive(FTDIDevice* cart, uint32_t index);
static void device_set_sc64(FTDIDevice* cart, uint32_t index);


/*********************************
             Globals
*********************************/

static char*    local_rompath  = NULL;
static bool     local_isz64    = false;
static CartType local_carttype = CART_NONE;
static CICType  local_cictype  = CIC_NONE;
static SaveType local_savetype = SAVE_NONE;

static FTDIDevice local_cart;


/*==============================
    device_find
    Finds the flashcart plugged in to USB
    @return The DeviceError enum
==============================*/

DeviceError device_find()
{
    FTDIDevice *cart = &local_cart;
    memset(&local_cart, 0, sizeof(FTDIDevice));

    // Initialize FTD
    if (FT_CreateDeviceInfoList((LPDWORD)(&cart->device_count)) != FT_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (cart->device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    cart->device_info = (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*cart->device_count);
    FT_GetDeviceInfoList(cart->device_info, (LPDWORD)(&cart->device_count));

    // Search the devices
    for (uint32_t i=0; i<cart->device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if ((local_carttype == CART_NONE || local_carttype == CART_64DRIVE1) && device_test_64drive1(cart, i))
        {
            device_set_64drive1(cart, i);
            break;
        }

        // Look for 64drive HW2 (FT232H Synchronous FIFO mode)
        if ((local_carttype == CART_NONE || local_carttype == CART_64DRIVE2) && device_test_64drive2(cart, i))
        {
            device_set_64drive2(cart, i);
            break;
        }

        // Look for an EverDrive
        if ((local_carttype == CART_NONE || local_carttype == CART_EVERDRIVE))
        {
            DeviceError deverr = device_test_everdrive(cart, i);
            if (deverr == DEVICEERR_OK)
                device_set_everdrive(cart, i);
            else if (deverr != DEVICEERR_NOTCART)
                return deverr;
            break;
        }

        // Look for SC64
        if ((local_carttype == CART_NONE || local_carttype == CART_SC64) && device_test_sc64(cart, i))
        {
            device_set_sc64(cart, i);
            break;
        }
    }

    // Finish
    free(cart->device_info);
    if (cart->carttype == CART_NONE)
        return DEVICEERR_CARTFINDFAIL;
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
    cart->synchronous = 0;
    cart->carttype = CART_64DRIVE1;

    // Set function pointers
    /*
    funcPointer_open = &device_open_64drive;
    funcPointer_sendrom = &device_sendrom_64drive;
    funcPointer_testdebug = &device_testdebug_64drive;
    funcPointer_senddata = &device_senddata_64drive;
    funcPointer_close = &device_close_64drive;
    */
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
    cart->synchronous = 1;
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
    /*
    funcPointer_open = &device_open_everdrive;
    funcPointer_sendrom = &device_sendrom_everdrive;
    funcPointer_testdebug = &device_testdebug_everdrive;
    funcPointer_senddata = &device_senddata_everdrive;
    funcPointer_close = &device_close_everdrive;
    */
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
    /*
    funcPointer_open = &device_open_sc64;
    funcPointer_sendrom = &device_sendrom_sc64;
    funcPointer_testdebug = &device_testdebug_sc64;
    funcPointer_senddata = &device_senddata_sc64;
    funcPointer_close = &device_close_sc64;
    */
}



/*==============================
    device_setrom
    Sets the path of the ROM to load
    @param The path to the ROM
==============================*/

void device_setrom(char* path)
{
    local_rompath = path;
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
    local_cictype = cic;
}


/*==============================
    device_setsave
    Forces a save type
    @param The savetype to set
==============================*/

void device_setsave(SaveType save)
{
    local_savetype = save;
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