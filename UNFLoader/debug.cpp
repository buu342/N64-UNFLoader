/***************************************************************
                           debug.cpp

Handles USB I/O.
***************************************************************/

#include <stdlib.h>
#include "main.h"
#include "helper.h"
#include "device.h"
#include "debug.h"


/*********************************
              Macros
*********************************/

#define BUFFER_SIZE 512


/*********************************
        Function Pointers
*********************************/

void debug_decidedata(ftdi_context_t* cart, u32 info, u8* buffer, u32* read);
void debug_handle_text(ftdi_context_t* cart, u32 size, u8* buffer, u32* read);


/*==============================
    debug_main
    The main debug loop for input/output
    @param A pointer to the cart context
==============================*/

void debug_main(ftdi_context_t *cart)
{
    u8 *buffer;
    DWORD pending = 0;
    pdprint("Debug mode started. Press ESC to stop.\n\n", CRDEF_INPUT);

    // Start the debug server loop
    buffer = malloc(BUFFER_SIZE);
    for ( ; ; ) 
	{
        // If ESC is pressed, stop the loop
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

        // Check if we have pending data
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
        {
            u32 info, read = 0;

            // Ensure we have valid data by reading the header
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            read += cart->bytes_read;
            if (buffer[0] != 'D' || buffer[1] != 'M' || buffer[2] != 'A' || buffer[3] != '@')
                terminate("Error: Unexpected DMA header: %c %c %c %c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

            // Get information about the incoming data
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            read += cart->bytes_read;
            info = swap_endian(buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0]);

            // Decide what to do with the received data
            debug_decidedata(cart, info, buffer, &read);

            // Read the completion signal
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            read += cart->bytes_read;
            if (buffer[0] != 'C' || buffer[1] != 'M' || buffer[2] != 'P' || buffer[3] != 'H')
                terminate("Error: Did not receive completion signal: %c %c %c %c.\n", buffer[0], buffer[1], buffer[2], buffer[3]);

            // The EverDrive 3.0 always sends 512 byte chunks, so ensure you always read 512 bytes
            if (cart->carttype == CART_EVERDRIVE3 && (read % 512) != 0)
            {
                int left = 512 - (read % 512);
                FT_Read(cart->handle, buffer, left, &cart->bytes_read);
            }
        }

        // If we got no more data, sleep a bit to be kind to the CPU
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending == 0)
            Sleep(10);
    }
    free(buffer);
}


/*==============================
    debug_decidedata
    Decides what function to call based on the command type stored in the info
    @param A pointer to the cart context
    @param 4 bytes with the info and size (from the cartridge)
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_decidedata(ftdi_context_t* cart, u32 info, u8* buffer, u32* read)
{
    u8 command = (info >> 24) & 0xFF;
    u32 size = info & 0xFFFFFF;

    // Decide what to do with the data based off the command type
    switch (command)
    {
        case DATATYPE_TEXT: debug_handle_text(cart, size, buffer, read); break;
        default:            terminate("Error: Unknown data type.");
    }
}


/*==============================
    debug_handle_text
    Handles DATATYPE_TEXT
    @param A pointer to the cart context
    @param The size of the incoming data
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_handle_text(ftdi_context_t* cart, u32 size, u8* buffer, u32* read)
{
    int total = 0;
    int left = size;

    // Ensure the data fits within our buffer
    if (left > BUFFER_SIZE)
        left = BUFFER_SIZE;

    // Read bytes until we finished
    while (left != 0)
    {
        // Read from the USB and print it
        FT_Read(cart->handle, buffer, left, &cart->bytes_read);
        pdprint("%s", CRDEF_PRINT, buffer);

        // Store the amount of bytes read
        (*read) += cart->bytes_read;
        total += cart->bytes_read;

        // Ensure the data fits within our buffer
        left = size - total;
        if (left > BUFFER_SIZE)
            left = BUFFER_SIZE;
    }
}