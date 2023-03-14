/***************************************************************
                       device_64drive.cpp

Handles 64Drive HW1 and HW2 USB communication. A lot of the code
here is courtesy of MarshallH's 64Drive USB tool:
http://64drive.retroactive.be/support.php
***************************************************************/

#include "device_64drive.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <thread>
#include <chrono>


/*********************************
        Function Prototypes
*********************************/

DeviceError device_sendcmd_64drive(FTDIDevice* cart, uint8_t command, bool reply, uint32_t* result, uint32_t numparams, ...);


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
    device_maxromsize_64drive
    Gets the max ROM size that 
    the 64Drive supports
    @return The max ROM size
==============================*/

uint32_t device_maxromsize_64drive()
{
    // TODO: Extended address mode for 256 MB ROMs
    return 64*1024*1024;
}


/*==============================
    device_shouldpadrom_64drive
    Checks if the ROM should be
    padded before uploading on the
    64Drive.
    @return Whether or not to pad
            the ROM.
==============================*/

bool device_shouldpadrom_64drive()
{
    // While the 64Drive does not need ROMs to be padded,
    // there is a firmware bug which causes the last few
    // bytes to get corrupted. This was not fun to debug...
    // Since the 64Drive is super fast at uploading, it 
    // doesn't hurt to pad the ROM.
    return true;
}


/*==============================
    device_explicitcic_64drive
    Checks if the 64Drive requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_64drive(uint8_t* bootcode)
{
    device_setcic(cic_from_hash(romhash(bootcode, 4032)));
    return true;
}


/*==============================
    device_testdebug_64drive
    Checks whether the 64Drive can use debug mode
    @param A pointer to the cart context
    @returns True if the firmware version is higher than 2.04
==============================*/

DeviceError device_testdebug_64drive(FTDIDevice* cart)
{
    uint32_t result;
    DeviceError err = device_sendcmd_64drive(cart, DEV_CMD_GETVER, true, &result, 0);
    if (err != DEVICEERR_OK)
        return err;

    // Firmware must be 2.05 or higher for USB stuff
    if ((result & 0x0000FFFF) < 205)
        return DEVICEERR_64D_CANTDEBUG;

    // Otherwise, we can use debug mode.
    return DEVICEERR_OK;
}


/*==============================
    device_sendcmd_64drive
    Opens the USB pipe
    @param  A pointer to the cart context
    @param  The command to send
    @param  A bool stating whether a reply should be expected
    @param  A pointer to store the result of the reply
    @param  The number of extra params to send
    @param  The extra variadic commands to send
    @return The device error, or OK
==============================*/

DeviceError device_sendcmd_64drive(FTDIDevice* cart, uint8_t command, bool reply, uint32_t* result, uint32_t numparams, ...)
{
    uint8_t  send_buff[32];
    uint32_t recv_buff[32];
    va_list  params;

    // Clear the buffers
    memset(send_buff, 0, 32*sizeof(uint8_t));
    memset(recv_buff, 0, 32*sizeof(uint32_t));

    // Setup the command to send
    send_buff[0] = command;
    send_buff[1] = 0x43;    // C
    send_buff[2] = 0x4D;    // M
    send_buff[3] = 0x44;    // D

    // Append extra arguments to the command if needed
    va_start(params, numparams);
    if (numparams > 0)
        *(uint32_t*)&send_buff[4] = swap_endian(va_arg(params, uint32_t));
    if (numparams > 1)
        *(uint32_t*)&send_buff[8] = swap_endian(va_arg(params, uint32_t));
    va_end(params);

    // Write to the cart
    if (FT_Write(cart->handle, send_buff, 4+(numparams*4), &cart->bytes_written) != FT_OK)
        return DEVICEERR_WRITEFAIL;
    if (cart->bytes_written == 0)
        return DEVICEERR_WRITEZERO;

    // If the command expects a response
    if (reply)
    {
        uint32_t expected = command << 24 | 0x00504D43;

        // These two instructions do not return a success, so ignore them
        if (command == DEV_CMD_PI_WR_BL || command == DEV_CMD_PI_WR_BL_LONG)
            return DEVICEERR_OK;

        // Read the reply from the 64Drive
        if (FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read) != FT_OK)
            return DEVICEERR_NOCOMPSIG;

        // Store the result if requested
        if (result != NULL)
        {
            (*result) = swap_endian(recv_buff[0]);

            // Read the rest of the stuff and the CMP as well
            if (FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read) != FT_OK)
                return DEVICEERR_NOCOMPSIG;
            if (FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read) != FT_OK)
                return DEVICEERR_NOCOMPSIG;
        }

        // Check that we received the signal that the operation completed
        if (memcmp(recv_buff, &expected, 4) != 0)
            return DEVICEERR_NOCOMPSIG;
    }

    // Success
    return DEVICEERR_OK;
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
    device_sendrom_64drive
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
    @return The device error, or OK
