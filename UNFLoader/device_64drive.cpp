/***************************************************************
                       device_64drive.cpp

Handles 64Drive HW1 and HW2 USB communication. A lot of the code
here is courtesy of MarshallH's 64Drive USB tool:
http://64drive.retroactive.be/support.php
***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_64drive.h"


/*********************************
        Function Prototypes
*********************************/

void device_sendcmd_64drive(ftdi_context_t* cart, u8 command, bool reply, u32* result, u32 numparams, ...);


/*==============================
    device_test_64drive1
    Checks whether the device passed as an argument is 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
    @returns true if the cart is a 64Drive HW1, or false otherwise
==============================*/

bool device_test_64drive1(ftdi_context_t* cart, int index)
{
    return (strcmp(cart->dev_info[index].Description, "64drive USB device A") == 0 && cart->dev_info[index].ID == 0x4036010);
}


/*==============================
    device_test_64drive2
    Checks whether the device passed as an argument is 64Drive HW2
    @param A pointer to the cart context
    @param The index of the cart
    @returns true if the cart is a 64Drive HW2, or false otherwise
==============================*/

bool device_test_64drive2(ftdi_context_t* cart, int index)
{
    return (strcmp(cart->dev_info[index].Description, "64drive USB device") == 0 && cart->dev_info[index].ID == 0x4036014);
}


/*==============================
    device_open_64drive
    Opens the USB pipe
    @param A pointer to the cart context
==============================*/

void device_open_64drive(ftdi_context_t* cart)
{
    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || !cart->handle)
        terminate("Unable to open flashcart.");

    // Reset the cart and set its timeouts
    testcommand(FT_ResetDevice(cart->handle), "Unable to reset flashcart.");
    testcommand(FT_SetTimeouts(cart->handle, 5000, 5000), "Unable to set flashcart timeouts.");

    // If the cart is in synchronous mode, enable the bits
    if (cart->synchronous)
    {
        testcommand(FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_RESET), "Unable to set bitmode %d.", FT_BITMODE_RESET);
        testcommand(FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_SYNC_FIFO), "Unable to set bitmode %d.", FT_BITMODE_SYNC_FIFO);
    }

    // Purge USB contents
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Unable to purge USB contents.");
}


/*==============================
    device_sendcmd_64drive
    Opens the USB pipe
    @param A pointer to the cart context
    @param The command to send
    @param A bool stating whether a reply should be expected
    @param A pointer to store the result of the reply
    @param The number of extra params to send
    @param The extra variadic commands to send
==============================*/

void device_sendcmd_64drive(ftdi_context_t* cart, u8 command, bool reply, u32* result, u32 numparams, ...)
{
    u8  send_buff[32];
    u32 recv_buff[32];
    va_list params;
    va_start(params, numparams);

    // Clear the buffers
    memset(send_buff, 0, 32);
    memset(recv_buff, 0, 32);

    // Setup the command to send
    send_buff[0] = command;
    send_buff[1] = 0x43;    // C
    send_buff[2] = 0x4D;    // M
    send_buff[3] = 0x44;    // D

    // Append extra arguments to the command if needed
    if (numparams > 0)
        *(u32 *)&send_buff[4] = swap_endian(va_arg(params, u32));
    if (numparams > 1)
        *(u32 *)&send_buff[8] = swap_endian(va_arg(params, u32));
    va_end(params);

    // Write to the cart
    testcommand(FT_Write(cart->handle, send_buff, 4+(numparams*4), &cart->bytes_written), "Unable to write to 64Drive.");
    if (cart->bytes_written == 0)
        terminate("No bytes were written to 64Drive.");

    // If the command expects a response
    if (reply)
    {
        u32 expected = command << 24 | 0x00504D43;

        // These two instructions do not return a success, so ignore them
        if (command == DEV_CMD_PI_WR_BL || command == DEV_CMD_PI_WR_BL_LONG)
            return;

        // Read the reply from the 64Drive
        testcommand(FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read), "Unable to read completion signal.");

        // Store the result if requested
        if (result != NULL)
        {
            (*result) = swap_endian(recv_buff[0]);

            // Read the rest of the stuff and the CMP as well
            testcommand(FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read), "Unable to read completion signal.");
            testcommand(FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read), "Unable to read completion signal.");
        }

        // Check that we received the signal that the operation completed
        if (memcmp(recv_buff, &expected, 4) != 0)
            terminate("Did not receive completion signal.");
    }
}


/*==============================
    device_testdebug_64drive
    Checks whether this cart can use debug mode
    @param A pointer to the cart context
    @returns True if the firmware version is higher than 2.04
==============================*/

bool device_testdebug_64drive(ftdi_context_t* cart)
{
    u32 result;
    device_sendcmd_64drive(cart, DEV_CMD_GETVER, true, &result, 0);

    // Firmware must be 2.05 or higher for USB stuff
    if ((result & 0x0000FFFF) < 205)
    {
        pdprint("Please upgrade to firmware 2.05 or higher to access USB debugging.\n", CRDEF_ERROR);
        return false;
    }

    // Otherwise, we can use debug mode.
    return true;
}


/*==============================
    device_sendrom_64drive
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
==============================*/

