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
    return true;
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

    // Start by setting the CIC
    if (cart->cictype != CIC_NONE)
    {
        DeviceError err = device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | cart->cictype, 0);
        if (err != DEVICEERR_OK)
            return err;
    }
    else
    {
        CICType cic;
        DeviceError err;
        uint8_t* bootcode = (uint8_t*)malloc(4032);
        if (bootcode == NULL)
            return DEVICEERR_MALLOCFAIL;

        // Read the bootcode and store it
        memcpy(bootcode, rom+0x40, 4032);

        // Pick the CIC from the bootcode
        cic = cic_from_hash(romhash(bootcode, 4032));
        if (cic != CIC_NONE)
        {
            err = device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | cic, 0);
            if (err != DEVICEERR_OK)
                return err;
        }

        // Free used memory
        free(bootcode);
    }
    
    // Then, set the save type
    if (cart->savetype != SAVE_NONE)
    {
        DeviceError err = device_sendcmd_64drive(cart, DEV_CMD_SETSAVE, false, NULL, 1, cart->savetype, 0);
        if (err != DEVICEERR_OK)
            return err;
    }

    // Decide a better, more optimized chunk size
    if (size > 16*1024*1024)
        chunk = 32;
    else if ( size > 2*1024*1024)
        chunk = 16;
    else
        chunk = 4;
    chunk *= 128*1024; // Convert to megabytes

    /*
    for ( ; ; )
    {
        int i, ch;

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
        
        // Check if ESC was pressed
        ch = getch();
        if (ch == CH_ESCAPE)
        {
            pdprint_replace("ROM upload canceled by the user\n", CRDEF_PROGRAM);
            free(rom_buffer);
            return;
        }

        // Try to send chunks
        for (i=0; i<2; i++)
        {
            int j;

            // If we failed the first time, clear the USB and try again
            if (i == 1)
            {
                FT_ResetPort(cart->handle);
                FT_ResetDevice(cart->handle);
                FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX);
            }

            // Send the chunk to RAM
            device_sendcmd_64drive(cart, DEV_CMD_LOADRAM, false, NULL, 2, ram_addr, (bytes_do & 0xffffff) | 0 << 24);
            fread(rom_buffer, bytes_do, 1, file);
            if (global_z64)
                for (j=0; j<bytes_do; j+=2)
                    SWAP(rom_buffer[j], rom_buffer[j+1]);
            FT_Write(cart->handle, rom_buffer, bytes_do, &cart->bytes_written);

            // If we managed to write, don't try again
            if (cart->bytes_written)
                break;
        }

        // Check for a timeout
        if (cart->bytes_written == 0)
        {
            free(rom_buffer);
            terminate("64Drive timed out.");
        }

        // Ignore the success response
        cart->status = FT_Read(cart->handle, rom_buffer, 4, &cart->bytes_read);

        // Keep track of how many bytes were uploaded
        bytes_left -= bytes_do;
        bytes_done += bytes_do;
        ram_addr += bytes_do;
    }

    // Wait for the CMP signal
    #ifndef LINUX
        Sleep(50);
    #else
        usleep(50);
    #endif

    // Read the incoming CMP signals to ensure everything's fine
    FT_GetQueueStatus(cart->handle, &cmps);
    while (cmps > 0)
    {
        // Read the CMP signal and ensure it's correct
        FT_Read(cart->handle, rom_buffer, 4, &cart->bytes_read);
        if (rom_buffer[0] != 'C' || rom_buffer[1] != 'M' || rom_buffer[2] != 'P' || rom_buffer[3] != 0x20)
            terminate("Received wrong CMPlete signal: %c %c %c %02x.", rom_buffer[0], rom_buffer[1], rom_buffer[2], rom_buffer[3]);

        // Wait a little bit before reading the next CMP signal
        #ifndef LINUX
            Sleep(50);
        #else
            usleep(50);
        #endif
        FT_GetQueueStatus(cart->handle, &cmps);
    }

    // Print that we've finished
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, ((double)(clock()-upload_time))/CLOCKS_PER_SEC);
    */

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