==============================*/

DeviceError device_sendrom_64drive(FTDIDevice* cart, uint8_t* rom, uint32_t size)
{
    uint32_t ram_addr = 0x0;
    uint32_t bytes_left = size;
    uint32_t bytes_done = 0;
    uint32_t bytes_do;
    uint32_t chunk = 0;
    DWORD    cmps;
    uint8_t  cmpbuff[4];

    // Start by setting the CIC
    if (cart->cictype != CIC_NONE)
    {
        DeviceError err = device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | ((uint32_t)cart->cictype), 0);
        if (err != DEVICEERR_OK)
            return err;
    }
    
    // Then, set the save type
    if (cart->savetype != SAVE_NONE)
    {
        DeviceError err = device_sendcmd_64drive(cart, DEV_CMD_SETSAVE, false, NULL, 1, (uint32_t)cart->savetype, 0);
        if (err != DEVICEERR_OK)
            return err;
    }

    // Decide a better, more optimized chunk size for sending
    if (size > 16*1024*1024)
        chunk = 32;
    else if (size > 2*1024*1024)
        chunk = 16;
    else
        chunk = 4;
    chunk *= 128*1024; // Convert to megabytes

    // Upload the ROM in a loop
    while (1)
    {
        // Decide how many bytes to send
        if (bytes_left >= chunk)
            bytes_do = chunk;
        else
            bytes_do = bytes_left;

        // If we have an uneven number of bytes, fix that
        if (bytes_do%4 != 0)
            bytes_do -= bytes_do%4;

        // End if we've got nothing else to send
        if (bytes_do <= 0)
            break;
        
        // Check if the upload was cancelled
        if (device_uploadcancelled())
            return DEVICEERR_UPLOADCANCELLED;

        // Try to send chunks
        for (int i=0; i<2; i++)
        {
            // If we failed the first time, clear the USB and try again
            if (i == 1)
            {
                if (FT_ResetPort(cart->handle) != FT_OK)
                    return DEVICEERR_RESETPORTFAIL;
                if (FT_ResetDevice(cart->handle) != FT_OK)
                    return DEVICEERR_RESETFAIL;
                if (FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
                    return DEVICEERR_PURGEFAIL;
            }

            // Send the chunk to cartridge RAM
            device_sendcmd_64drive(cart, DEV_CMD_LOADRAM, false, NULL, 2, ram_addr, (bytes_do & 0xffffff) | 0 << 24);
            FT_Write(cart->handle, rom+bytes_done, bytes_do, &cart->bytes_written);

            // If we managed to write, don't try again
            if (cart->bytes_written)
                break;
        }

        // Check for a timeout
        if (cart->bytes_written == 0)
            return DEVICEERR_TIMEOUT;

        // Ignore the success response
        cart->status = FT_Read(cart->handle, cmpbuff, 4, &cart->bytes_read);

        // Keep track of how many bytes were uploaded
        bytes_left -= bytes_do;
        bytes_done += bytes_do;
        ram_addr += bytes_do;
        device_setuploadprogress((((float)bytes_done)/((float)size))*100.0f);
    }

    // Wait for the CMP signal
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Read the incoming CMP signals to ensure everything's fine
    FT_GetQueueStatus(cart->handle, &cmps);
    while (cmps > 0)
    {
        // Read the CMP signal and ensure it's correct
        FT_Read(cart->handle, cmpbuff, 4, &cart->bytes_read);
        if (cmpbuff[0] != 'C' || cmpbuff[1] != 'M' || cmpbuff[2] != 'P' || cmpbuff[3] != 0x20)
            return DEVICEERR_64D_BADCMP;

        // Wait a little bit before reading the next CMP signal
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        FT_GetQueueStatus(cart->handle, &cmps);
    }

    // Success
    device_setuploadprogress(100.0f);
    return DEVICEERR_OK;
}


