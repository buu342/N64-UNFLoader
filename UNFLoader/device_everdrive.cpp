/***************************************************************
                      device_everdrive.cpp

Handles EverDrive USB communication. A lot of the code here
is courtesy of KRIKzz's USB tool:
http://krikzz.com/pub/support/everdrive-64/x-series/dev/
***************************************************************/

#include "device_everdrive.h"
#include "Include/ftd2xx.h"
#include <string.h>

typedef struct 
{
    uint32_t  device_index;
    FT_HANDLE handle;
    DWORD     bytes_written;
    DWORD     bytes_read;
} ED64Handle;


/*==============================
    device_test_everdrive
    Checks whether the device passed as an argument is EverDrive
    @param  A pointer to the cart context
    @param  The index of the cart
    @return DEVICEERR_OK if the cart is an Everdive, 
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_everdrive(CartDevice* cart)
{
    DWORD device_count;
    FT_DEVICE_LIST_INFO_NODE* device_info;

    // Initialize FTD
    if (FT_CreateDeviceInfoList(&device_count) != FT_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    device_info = (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*device_count);
    FT_GetDeviceInfoList(device_info, &device_count);

    // Search the devices
    for (uint32_t i=0; i<device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if (strcmp(device_info[i].Description, "FT245R USB FIFO") == 0 && device_info[i].ID == 0x04036001)
        {
            FT_HANDLE temphandle;
            DWORD bytes_written;
            DWORD bytes_read;

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
            if (FT_Open(i, &temphandle) != FT_OK || !temphandle)
            {
                free(device_info);
                return DEVICEERR_CANTOPEN;
            }

            // Initialize the USB
            if (FT_ResetDevice(temphandle) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_RESETFAIL;
            }
            if (FT_SetTimeouts(temphandle, 500, 500) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_TIMEOUTSETFAIL;
            }
            if (FT_Purge(temphandle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_PURGEFAIL;
            }

            // Send the test command
            if (FT_Write(temphandle, send_buff, 16, &bytes_written) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_WRITEFAIL;
            }
            if (FT_Read(temphandle, recv_buff, 16, &bytes_read) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_READFAIL;
            }
            if (FT_Close(temphandle) != FT_OK)
            {
                free(device_info);
                return DEVICEERR_CLOSEFAIL;
            }

            // Check if the EverDrive responded correctly
            if (recv_buff[3] == 'r')
            {
                ED64Handle* fthandle = (ED64Handle*) malloc(sizeof(ED64Handle));
                free(device_info);
                fthandle->device_index = i;
                cart->structure = fthandle;
                return DEVICEERR_OK;
            }
        }
    }

    // Could not find the flashcart
    free(device_info);
    return DEVICEERR_NOTCART;
}


/*==============================
    device_maxromsize_everdrive
    Gets the max ROM size that 
    the EverDrive supports
    @return The max ROM size
==============================*/

uint32_t device_maxromsize_everdrive()
{
    return 64*1024*1024;
}


/*==============================
    device_shouldpadrom_everdrive
    Checks if the ROM should be
    padded before uploading on the
    EverDrive.
    @return Whether or not to pad
            the ROM.
==============================*/

bool device_shouldpadrom_everdrive()
{
    return false;
}


/*==============================
    device_explicitcic_everdrive
    Checks if the EverDrive requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_everdrive(byte* bootcode)
{
    (void)(bootcode); // Ignore unused paramater warning
    return false;
}


/*==============================
    device_open_everdrive
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_everdrive(CartDevice* cart)
{
    ED64Handle* fthandle = (ED64Handle*) cart->structure;

    // Open the cart
    if (FT_Open(fthandle->device_index, &fthandle->handle) != FT_OK || fthandle->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // Reset the cart
    if (FT_ResetDevice(fthandle->handle) != FT_OK)
        return DEVICEERR_RESETFAIL;
    if (FT_SetTimeouts(fthandle->handle, 500, 500) != FT_OK)
        return DEVICEERR_TIMEOUTSETFAIL;
    if (FT_Purge(fthandle->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
        return DEVICEERR_PURGEFAIL;

    // Ok
    return DEVICEERR_OK;
}


/*==============================
    device_close_everdrive
    Closes the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_everdrive(CartDevice* cart)
{
    ED64Handle* fthandle = (ED64Handle*) cart->structure;
    if (FT_Close(fthandle->handle) != FT_OK)
        return DEVICEERR_CLOSEFAIL;
    free(fthandle);
    cart->structure = NULL;
    return DEVICEERR_OK;
}