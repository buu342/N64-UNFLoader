/***************************************************************
                      device_everdrive.cpp

Handles EverDrive USB communication. A lot of the code here
is courtesy of KRIKzz's USB tool:
http://krikzz.com/pub/support/everdrive-64/x-series/dev/
***************************************************************/

#include "device_everdrive.h"
#include "Include/ftd2xx.h"
#include <string.h>
#include <thread>
#include <chrono>

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
        // Look for an EverDrive
        if (strcmp(device_info[i].Description, "FT245R USB FIFO") == 0 && device_info[i].ID == 0x04036001)
        {
            FT_HANDLE temphandle;
            DWORD bytes_written;
            DWORD bytes_read;
            char send_buff[16];
            char recv_buff[16];
            memset(send_buff, 0, 16);
            memset(recv_buff, 0, 16);

            // If we don't have a ROM, we probably just want debug mode, so assume that this is an ED
            if (device_getrom() == NULL)
            {
                ED64Handle* fthandle = (ED64Handle*)malloc(sizeof(ED64Handle));
                free(device_info);
                fthandle->device_index = i;
                cart->structure = fthandle;
                return DEVICEERR_OK;
            }

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
    device_rompadding_everdrive
    Calculates the correct ROM size 
    for uploading on the 64Drive
    @param  The current ROM size
    @return The correct ROM size 
            for uploading.
==============================*/

uint32_t device_rompadding_everdrive(uint32_t romsize)
{
    return ALIGN(romsize, 512);
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
    device_sendcmd_everdrive
    Sends a command to the flashcart
    @param A pointer to the cart handle
    @param A char with the command to send
    @param The address to send the data to
    @param The size of the data to send
    @param Any other args to send with the data
==============================*/

static DeviceError device_sendcmd_everdrive(ED64Handle* cart, char command, int address, int size, int arg)
{
    byte cmd_buffer[16];
    size /= 512;

    // Define the command and send it
    cmd_buffer[0] = 'c';
    cmd_buffer[1] = 'm';
    cmd_buffer[2] = 'd';
    cmd_buffer[3] = command;
    cmd_buffer[4] = (char) (address >> 24);
    cmd_buffer[5] = (char) (address >> 16);
    cmd_buffer[6] = (char) (address >> 8);
    cmd_buffer[7] = (char) (address);
    cmd_buffer[8] = (char) (size >> 24);
    cmd_buffer[9] = (char) (size >> 16);
    cmd_buffer[10]= (char) (size >> 8);
    cmd_buffer[11]= (char) (size);
    cmd_buffer[12]= (char) (arg >> 24);
    cmd_buffer[13]= (char) (arg >> 16);
    cmd_buffer[14]= (char) (arg >> 8);
    cmd_buffer[15]= (char) (arg);
    if (FT_Write(cart->handle, cmd_buffer, 16, &cart->bytes_written) != FT_OK)
        return DEVICEERR_WRITEFAIL;
    return DEVICEERR_OK;
}



/*==============================
    device_sendrom_everdrive
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
    @return The device error, or OK
==============================*/

DeviceError device_sendrom_everdrive(CartDevice* cart, byte* rom, uint32_t size)
{
    DeviceError err;
    ED64Handle* fthandle = (ED64Handle*)cart->structure;
    uint32_t    bytes_done = 0;
    uint32_t    bytes_left = size;
    uint32_t    crc_area = 0x100000 + 4096;

    // Fill memory if the file is too small
    if (size < crc_area)
    {
        char recv_buff[16];
        err = device_sendcmd_everdrive(fthandle, 'c', 0x10000000, crc_area, 0);
        if (err != DEVICEERR_OK)
            return err;
        err = device_sendcmd_everdrive(fthandle, 't', 0, 0, 0);
        if (err != DEVICEERR_OK)
            return err;
        if (FT_Read(fthandle->handle, recv_buff, 16, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
    }

    // Set the save type
    if (cart->savetype != SAVE_NONE)
    {
        rom[0x3C] = 'E';
        rom[0x3D] = 'D';
        switch (cart->savetype)
        {
            case SAVE_EEPROM4K:     rom[0x3F] = 0x10; break;
            case SAVE_EEPROM16K:    rom[0x3F] = 0x20; break;
            case SAVE_SRAM256:      rom[0x3F] = 0x30; break;
            case SAVE_FLASHRAM:     rom[0x3F] = 0x50; break;
            case SAVE_SRAM768:      rom[0x3F] = 0x40; break;
            case SAVE_FLASHRAMPKMN: rom[0x3F] = 0x60; break;
            default: break;
        }
    }

    // Send a command saying we're about to write to the cart
    err = device_sendcmd_everdrive(fthandle, 'W', 0x10000000, size, 0);
    if (err != DEVICEERR_OK)
        return err;

    // Upload the ROM in a loop
    while (bytes_left > 0)
    {
        uint32_t bytes_do = 0x8000;

        // Decide how many bytes to send
        if (bytes_left < bytes_do)
            bytes_do = bytes_left;

        // Check if the upload was cancelled
        if (device_uploadcancelled())
           break;

        // Send the data to the everdrive
        if (FT_Write(fthandle->handle, rom + bytes_done, bytes_do, &fthandle->bytes_written)  != FT_OK)
            return DEVICEERR_WRITEFAIL;
        if (fthandle->bytes_written == 0)
            return DEVICEERR_TIMEOUT;

        // Update the upload progress
        device_setuploadprogress((((float)bytes_done) / ((float)size)) * 100.0f);
        bytes_left -= bytes_do;
        bytes_done += bytes_do;
    }

    // Sleep for a bit before sending the PIF command (the delay is needed)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Return an error if the upload was cancelled
    if (device_uploadcancelled())
        return DEVICEERR_UPLOADCANCELLED;

    // Send the PIFboot command
    err = device_sendcmd_everdrive(fthandle, 's', 0, 0, 0);
    if (err != DEVICEERR_OK)
        return err;

    // Write the filename of the save file if necessary
    if (cart->savetype != SAVE_NONE)
    {
        int32_t  i;
        char*    path = device_getrom();
        uint32_t pathlen = strlen(device_getrom());
        char*    filename = (char*)malloc(pathlen+1);
        int32_t  extension_start = -1;
        if (filename == NULL)
            return DEVICEERR_MALLOCFAIL;

        // Extract the filename from the path
        for (i=pathlen; i>=0; i--)
        {
            if (path[i] == '.' && extension_start == -1)
                extension_start = i;
            if (path[i] == '\\' || path[i] == '/')
            {
                i++;
                break;
            }
        }

        // Copy the string and send it over to the cart
        if (extension_start == -1)
            extension_start = pathlen;
        memcpy(filename, path+i, (extension_start-i));
        if (FT_Write(fthandle->handle, filename, 256, &fthandle->bytes_written) != FT_OK)
            return DEVICEERR_WRITEFAIL;
        free(filename);
    }

    // Success
    device_setuploadprogress(100.0f);
    return DEVICEERR_OK;
}


/*==============================
    device_testdebug_everdrive
    Checks whether this cart can use debug mode
    @param A pointer to the cart context
    @returns Always returns DEVICEERR_OK.
==============================*/

DeviceError device_testdebug_everdrive(CartDevice* cart)
{
    // TODO: Make this check EverDrive firmware version
    (void)cart; // Prevents error about unused parameter
    return DEVICEERR_OK;
}


/*==============================
    device_senddata_everdrive
    Sends data to the EverDrive
    @param  A pointer to the cart context
    @param  The datatype that is being sent
    @param  A buffer containing said data
    @param  The size of the data
    @return The device error, or OK
==============================*/

DeviceError device_senddata_everdrive(CartDevice* cart, USBDataType datatype, byte* data, uint32_t size)
{
    ED64Handle* fthandle = (ED64Handle*)cart->structure;
    byte     buffer[16];
    uint32_t header;
    uint32_t newsize = device_getprotocol() == PROTOCOL_VERSION2 ? ALIGN(size, 2) : ALIGN(size, 512);
    byte*    datacopy = NULL;
    uint32_t bytes_done = 0;
    uint32_t bytes_left = newsize;

    // Put in the DMA header along with length and type information in the buffer
    header = (size & 0xFFFFFF) | (((uint32_t)datatype) << 24);
    buffer[0] = 'D';
    buffer[1] = 'M';
    buffer[2] = 'A';
    buffer[3] = '@';
    buffer[4] = (header >> 24) & 0xFF;
    buffer[5] = (header >> 16) & 0xFF;
    buffer[6] = (header >> 8)  & 0xFF;
    buffer[7] = header & 0xFF;

    // Send the DMA message
    if (FT_Write(fthandle->handle, buffer, 8, &fthandle->bytes_written) != FT_OK)
        return DEVICEERR_WRITEFAIL;

    // Handle old protocol (doesn't matter what we sent, just needs to make the DMA message 16 bytes aligned)
    if (device_getprotocol() == PROTOCOL_VERSION1)
    {
        if (FT_Write(fthandle->handle, buffer, 8, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
    }

    // Copy the data onto a temp variable
    datacopy = (byte*) calloc(newsize, 1);
    if (datacopy == NULL)
        return DEVICEERR_MALLOCFAIL;
    memcpy(datacopy, data, size);

    // Send the data in chunks
    device_setuploadprogress(0.0f);
    while (bytes_left > 0)
    {
        uint32_t bytes_do = 512;
        if (bytes_left < 512)
            bytes_do = bytes_left;
        if (FT_Write(fthandle->handle, datacopy+bytes_done, bytes_do, &fthandle->bytes_written) != FT_OK)
            return DEVICEERR_WRITEFAIL;
        bytes_left -= bytes_do;
        bytes_done += bytes_do;
        device_setuploadprogress((((float)bytes_done)/((float)newsize))*100.0f);
    }

    // Send the CMP signal
    buffer[0] = 'C';
    buffer[1] = 'M';
    buffer[2] = 'P';
    buffer[3] = 'H';
    if (FT_Write(fthandle->handle, buffer, 4, &fthandle->bytes_written) != FT_OK)
        return DEVICEERR_WRITEFAIL;

    // Handle old protocol (doesn't matter what we sent, just needs to make the CMP message 16 bytes aligned)
    if (device_getprotocol() == PROTOCOL_VERSION1)
    {
        if (FT_Write(fthandle->handle, buffer, 12, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
    }

    // Free used up resources
    device_setuploadprogress(100.0f);
    free(datacopy);
    return DEVICEERR_OK;
}


/*==============================
    device_receivedata_everdrive
    Receives data from the EverDrive
    @param  A pointer to the cart context
    @param  A pointer to an 32-bit value where
            the received data header will be
            stored.
    @param  A pointer to a byte buffer pointer
            where the data will be malloc'ed into.
    @return The device error, or OK
==============================*/

DeviceError device_receivedata_everdrive(CartDevice* cart, uint32_t* dataheader, byte** buff)
{
    ED64Handle* fthandle = (ED64Handle*)cart->structure;
    DWORD size;
    uint32_t alignment = device_getprotocol() == PROTOCOL_VERSION2 ? 2 : 16;

    // First, check if we have data to read
    if (FT_GetQueueStatus(fthandle->handle, &size) != FT_OK)
        return DEVICEERR_POLLFAIL;

    // If we do
    if (size > 0)
    {
        uint32_t dataread = 0;
        uint32_t totalread = 0;
        byte     temp[4];

        // Ensure we have valid data by reading the header
        if (FT_Read(fthandle->handle, temp, 4, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        if (temp[0] != 'D' || temp[1] != 'M' || temp[2] != 'A' || temp[3] != '@')
            return DEVICEERR_64D_BADDMA;
        totalread += fthandle->bytes_read;

        // Get information about the incoming data and store it in dataheader
        if (FT_Read(fthandle->handle, temp, 4, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        (*dataheader) = swap_endian(temp[3] << 24 | temp[2] << 16 | temp[1] << 8 | temp[0]);
        totalread += fthandle->bytes_read;

        // Read the data into the buffer, in 512 byte chunks
        size = (*dataheader) & 0xFFFFFF;
        (*buff) = (byte*)malloc(size);
        if ((*buff) == NULL)
            return DEVICEERR_MALLOCFAIL;

        // Do in 512 byte chunks so we have a progress bar (and because the N64 does it in 512 byte chunks anyway)
        device_setuploadprogress(0.0f);
        while (dataread < size)
        {
            uint32_t readamount = size-dataread;
            if (readamount > 512)
                readamount = 512;
            if (FT_Read(fthandle->handle, (*buff)+dataread, readamount, &fthandle->bytes_read) != FT_OK)
                return DEVICEERR_READFAIL;
            totalread += fthandle->bytes_read;
            dataread += fthandle->bytes_read;
            device_setuploadprogress((((float)dataread)/((float)size))*100.0f);
        }

        // Read the completion signal
        if (FT_Read(fthandle->handle, temp, 4, &fthandle->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        if (temp[0] != 'C' || temp[1] != 'M' || temp[2] != 'P' || temp[3] != 'H')
            return DEVICEERR_64D_BADCMP;
        totalread += fthandle->bytes_read;

        // Ensure 2 byte alignment by reading X amount of bytes needed
        if (totalread % alignment != 0)
        {
            byte* tempbuff = (byte*)malloc(alignment*sizeof(byte));
            int left = alignment - (totalread % alignment);
            if (FT_Read(fthandle->handle, tempbuff, left, &fthandle->bytes_read) != FT_OK)
                return DEVICEERR_READFAIL;
            free(tempbuff);
        }
        device_setuploadprogress(100.0f);
    }
    else
    {
        (*dataheader) = 0;
        (*buff) = NULL;
    }

    // All's good
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
