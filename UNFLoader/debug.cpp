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

#define VERBOSE 0
#define BUFFER_SIZE 512
#define BLINKRATE   40


/*********************************
        Function Pointers
*********************************/

void debug_textinput(ftdi_context_t* cart, WINDOW* inputwin, char* buffer, u16* cursorpos);
void debug_decidedata(ftdi_context_t* cart, u32 info, char* buffer, u32* read);
void debug_handle_text(ftdi_context_t* cart, u32 size, char* buffer, u32* read);


/*==============================
    debug_main
    The main debug loop for input/output
    @param A pointer to the cart context
==============================*/

void debug_main(ftdi_context_t *cart)
{
    char *outbuff, *inbuff;
    u16 cursorpos = 0;
    DWORD pending = 0;
    WINDOW* inputwin = newwin(1, getmaxx(stdscr), getmaxy(stdscr)-1, 0);
    int alignment;

    pdprint("Debug mode started. Press ESC to stop.\n\n", CRDEF_INPUT);
    timeout(0);
    curs_set(0);

    // Initialize our buffers
    outbuff = malloc(BUFFER_SIZE);
    inbuff = malloc(BUFFER_SIZE);
    memset(inbuff, 0, BUFFER_SIZE);

    // Decide the alignment based off the cart that's connected
    switch (cart->carttype)
    {
        case CART_EVERDRIVE3: alignment = 512; break;
        case CART_EVERDRIVE7: alignment = 16; break;
        default: alignment = 0;
    }

    // Start the debug server loop
    for ( ; ; ) 
	{
        // If ESC is pressed, stop the loop
		if (GetAsyncKeyState(VK_ESCAPE))
			break;
        debug_textinput(cart, inputwin, inbuff, &cursorpos);

        // Check if we have pending data
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
        {
            u32 info, read = 0;
            #if VERBOSE
                pdprint("Receiving %d bytes\n", CRDEF_INFO, pending);
            #endif

            // Ensure we have valid data by reading the header
            FT_Read(cart->handle, outbuff, 4, &cart->bytes_read);
            read += cart->bytes_read;
            if (outbuff[0] != 'D' || outbuff[1] != 'M' || outbuff[2] != 'A' || outbuff[3] != '@')
                terminate("Error: Unexpected DMA header: %c %c %c %c\n", outbuff[0], outbuff[1], outbuff[2], outbuff[3]);

            // Get information about the incoming data
            FT_Read(cart->handle, outbuff, 4, &cart->bytes_read);
            read += cart->bytes_read;
            info = swap_endian(outbuff[3] << 24 | outbuff[2] << 16 | outbuff[1] << 8 | outbuff[0]);

            // Decide what to do with the received data
            debug_decidedata(cart, info, outbuff, &read);

            // Read the completion signal
            FT_Read(cart->handle, outbuff, 4, &cart->bytes_read);
            read += cart->bytes_read;
            if (outbuff[0] != 'C' || outbuff[1] != 'M' || outbuff[2] != 'P' || outbuff[3] != 'H')
                terminate("Error: Did not receive completion signal: %c %c %c %c.\n", outbuff[0], outbuff[1], outbuff[2], outbuff[3]);

            // Ensure byte alignment by reading X amount of bytes needed
            if (alignment != 0 && (read % alignment) != 0)
            {
                int left = alignment - (read % alignment);
                FT_Read(cart->handle, outbuff, left, &cart->bytes_read);
            }
        }

        // If we got no more data, sleep a bit to be kind to the CPU
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending == 0)
            Sleep(10);
    }

    // Clean up everything
    free(outbuff);
    free(inbuff);
    wclear(inputwin);
    wrefresh(inputwin);
    delwin(inputwin);
    curs_set(0);
}


/*==============================
    debug_textinput
    TODO: Write this header
==============================*/

void debug_textinput(ftdi_context_t* cart, WINDOW* inputwin, char* buffer, u16* cursorpos)
{
    static int blinker = 0;
    static int size = 0;
    int ch = getch();

    // Decide what to do on key presses
    if ((ch == KEY_ENTER || ch == '\n' || ch == '\r') && size != 0)
    {
        pdprint("Sending command %s\n", CRDEF_INFO, buffer);
        device_senddata(buffer, size);
        memset(buffer, 0, BUFFER_SIZE);
        (*cursorpos) = 0;
        size = 0;
    }
    else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
    {
        if ((*cursorpos) > 0)
        {
            buffer[--(*cursorpos)] = 0;
            size--;
        }
    }
    else if (ch != ERR && isascii(ch) && ch > 0x1F)
    {
        buffer[(*cursorpos)++] = ch;
        size++;
    }

    // Display what we've written
    pdprintw(inputwin, buffer, CRDEF_INPUT);

    // Draw the blinker
    blinker = (blinker++) % (1+BLINKRATE * 2);
    if (blinker >= BLINKRATE)
    {
        int x, y;
        getyx(inputwin, y, x);
        mvwaddch(inputwin, y, (*cursorpos), 219);
        wmove(inputwin, y, x);
    }

    // Refresh the input window
    wrefresh(inputwin);
    wclear(inputwin);
}


/*==============================
    debug_decidedata
    Decides what function to call based on the command type stored in the info
    @param A pointer to the cart context
    @param 4 bytes with the info and size (from the cartridge)
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_decidedata(ftdi_context_t* cart, u32 info, char* buffer, u32* read)
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

void debug_handle_text(ftdi_context_t* cart, u32 size, char* buffer, u32* read)
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