/*==============================
    device_senddata_64drive
    TODO
==============================*/

DeviceError device_senddata_64drive(FTDIDevice* cart, USBDataType datatype, uint8_t* data, uint32_t size)
{
    uint8_t buf[4];
    uint32_t cmp_magic;
    uint32_t newsize = 0;
    uint8_t* datacopy = NULL;
    DeviceError err;

    // Pad the data to be 512 byte aligned if it is large, if not then to 4 bytes
    if (size > 512 && (size%512) != 0)
        newsize = (size-(size%512))+512;
    else if (size % 4 != 0)
        newsize = (size & ~3) + 4;
    else
        newsize = size;

    // Copy the data onto a temp variable
    datacopy = (uint8_t*) calloc(newsize, 1);
    if (datacopy == NULL)
        return DEVICEERR_MALLOCFAIL;
    memcpy(datacopy, data, size);

    // Send this block of data
    device_setuploadprogress(0.0f);
    err = device_sendcmd_64drive(cart, DEV_CMD_USBRECV, false, NULL, 1, (newsize & 0x00FFFFFF) | datatype << 24, 0);
    if (err != DEVICEERR_OK)
        return err;
    if (FT_Write(cart->handle, datacopy, newsize, &cart->bytes_written) != FT_OK)
        return DEVICEERR_WRITEFAIL;

    // Read the CMP signal
    if (FT_Read(cart->handle, buf, 4, &cart->bytes_read) != FT_OK)
        return DEVICEERR_READFAIL;
    cmp_magic = swap_endian(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]);
    if (cmp_magic != 0x434D5040)
        return DEVICEERR_64D_BADCMP;

    // Free used up resources
    device_setuploadprogress(100.0f);
    free(datacopy);
    return DEVICEERR_OK;
}


/*==============================
    device_receivedata_64drive
    TODO
==============================*/

DeviceError device_receivedata_64drive(FTDIDevice* cart, uint32_t* dataheader, uint8_t** buff)
{
    uint32_t size;

    // First, check if we have data to read
    FT_GetQueueStatus(cart->handle, &size);

    // If we do
    if (size > 0)
    {
        uint32_t read = 0;
        uint8_t temp[4];

        // Ensure we have valid data by reading the header
        if (FT_Read(cart->handle, temp, 4, &cart->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        if (temp[0] != 'D' || temp[1] != 'M' || temp[2] != 'A' || temp[3] != '@')
            return DEVICEERR_64D_BADDMA;

        // Get information about the incoming data and store it in dataheader
        if (FT_Read(cart->handle, temp, 4, &cart->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        (*dataheader) = swap_endian(temp[3] << 24 | temp[2] << 16 | temp[1] << 8 | temp[0]);

        // Read the data into the buffer, in 512 byte chunks
        size = (*dataheader) & 0xFFFFFF;
        (*buff) = (uint8_t*)malloc(size);
        if ((*buff) == NULL)
            return DEVICEERR_MALLOCFAIL;

        // Do in 512 byte chunks so we have a progress bar (and because the N64 does it in 512 byte chunks anyway)
        device_setuploadprogress(0.0f);
        while (read < size)
        {
            uint32_t readamount = size-read;
            if (readamount > 512)
                readamount = 512;
            if (FT_Read(cart->handle, (*buff)+read, readamount, &cart->bytes_read) != FT_OK)
                return DEVICEERR_READFAIL;
            read += cart->bytes_read;
            device_setuploadprogress((((float)read)/((float)size))*100.0f);
        }

        // Read the completion signal
        if (FT_Read(cart->handle, temp, 4, &cart->bytes_read) != FT_OK)
            return DEVICEERR_READFAIL;
        if (temp[0] != 'C' || temp[1] != 'M' || temp[2] != 'P' || temp[3] != 'H')
            return DEVICEERR_64D_BADCMP;
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