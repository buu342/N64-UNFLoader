/***************************************************************
                      device_everdrive.cpp

Handles EverDrive USB communication. A lot of the code here
is courtesy of KRIKzz's USB tool:
http://krikzz.com/pub/support/everdrive-64/x-series/dev/
***************************************************************/

#include "device_everdrive.h"
#include <string.h>


/*==============================
    device_test_everdrive
    Checks whether the device passed as an argument is EverDrive
    @param A pointer to the cart context
    @param The index of the cart
    @returns The device error during the testing process
==============================*/

DeviceError device_test_everdrive(FTDIDevice* cart, uint32_t index)
{
    if (strcmp(cart->device_info[index].Description, "FT245R USB FIFO") == 0 && cart->device_info[index].ID == 0x04036001)
    {
        char send_buff[16];
        char recv_buff[16];
        memset(send_buff, 0, 16);
        memset(recv_buff, 0, 16);

        // Define the command to send
        send_buff[0] = 'c';
        send_buff[1] = 'm';
        send_buff[2] = 'd';
        send_buff[3] = 't';

        // Open the device
        cart->status = FT_Open(index, &cart->handle);
        if (cart->status != FT_OK || !cart->handle)
            return DEVICEERR_CANTOPEN;

        // Initialize the USB
        if (FT_ResetDevice(cart->handle) != FT_OK)
            return DEVICEERR_RESETFAIL;
        if (FT_SetTimeouts(cart->handle, 500, 500) != FT_OK)
            return DEVICEERR_TIMEOUTSETFAIL;
        if (FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
            return DEVICEERR_PURGEFAIL;

        // Send the test command
        if (FT_Write(cart->handle, send_buff, 16, &cart->bytes_written) != FT_OK)
            return DEVICEERR_WRITEFAIL;
        if (FT_Read(cart->handle, recv_buff, 16, &cart->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        if (FT_Close(cart->handle) != FT_OK)
            return DEVICEERR_CLOSEFAIL;
        cart->handle = 0;

        // Check if the EverDrive responded correctly
        if (recv_buff[3] == 'r')
            return DEVICEERR_OK;
    }
    return DEVICEERR_NOTCART;
}