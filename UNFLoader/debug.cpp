#include <stdlib.h>
#include "main.h"
#include "helper.h"
#include "device.h"
#include "debug.h"


/*==============================
    debug_main
    The main debug loop for input/output
    @param A pointer to the cart context
==============================*/

void debug_main(ftdi_context_t *cart)
{
    char *buffer;
    DWORD pending = 0;
    pdprint("Debug mode started. Press ESC to stop.\n\n", CRDEF_INPUT);

    // Start the debug server loop
    buffer = malloc(512);
    for ( ; ; ) 
	{
        // If ESC is pressed, stop the loop
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

        // Check if we have pending data
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
        {
            u8 command;
            u32 size;
            int read = 0;

            // Ensure we have valid data by reading the header
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            read += cart->bytes_read;
            if (buffer[0] != 'D' || buffer[1] != 'M' || buffer[2] != 'A' || buffer[3] != '@')
                terminate("Error: Unexpected DMA header: %c %c %c %c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

            // Get the command type and size of the incoming data
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            read += cart->bytes_read;
            size = buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
            size = swap_endian(size);
            command = (size >> 24) & 0xFF;
            size &= 0xFFFFFF;

            // Decide what to do with the data based off the command type
            switch (command)
            {
                case DATATYPE_TEXT:
                    FT_Read(cart->handle, buffer, size, &cart->bytes_read);
                    pdprint("%s", CRDEF_PRINT, buffer);
                    read += cart->bytes_read;
                    break;
                default:
                    terminate("Error: Unknown data type.");
            }

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
        if (pending > 0)
            Sleep(10);
    }
    free(buffer);
}
