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
    char buffer[512];
    DWORD pending = 0;
    u8 block_magic[] = { 'D', 'M', 'A', '@' };
    pdprint("Debug mode started. Press ESC to stop.\n\n", CRDEF_INPUT);

    // Start the debug server loop
    for ( ; ; )
	{
        // If ESC is pressed, stop the loop
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

        // Check if we have pending data
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
        {
            int receive, command, size;

            // Ensure the command is correct by reading the header
            pdprint("Receiving %d bytes from flashcart.\n", CRDEF_INFO, pending);
            FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            pdprint("%c %c %c %c\n", CRDEF_INFO, buffer[0], buffer[1], buffer[2], buffer[3]);
            if (memcmp(buffer, block_magic, 4) != 0) 
                terminate("Error: Unexpected DMA header.\n");
            
            // Get the type and size of the incoming data
			FT_Read(cart->handle, buffer, 4, &cart->bytes_read);
            receive = buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
            receive = swap_endian(receive);

			command = (receive >> 24) & 0xff;
			size = receive & 0xffffff;
            pdprint("Received command %x with size %x.\n", CRDEF_INFO, command, size);
        }

        // If we got no more data, sleep a bit to be kind to the CPU
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
         Sleep(10);
    }
}
