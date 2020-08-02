#include "main.h"
#include "helper.h"
#include "device.h"


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

            // Ensure the command is correct by reading the header
            pdprint("Receiving %d bytes from flashcart.\n", CRDEF_INFO, pending);

            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            if (buffer[0] != 'D' || buffer[1] != 'M' || buffer[2] != 'A' || buffer[3] != '@')
                terminate("Error: Unexpected DMA header: %c %c %c %c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

            // Get the type and size of the incoming data
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            size = buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
            size = swap_endian(size);
            command = (size >> 24) & 0xFF;
            size &= 0xFFFFFF;
            pdprint("Received command %02x with size %d bytes.\n", CRDEF_INFO, command, size);

            // Read the data and print it
            FT_Read(cart->handle, buffer, size, &cart->bytes_read);
            pdprint("%s\n", CRDEF_PRINT, buffer);

            // Read the completion signal
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            if (buffer[0] != 'C' || buffer[1] != 'M' || buffer[2] != 'P' || buffer[3] != 'H')
                terminate("Error: Did not receive completion signal: %c %c %c %c.\n", buffer[0], buffer[1], buffer[2], buffer[3]);
        }

        // If we got no more data, sleep a bit to be kind to the CPU
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
            Sleep(10);
    }
    free(buffer);
}
