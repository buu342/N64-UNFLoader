/***************************************************************
                      device_everdrive.cpp

Handles EverDrive USB communication. A lot of the code here
is courtesy of KRIKzz's USB tool:
http://krikzz.com/pub/support/everdrive-64/x-series/dev/
***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_everdrive.h"


/*==============================
    device_test_everdrive
    Checks whether the device passed as an argument is EverDrive
    @param A pointer to the cart context
    @param The index of the cart
    @returns true if the cart is an EverDrive, or false otherwise
==============================*/

bool device_test_everdrive(ftdi_context_t* cart, int index)
{
    if (strcmp(cart->dev_info[index].Description, "FT245R USB FIFO") == 0 &&cart->dev_info[index].ID == 0x04036001)
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
        {
            free(cart->dev_info);
            terminate("Could not open device.");
        }

        // Initialize the USB
        testcommand(FT_ResetDevice(cart->handle), "Unable to reset flashcart.");
        testcommand(FT_SetTimeouts(cart->handle, 500, 500), "Unable to set flashcart timeouts.");
        testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Unable to purge USB contents.");

        // Send the test command
        testcommand(FT_Write(cart->handle, send_buff, 16, &cart->bytes_written), "Unable to write to flashcart.");
        testcommand(FT_Read(cart->handle, recv_buff, 16, &cart->bytes_read), "Unable to read from flashcart.");
        testcommand(FT_Close(cart->handle), "Unable to close flashcart.");
        cart->handle = 0;

        // Check if the EverDrive responded correctly
        return recv_buff[3] == 'r';
    }
    return false;
}


/*==============================
    device_open_everdrive
    Opens the USB pipe
    @param A pointer to the cart context
==============================*/

void device_open_everdrive(ftdi_context_t* cart)
{
    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || !cart->handle)
        terminate("Unable to open flashcart.");

    // Reset the cart
    testcommand(FT_ResetDevice(cart->handle), "Unable to reset flashcart.");
    testcommand(FT_SetTimeouts(cart->handle, 500, 500), "Unable to set flashcart timeouts.");
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Unable to purge USB contents.");
}

/*==============================
    device_sendcmd_everdrive
    Sends a command to the flashcart
    @param A pointer to the cart context
    @param A char with the command to send
    @param The address to send the data to
    @param The size of the data to send
    @param Any other args to send with the data
==============================*/

void device_sendcmd_everdrive(ftdi_context_t* cart, char command, int address, int size, int arg)
{
    char* cmd_buffer = (char*) malloc(sizeof(char) * 16);
    size /= 512;

    // Check we managed to malloc
    if (cmd_buffer == NULL)
        terminate("Unable to allocate memory for buffer.");

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
    FT_Write(cart->handle, cmd_buffer, 16, &cart->bytes_written);

    // Free the allocated memory
    free(cmd_buffer);
}


/*==============================
    device_sendrom_everdrive
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
==============================*/

