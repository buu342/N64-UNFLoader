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

void device_sendcmd_64drive(ftdi_context_t* cart, u8 command, bool reply, u8 numparams, ...);


/*==============================
    device_test_64drive1
    Checks whether the device passed as an argument is 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
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
        terminate("Error: Unable to open flashcart.\n");

    // Reset the cart and set its timeouts
    testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
    testcommand(FT_SetTimeouts(cart->handle, 5000, 5000), "Error: Unable to set flashcart timeouts.\n");

    // If the cart is in synchronous mode, enable the bits
    if (cart->synchronous)
    {
        testcommand(FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_RESET), "Error: Unable to set bitmode %d.\n", FT_BITMODE_RESET);
        testcommand(FT_SetBitMode(cart->handle, 0xff, FT_BITMODE_SYNC_FIFO), "Error: Unable to set bitmode %d.\n", FT_BITMODE_SYNC_FIFO);
    }

    // Purge USB contents
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");
}


/*==============================
    device_sendcmd_64drive
    Opens the USB pipe
    @param A pointer to the cart context
    @param The command to send
    @param A bool stating whether a reply should be expected
    @param The number of extra params to send
    @param The extra variadic commands to send
==============================*/

void device_sendcmd_64drive(ftdi_context_t* cart, u8 command, bool reply, u8 numparams, ...)
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
	if(numparams > 0) 
        *(u32 *)&send_buff[4] = swap_endian(va_arg(params, u32));
	if(numparams > 1) 
        *(u32 *)&send_buff[8] = swap_endian(va_arg(params, u32));
    va_end(params);

    // Write to the cart
    testcommand(FT_Write(cart->handle, send_buff, 4+(numparams*4), &cart->bytes_written), "Error: Unable to write to 64Drive.\n"); 
    if (cart->bytes_written == 0)
        terminate("Error: No bytes were written to 64Drive.\n");

    // If the command expects a response
    if (reply)
    {
        // These two instructions do not return a success, so ignore them
	    if(command == DEV_CMD_PI_WR_BL || command == DEV_CMD_PI_WR_BL_LONG) 
            return;
    		
        // Check that we received the signal that the operation completed
        testcommand(FT_Read(cart->handle, recv_buff, 4, &cart->bytes_read), "Error: Unable to read completion signal.\n");
	    recv_buff[1] = command << 24 | 0x504D43;
	    if (memcmp(recv_buff, &recv_buff[1], 4) != 0)
            terminate("Error: Did not receive completion signal.\n");
    }
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
    
    // Check we managed to malloc
    if (rom_buffer == NULL)
        terminate("Error: Unable to allocate memory for buffer.\n");

    // If the CIC argument was provided
    if (global_cictype != 0 && cart->cictype == 0)
    {
        int cic = -1;

        // Get the CIC key
        switch(global_cictype)
        {
            case 6101: cic = 0; break;
            case 6102: cic = 1; break;
            case 7101: cic = 2; break;
            case 7102: cic = 3; break;
            case 6103:
            case 7103: cic = 4; break;
            case 6105:
            case 7105: cic = 5; break;
            case 6106:
            case 7106: cic = 6; break;
            case 5101: cic = 7; break;
            default: terminate("Unknown CIC type '%d'.\n", global_cictype);
        }

        // Set the CIC
        cart->cictype = global_cictype;
        device_sendcmd_64drive(cart, DEV_CMD_SETCIC, false, 1, (1 << 31) | cic, 0);
        pdprint("CIC set to %d.\n", CRDEF_PROGRAM, global_cictype);
    }
    
    // Set Savetype
    if (global_savetype != 0)
    {
        device_sendcmd_64drive(cart, DEV_CMD_SETSAVE, false, 1, global_savetype, 0);
        pdprint("Changed save type to %d.\n", CRDEF_PROGRAM, global_cictype);
    }

    // Decide a better, more optimized chunk size
	if(size > 16 * 1024 * 1024) 
		chunk = 32;
	else if( size > 2 * 1024 * 1024)
		chunk = 16;
	else 
		chunk = 4;
	chunk *= 128 * 1024; // Convert to megabytes

    // Send chunks to the cart
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", 0);
    for ( ; ; )
    {
        int i;

        // Decide how many bytes to send
		if(bytes_left >= chunk) 
			bytes_do = chunk;
		else
			bytes_do = bytes_left;

        // If we have an uneven number of bytes, fix that
		if (bytes_do%4 != 0) 
			bytes_do -= bytes_do%4;

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

			// Send the chunk to RAM
			device_sendcmd_64drive(cart, DEV_CMD_LOADRAM, false, 2, ram_addr, (bytes_do & 0xffffff) | 0 << 24);
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
            terminate("Error: 64Drive timed out");
        }

		// Ignore the success response
		cart->status = FT_Read(cart->handle, rom_buffer, 4, &cart->bytes_read);

        // Keep track of how many bytes were uploaded
		bytes_left -= bytes_do;
		bytes_done += bytes_do;
		ram_addr += bytes_do;

		// Draw the progress bar
		progressbar_draw("Uploading ROM", (float)bytes_done/size);
	}

    // Print that we've finished
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, ((double)(clock()-upload_time))/CLOCKS_PER_SEC);
    free(rom_buffer);

    // I'm supposed to read a reply from the 64Drive, but because it's unreliable in listen mode I'm just gonna purge instead
    FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX);
}


/*==============================
    device_close_64drive
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_64drive(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Error: Unable to close flashcart.\n");
}