void device_sendrom_64drive(ftdi_context_t* cart, FILE *file, u32 size)
{
    u32    ram_addr = 0x0;
    int	   bytes_left = size;
    int	   bytes_done = 0;
    int	   bytes_do;
    int	   chunk = 0;
    u8*    rom_buffer = (u8*) malloc(sizeof(u8) * 4*1024*1024);
    time_t upload_time = clock();
    DWORD  cmps;

    // Check we managed to malloc
    if (rom_buffer == NULL)
        terminate("Unable to allocate memory for buffer.");

    // Handle CIC
    if (global_cictype == -1)
    {
        int cic = -1;
        int j;
        u8* bootcode = (u8*)malloc(4032);
        if (bootcode == NULL)
            terminate("Unable to allocate memory for bootcode buffer.");

        // Read the bootcode and store it
        fseek(file, 0x40, SEEK_SET);
        fread(bootcode, 1, 4032, file);
        fseek(file, 0, SEEK_SET);

        // Byteswap if needed
        if (global_z64)
            for (j=0; j<4032; j+=2)
                SWAP(bootcode[j], bootcode[j+1]);

        // Pick the CIC from the bootcode
        cic = cic_from_hash(romhash(bootcode, 4032));
        if (cic != -1)
        {
            // Set the CIC and print it
            cart->cictype = global_cictype;
            device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | cic, 0);
            if (cic == 303)
                terminate("The 8303 CIC is not supported through USB");
            pdprint("CIC set to ", CRDEF_PROGRAM);
            switch (cic)
            {
                case 0: pdprint("6101", CRDEF_PROGRAM); break;
                case 1: pdprint("6102", CRDEF_PROGRAM); break;
                case 2: pdprint("7101", CRDEF_PROGRAM); break;
                case 3: pdprint("7102", CRDEF_PROGRAM); break;
                case 4: pdprint("x103", CRDEF_PROGRAM); break;
                case 5: pdprint("x105", CRDEF_PROGRAM); break;
                case 6: pdprint("x106", CRDEF_PROGRAM); break;
                case 7: pdprint("5101", CRDEF_PROGRAM); break;
            }
            pdprint(" automatically.\n", CRDEF_PROGRAM);
        }
        else
            pdprint("Unknown CIC! Game might not boot properly!\n", CRDEF_PROGRAM);

        // Free used memory
        free(bootcode);
    }
    else if (cart->cictype == 0)
    {
        int cic = -1;

        // Get the CIC key
        switch(global_cictype)
        {
            case 0:
            case 6101: cic = 0; break;
            case 1:
            case 6102: cic = 1; break;
            case 2:
            case 7101: cic = 2; break;
            case 3:
            case 7102: cic = 3; break;
            case 4:
            case 103:
            case 6103:
            case 7103: cic = 4; break;
            case 5:
            case 105:
            case 6105:
            case 7105: cic = 5; break;
            case 6:
            case 106:
            case 6106:
            case 7106: cic = 6; break;
            case 7:
            case 5101: cic = 7; break;
            case 303: terminate("This CIC is not supported through USB");
            default: terminate("Unknown CIC type '%d'.", global_cictype);
        }

        // Set the CIC
        cart->cictype = global_cictype;
        device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | cic, 0);
        pdprint("CIC set to %d.\n", CRDEF_PROGRAM, global_cictype);
    }

    // Set Savetype
    if (global_savetype != 0)
    {
        device_sendcmd_64drive(cart, DEV_CMD_SETSAVE, false, NULL, 1, global_savetype, 0);
        pdprint("Save type set to %d.\n", CRDEF_PROGRAM, global_savetype);
    }

    // Decide a better, more optimized chunk size
    if (size > 16 * 1024 * 1024)
        chunk = 32;
    else if ( size > 2 * 1024 * 1024)
        chunk = 16;
    else
        chunk = 4;
    chunk *= 128 * 1024; // Convert to megabytes

    // Send chunks to the cart
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM (ESC to cancel)", CRDEF_PROGRAM, 0);
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

        // Draw the progress bar
        progressbar_draw("Uploading ROM (ESC to cancel)", CRDEF_PROGRAM, (float)bytes_done/size);
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
    free(rom_buffer);
}


/*==============================
    device_senddata_64drive
    Sends data to the flashcart
    @param A pointer to the cart context
    @param A pointer to the data to send
    @param The size of the data
==============================*/

void device_senddata_64drive(ftdi_context_t* cart, int datatype, char* data, u32 size)
{
    u8 buf[4];
    u32 cmp_magic;
    int newsize = 0;
    char* datacopy = NULL;

    // Pad the data to be 512 byte aligned if it is large, if not then to 4 bytes
    if (size > 512 && (size%512) != 0)
        newsize = (size-(size%512))+512;
    else if (size % 4 != 0)
        newsize = (size & ~3) + 4;
    else
        newsize = size;

    // Copy the data onto a temp variable
    datacopy = (char*) calloc(newsize, 1);
    memcpy(datacopy, data, size);
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading data", CRDEF_INFO, 0.0);

    // Send this block of data
    device_sendcmd_64drive(cart, DEV_CMD_USBRECV, false, NULL, 1, (newsize & 0x00FFFFFF) | datatype << 24, 0);
    cart->status = FT_Write(cart->handle, datacopy, newsize, &cart->bytes_written);

    // Read the CMP signal
    cart->status = FT_Read(cart->handle, buf, 4, &cart->bytes_read);
    cmp_magic = swap_endian(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]);
    if (cmp_magic != 0x434D5040)
        terminate("Received wrong CMPlete signal.");

    // Draw the progress bar
    progressbar_draw("Uploading data", CRDEF_INFO, 1.0);

    // Free used up resources
    free(datacopy);
}


/*==============================
    device_close_64drive
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_64drive(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Unable to close flashcart.");
    cart->handle = 0;
}
