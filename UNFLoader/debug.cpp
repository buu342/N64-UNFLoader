/***************************************************************
                           debug.cpp

Handles USB I/O.
***************************************************************/

#include "Include/lodepng.h"
#include "main.h"
#include "helper.h"
#include "device.h"
#include "debug.h"


/*********************************
              Macros
*********************************/

#define VERBOSE     0
#define BUFFER_SIZE 512
#define HEADER_SIZE 16
#define BLINKRATE   40
#define PATH_SIZE   256


/*********************************
       Function Prototypes
*********************************/

void debug_textinput(ftdi_context_t* cart, WINDOW* inputwin, char* buffer, u16* cursorpos);
void debug_decidedata(ftdi_context_t* cart, u32 info, char* buffer, u32* read);
void debug_handle_text(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_rawbinary(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_header(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_screenshot(ftdi_context_t* cart, u32 size, char* buffer, u32* read);


/*********************************
         Global Variables
*********************************/

static int debug_headerdata[HEADER_SIZE];


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
    time_t debugtimeout = clock() + global_timeout*CLOCKS_PER_SEC;
    int alignment;

    pdprint("Debug mode started. Press ESC to stop.", CRDEF_INPUT);
    if (global_timeout != 0)
        pdprint(" Timeout after %d seconds.", CRDEF_INPUT, global_timeout);
    pdprint("\n\n", CRDEF_INPUT);
    timeout(0);
    curs_set(0);

    // Initialize our buffers
    outbuff = malloc(BUFFER_SIZE);
    inbuff = malloc(BUFFER_SIZE);
    memset(inbuff, 0, BUFFER_SIZE);

    // Open file for debug output
    if (global_debugout != NULL)
    {
        fopen_s(&global_debugoutptr, global_debugout, "w+");
        if (global_debugoutptr == NULL)
            terminate("\nError: Unable to open %s for writing debug output.", global_debugout);
    }

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
		if (GetAsyncKeyState(VK_ESCAPE) || debugtimeout < clock())
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

            // Reset the timeout clock
            debugtimeout = clock() + global_timeout*CLOCKS_PER_SEC;
        }

        // If we got no more data, sleep a bit to be kind to the CPU
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending == 0)
            Sleep(10);
    }

    // Close the debug output file if it exists
    if (global_debugoutptr != NULL)
    {
        fclose(global_debugoutptr);
        global_debugoutptr = NULL;
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
        case DATATYPE_TEXT:       debug_handle_text(cart, size, buffer, read); break;
        case DATATYPE_RAWBINARY:  debug_handle_rawbinary(cart, size, buffer, read); break;
        case DATATYPE_HEADER:     debug_handle_header(cart, size, buffer, read); break;
        case DATATYPE_SCREENSHOT: debug_handle_screenshot(cart, size, buffer, read); break;
        default:                  terminate("Error: Unknown data type.");
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


/*==============================
    debug_handle_rawbinary
    Handles DATATYPE_RAWBINARY
    @param A pointer to the cart context
    @param The size of the incoming data
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_handle_rawbinary(ftdi_context_t* cart, u32 size, char* buffer, u32* read)
{
    int total = 0;
    int left = size;
    char* filename = malloc(PATH_SIZE);
    char* extraname = gen_filename();
    FILE* fp; 

    // Ensure we malloced successfully
    if (filename == NULL || extraname == NULL)
        terminate("Error: Unable to allocate memory for binary file.");

    // Create the binary file to save data to
    memset(filename, 0, PATH_SIZE);
    if (global_exportpath != NULL)
        strcat_s(filename, PATH_SIZE, global_exportpath);
    strcat_s(filename, PATH_SIZE, "binaryout-");
    strcat_s(filename, PATH_SIZE, extraname);
    strcat_s(filename, PATH_SIZE, ".bin");
    fopen_s(&fp, filename, "wb+");

    // Ensure the file was created
    if (fp == NULL)
        terminate("Error: Unable to create binary file.");

    // Ensure the data fits within our buffer
    if (left > BUFFER_SIZE)
        left = BUFFER_SIZE;

    // Read bytes until we finished
    while (left != 0)
    {
        // Read from the USB and save it to our binary file
        FT_Read(cart->handle, buffer, left, &cart->bytes_read);
        fwrite(buffer, 1, left, fp);

        // Store the amount of bytes read
        (*read) += cart->bytes_read;
        total += cart->bytes_read;

        // Ensure the data fits within our buffer
        left = size - total;
        if (left > BUFFER_SIZE)
            left = BUFFER_SIZE;
    }

    // Close the file and free the memory used for the filename
    pdprint("Wrote %d bytes to %s.\n", CRDEF_INFO, size, filename);
    fclose(fp);
    free(filename);
    free(extraname);
}


/*==============================
    debug_handle_header
    Handles DATATYPE_HEADER
    @param A pointer to the cart context
    @param The size of the incoming data
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_handle_header(ftdi_context_t* cart, u32 size, char* buffer, u32* read)
{
    int total = 0;
    int left = size;

    // Ensure the data fits within our buffer
    if (left > HEADER_SIZE)
        left = HEADER_SIZE;

    // Read bytes until we finished
    while (left != 0)
    {
        // Read from the USB and save it to the global headerdata
        FT_Read(cart->handle, buffer, left, &cart->bytes_read);
        for (int i=0; i<(int)cart->bytes_read; i+=4)
            debug_headerdata[i/4] = swap_endian(buffer[i + 3] << 24 | buffer[i + 2] << 16 | buffer[i + 1] << 8 | buffer[i]);

        // Store the amount of bytes read
        (*read) += cart->bytes_read;
        total += cart->bytes_read;

        // Ensure the data fits within our buffer
        left = size - total;
        if (left > HEADER_SIZE)
            left = HEADER_SIZE;
    }
}


/*==============================
    debug_handle_screenshot
    Handles DATATYPE_SCREENSHOT
    @param A pointer to the cart context
    @param The size of the incoming data
    @param The buffer to use
    @param A pointer to a variable that stores the number of bytes read
==============================*/

void debug_handle_screenshot(ftdi_context_t* cart, u32 size, char* buffer, u32* read)
{
    int total = 0;
    int left = size;
    int j=0;
    u8* image;
    int w = debug_headerdata[2], h = debug_headerdata[3];
    char* filename = malloc(PATH_SIZE);
    char* extraname = gen_filename();

    // Ensure we got a data header of type screenshot
    if (debug_headerdata[0] != DATATYPE_SCREENSHOT)
        terminate("Error: Unexpected data header for screenshot.");

    // Allocate space for the image
    image = malloc(4*w*h);

    // Ensure we malloced successfully
    if (filename == NULL || extraname == NULL || image == NULL)
        terminate("Error: Unable to allocate memory for binary file.");

    // Create the binary file to save data to
    memset(filename, 0, PATH_SIZE);
    if (global_exportpath != NULL)
        strcat_s(filename, PATH_SIZE, global_exportpath);
    strcat_s(filename, PATH_SIZE, "screenshot-");
    strcat_s(filename, PATH_SIZE, extraname);
    strcat_s(filename, PATH_SIZE, ".png");

    // Ensure the data fits within our buffer
    if (left > BUFFER_SIZE)
        left = BUFFER_SIZE;

    // Read bytes until we finished
    while (left != 0)
    {
        // Read from the USB and save it to our binary file
        FT_Read(cart->handle, buffer, left, &cart->bytes_read);
        for (int i=0; i<(int)cart->bytes_read; i+=4)
        {
            int texel = swap_endian((buffer[i+3]<<24)&0xFF000000 | (buffer[i+2]<<16)&0xFF0000 | (buffer[i+1]<<8)&0xFF00 | buffer[i]&0xFF);
            if (debug_headerdata[1] == 2) 
            {
                short pixel1 = texel >> 16;
                short pixel2 = texel;
                image[j++] = 0x08*((pixel1>>11) & 0x001F); // R1
                image[j++] = 0x08*((pixel1>>6) & 0x001F);  // G1
                image[j++] = 0x08*((pixel1>>1) & 0x001F);  // B1
                image[j++] = 0xFF;

                image[j++] = 0x08*((pixel2>>11) & 0x001F); // R2
                image[j++] = 0x08*((pixel2>>6) & 0x001F);  // G2
                image[j++] = 0x08*((pixel2>>1) & 0x001F);  // B2
                image[j++] = 0xFF;
            }
            else
            {
                // TODO: Test this because I sure as hell didn't >:V
                image[j++] = (texel>>24) & 0xFF; // R
                image[j++] = (texel>>16) & 0xFF; // G
                image[j++] = (texel>>8)  & 0xFF; // B
                image[j++] = (texel>>0)  & 0xFF; // Alpha
            }
        }

        // Store the amount of bytes read
        (*read) += cart->bytes_read;
        total += cart->bytes_read;

        // Ensure the data fits within our buffer
        left = size - total;
        if (left > BUFFER_SIZE)
            left = BUFFER_SIZE;
    }

    // Close the file and free the dynamic memory used
    lodepng_encode32_file(filename, image, w, h);
    pdprint("Wrote %dx%d pixels to %s.\n", CRDEF_INFO, w, h, filename);
    free(image);
    free(filename);
    free(extraname);
}