void device_sendrom_everdrive(ftdi_context_t* cart, FILE *file, u32 size)
{
	int	   bytes_done = 0;
    int	   bytes_left;
	int	   bytes_do;
    char*  rom_buffer = (char*) malloc(sizeof(int) * 32*1024);
    int    crc_area = 0x100000 + 4096;
    time_t upload_time = clock();

    // Check we managed to malloc
    if (rom_buffer == NULL)
        terminate("Unable to allocate memory for buffer.");

    // Fill memory if the file is too small
    if ((int)size < crc_area)
    {
        char recv_buff[16];
        pdprint("Filling ROM.\n", CRDEF_PROGRAM);
        device_sendcmd_everdrive(cart, 'c', 0x10000000, crc_area, 0);
        device_sendcmd_everdrive(cart, 't', 0, 0, 0);
        FT_Read(cart->handle, recv_buff, 16, &cart->bytes_read);
    }

    // Savetype message
    if (global_savetype != 0)
        pdprint("Save type set to %d.\n", CRDEF_PROGRAM, global_savetype);

    // Get the correctly padded ROM size
    size = calc_padsize(size);
    bytes_left = size;

    // Initialize the progress bar
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", CRDEF_PROGRAM, 0);

    // Send a command saying we're about to write to the cart
    device_sendcmd_everdrive(cart, 'W', 0x10000000, size, 0);

    // Upload the ROM
    for ( ; ; )
    {
        int i;

        // Decide how many bytes to send
		if (bytes_left >= 0x8000)
			bytes_do = 0x8000;
		else
			bytes_do = bytes_left;

        // End if we've got nothing else to send
		if (bytes_do <= 0)
            break;

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

            // Read the ROM to the buffer and byteswap it if needed
            fread(rom_buffer, bytes_do, 1, file);
            if (global_z64)
                for (j=0; j<bytes_do; j+=2)
                    SWAP(rom_buffer[j], rom_buffer[j+1]);

            // Set Savetype if first time sending data
            if (global_savetype != 0 && bytes_done == 0)
            {
                rom_buffer[0x3C] = 'E';
                rom_buffer[0x3D] = 'D';
                switch (global_savetype)
                {
                    case 1: rom_buffer[0x3F] = 0x10; break;
                    case 2: rom_buffer[0x3F] = 0x20; break;
                    case 3: rom_buffer[0x3F] = 0x30; break;
                    case 4: rom_buffer[0x3F] = 0x50; break;
                    case 5: rom_buffer[0x3F] = 0x40; break;
                    case 6: rom_buffer[0x3F] = 0x60; break;
                }
            }

            // Send the chunk to RAM. If we reached EOF it doesn't matter what we send
            // TODO: Send 0's when EOF is reached
			FT_Write(cart->handle, rom_buffer, bytes_do, &cart->bytes_written);

            // If we managed to write, don't try again
			if (cart->bytes_written)
                break;
		}

        // Check for a timeout
		if (cart->bytes_written == 0)
            terminate("Everdrive timed out.");

         // Keep track of how many bytes were uploaded
		bytes_left -= bytes_do;
		bytes_done += bytes_do;

		// Draw the progress bar
		progressbar_draw("Uploading ROM", CRDEF_PROGRAM, (float)bytes_done/size);
    }

    // Send the PIFboot command
    #ifndef LINUX // Delay is needed or it won't boot properly
        Sleep(500);
    #else
        usleep(500);
    #endif
    pdprint_replace("Sending pifboot\n", CRDEF_PROGRAM);
    device_sendcmd_everdrive(cart, 's', 0, 0, 0);

    // Write the filename of the save file if necessary
    if (global_savetype != 0)
    {
        u32 i;
        u32 len = strlen(global_filename);
        int extension = -1;
        char filename[256];
        memset(filename, 0, 256);
        for (i=len; i>0; i--)
        {
            if (global_filename[i] == '.' && extension == -1)
                extension = i;
            if (global_filename[i] == '\\' || global_filename[i] == '/')
            {
                i++;
                break;
            }
        }
        if (extension == -1)
            extension = len;
        memcpy(filename, global_filename+i, (extension-i));
        FT_Write(cart->handle, filename, 256, &cart->bytes_written);
    }

    // Print that we've finished
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, ((double)(clock()-upload_time))/CLOCKS_PER_SEC);
    free(rom_buffer);
}


/*==============================
    device_senddata_everdrive
    Sends data to the flashcart
    @param A pointer to the cart context
    @param A pointer to the data to send
    @param The size of the data
==============================*/

void device_senddata_everdrive(ftdi_context_t* cart, int datatype, char* data, u32 size)
{
    int left = size;
    int read = 0;
    u32 header = (size & 0xFFFFFF) | (datatype << 24);
    char*  buffer = (char*) malloc(sizeof(char) * 512);
    char cmp[] = {'C', 'M', 'P', 'H'};

    // Put in the DMA header along with length and type information in the buffer
    buffer[0] = 'D';
    buffer[1] = 'M';
    buffer[2] = 'A';
    buffer[3] = '@';
    buffer[4] = (header >> 24) & 0xFF;
    buffer[5] = (header >> 16) & 0xFF;
    buffer[6] = (header >> 8)  & 0xFF;
    buffer[7] = header & 0xFF;

    // Send the DMA message
    FT_Write(cart->handle, buffer, 16, &cart->bytes_written);

    // Upload the data
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading data", CRDEF_INFO, 0);
    for ( ; ; )
    {
        int i, block;

        // Decide how many bytes to send
		if (left >= 512)
			block = 512;
		else
			block = left;

        // End if we've got nothing else to send
		if (block <= 0)
            break;

        // Try to send chunks
		for (i=0; i<2; i++)
        {
            // If we failed the first time, clear the USB and try again
			if (i == 1)
            {
				FT_ResetPort(cart->handle);
				FT_ResetDevice(cart->handle);
				FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX);
			}

			// Send the chunk through USB
			memcpy(buffer, data+read, block);
			FT_Write(cart->handle, buffer, 512, &cart->bytes_written);

            // If we managed to write, don't try again
			if (cart->bytes_written)
                break;
		}

        // Check for a timeout
		if (cart->bytes_written == 0)
            terminate("Everdrive timed out.");

        // Draw the progress bar
        progressbar_draw("Uploading data", CRDEF_INFO, (float)read/size);

        // Keep track of how many bytes were uploaded
		left -= block;
		read += block;
    }

    // Send the CMP signal
    memcpy(buffer, cmp, 4);
    FT_Write(cart->handle, buffer, 16, &cart->bytes_written);

    // Free the data used by the buffer
    free(buffer);
}


/*==============================
    device_close_everdrive
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_everdrive(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Unable to close flashcart.");
    cart->handle = 0;
}
