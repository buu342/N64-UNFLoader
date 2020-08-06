/***************************************************************
                      device_everdrive3.cpp
                               
Handles EverDrive 3.0 USB communication. A lot of the code here 
is courtesy of KRIKzz, saturnu, and jsdf's USB tools:
http://krikzz.com/pub/support/everdrive-64/v2x-v3x/
https://github.com/jsdf/loader64
***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_everdrive3.h"


/*==============================
    device_test_everdrive3
    Checks whether the device passed as an argument is EverDrive 3.0
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

bool device_test_everdrive3(ftdi_context_t* cart, int index)
{
    if (strcmp(cart->dev_info[index].Description, "FT245R USB FIFO") == 0 && cart->dev_info[index].ID == 0x04036001){
        char* send_buff = (char*) malloc(sizeof(char) * 512);
        char* recv_buff = (char*) malloc(sizeof(char) * 512);
        char response;
        memset(send_buff, 0, 512);
        memset(recv_buff, 0, 512);  

        // Define the command to send
        send_buff[0]='C';
        send_buff[1]='M';
        send_buff[2]='D';
        send_buff[3]='T'; 

        // Open the device
        pdprint("Opening the device\n", CRDEF_PROGRAM);
        cart->status = FT_Open(index, &cart->handle);
        if(cart->status != FT_OK || !cart->handle)
        {
            free(cart->dev_info);
            terminate("Error: Could not open device.\n");
        }

        // Initialize the USB
        testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
        testcommand(FT_SetTimeouts(cart->handle, 500, 500), "Error: Unable to set flashcart timeouts.\n");
        testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");

        // Send the test command
        testcommand(FT_Write(cart->handle, send_buff, 512, &cart->bytes_written), "Error: Unable to write to flashcart.\n");
        testcommand(FT_Read(cart->handle, recv_buff, 512, &cart->bytes_read), "Error: Unable to read from flashcart.\n");
        testcommand(FT_Close(cart->handle), "Error: Unable to close flashcart.\n");

        // Check if the EverDrive 3.0 responded correctly
        response = recv_buff[3];
        free(send_buff);
        free(recv_buff);

        return response == 'k';
    }
    return false;
}


/*==============================
    device_open_everdrive3
    Opens the USB pipe
    @param A pointer to the cart context
==============================*/

void device_open_everdrive3(ftdi_context_t* cart)
{
    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || !cart->handle)
        terminate("Error: Unable to open flashcart.\n");

    // Reset the cart
    testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
    testcommand(FT_SetTimeouts(cart->handle, 500, 500), "Error: Unable to set flashcart timeouts.\n");
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");
}


/*==============================
    device_sendcmd_everdrive3
    Sends a command to the flashcart
    @param A pointer to the cart context
    @param A char with the command to send
    @param The size of the data to send
    @param The offset of the data to send
==============================*/

void device_sendcmd_everdrive3(ftdi_context_t* cart, char command, int size, char offset)
{
    char* cmd_buffer = (char*) malloc(sizeof(char) * 512);

    // Check we managed to malloc
    if (cmd_buffer == NULL)
        terminate("Error: Unable to allocate memory for buffer.\n");

    // Define the command and send it
    cmd_buffer[0] = 'C';
	cmd_buffer[1] = 'M';
	cmd_buffer[2] = 'D'; 
	cmd_buffer[3] = command;
	cmd_buffer[4] = offset;
	cmd_buffer[5] = 0;	
	cmd_buffer[6] = (char) ((size / 0x200) >> 8);
	cmd_buffer[7] = (char)  (size / 0x200);
    FT_Write(cart->handle, cmd_buffer, 512, &cart->bytes_written);

    // Free the allocated memory
    free(cmd_buffer);
}


/*==============================
    device_sendrom_everdrive3
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
==============================*/

void device_sendrom_everdrive3(ftdi_context_t* cart, FILE *file, u32 size)
{
	int	   bytes_done = 0;
    int	   bytes_left;
	int	   bytes_do;
    char*  rom_buffer = (char*) malloc(sizeof(int) * 32*1024);
    time_t upload_time = clock();

    // Check we managed to malloc
    if (rom_buffer == NULL)
        terminate("Error: Unable to allocate memory for buffer.\n");

    // Get the correctly padded ROM size
    size = calc_padsize(size);
    bytes_left = size;

    // State that we're gonna send
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", 0);

    // Send a command saying we're about to write to the cart
	device_sendcmd_everdrive3(cart, 'W', 0, 0);

    // Upload the ROM
    for ( ; ; )
    {
        int i;

        // Decide how many bytes to send
		if(bytes_left >= 0x8000) 
			bytes_do = 0x8000;
		else
			bytes_do = bytes_left;

        // End if we've got nothing else to send
		if (bytes_do <= 0) 
            break;

        // If the ROM is too small, run the fill command
        if (size < 0x200000)
        {
            char* recv_buff = (char*) malloc(sizeof(char) * 512);
            device_sendcmd_everdrive3(cart, 'F', 0, 0);
            FT_Read(cart->handle, recv_buff, 512, &cart->bytes_read);
            if (recv_buff[3] != 'k')
                terminate("Error: Fill command failed.\n");
            free(recv_buff);
        }

        // If the ROM is larger than 32MB, resend the write command
		if (bytes_done == 0x2000000)
            device_sendcmd_everdrive3(cart, 'W', size-0x2000000, 0x40);

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

			// Send the chunk to RAM. If we reached EOF it doesn't matter what we send
            // TODO: Send 0's when EOF is reached
			fread(rom_buffer, bytes_do, 1, file);
            if (global_z64)
                for (j=0; j<bytes_do; j+=2)
                    SWAP(rom_buffer[j], rom_buffer[j+1]);
			FT_Write(cart->handle, rom_buffer, bytes_do, &cart->bytes_written);

            // If we managed to write, don't try again
			if(cart->bytes_written) 
                break;
		}

        // Check for a timeout
		if(cart->bytes_written == 0) 
        {
            free(rom_buffer);
            terminate("Error: Everdrive timed out.");
        }

         // Keep track of how many bytes were uploaded
		bytes_left -= bytes_do;
		bytes_done += bytes_do;

		// Draw the progress bar
		progressbar_draw("Uploading ROM", (float)bytes_done/size);
    }

    // Send the PIFboot command
    Sleep(500); // Delay is needed or it won't boot properly
    pdprint_replace("Sending pifboot\n", CRDEF_PROGRAM);
    device_sendcmd_everdrive3(cart, 'S', 0, 0);

    // Print that we've finished
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, ((double)(clock()-upload_time))/CLOCKS_PER_SEC);
    free(rom_buffer);
}


/*==============================
    device_close_everdrive3
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_everdrive3(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Error: Unable to close flashcart.\n");
}