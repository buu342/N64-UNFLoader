/***************************************************************
                       device_64drive.cpp

Handles 64Drive HW1 and HW2 USB communication. A lot of the code
here is courtesy of MarshallH's 64Drive USB tool:
http://64drive.retroactive.be/support.php
***************************************************************/

#include "device_64drive.h"
#include <string.h>


/*==============================
    device_test_64drive1
    Checks whether the device passed as an argument is 64Drive HW1
    @param  A pointer to the cart context
    @param  The index of the cart
    @return True if the cart is a 64Drive HW1, or false otherwise
==============================*/

bool device_test_64drive1(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "64drive USB device A") == 0 && cart->device_info[index].ID == 0x4036010);
}


/*==============================
    device_test_64drive2
    Checks whether the device passed as an argument is 64Drive HW2
    @param  A pointer to the cart context
    @param  The index of the cart
    @return True if the cart is a 64Drive HW2, or false otherwise
==============================*/

bool device_test_64drive2(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "64drive USB device") == 0 && cart->device_info[index].ID == 0x4036014);
}


/*==============================
    device_open_64drive
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_64drive(FTDIDevice* cart)
{
    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || cart->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // Reset the cart and set its timeouts
    if (FT_ResetDevice(cart->handle) != FT_OK)
        return DEVICEERR_RESETFAIL;
    if (FT_SetTimeouts(cart->handle, 5000, 5000) != FT_OK)
        return DEVICEERR_TIMEOUTSETFAIL;

    // If the cart is in synchronous mode, enable the bits
    if (cart->synchronous)
    {
        if (FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_RESET) != FT_OK)
            return DEVICEERR_BITMODEFAIL_RESET;
        if (FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_SYNC_FIFO) != FT_OK)
            return DEVICEERR_BITMODEFAIL_SYNCFIFO;
    }

    // Purge USB contents
    if (FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
        return DEVICEERR_PURGEFAIL;

    // Ok
    return DEVICEERR_OK;
}


/*==============================
    device_close_64drive
    Closes the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_64drive(FTDIDevice* cart)
{
    if (FT_Close(cart->handle) != FT_OK)
        return DEVICEERR_CLOSEFAIL;
    return DEVICEERR_OK;
}
