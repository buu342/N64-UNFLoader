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

void debug_textinput(ftdi_context_t* cart, WINDOW* inputwin, char* buffer, u16* cursorpos, int ch);
void debug_appendfilesend(char* data, u32 size);
void debug_filesend(char* filename);
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
    outbuff = (char*) malloc(BUFFER_SIZE);
    inbuff = (char*) malloc(BUFFER_SIZE);
    memset(inbuff, 0, BUFFER_SIZE);

    // Open file for debug output
    if (global_debugout != NULL)
    {
        global_debugoutptr = fopen(global_debugout, "w+");
        if (global_debugoutptr == NULL)
        {
            pdprint("\n", CRDEF_ERROR);
            terminate("Unable to open %s for writing debug output.", global_debugout);
        }
    }

    // Decide the alignment based off the cart that's connected
    switch (cart->carttype)
    {
        case CART_EVERDRIVE: alignment = 16; break;
        case CART_SC64: alignment = 4; break;
        default: alignment = 0;
    }

    // Start the debug server loop
    for ( ; ; ) 
	{
        char ch = getch();

        // If ESC is pressed, stop the loop
		if (ch == 27 || (global_timeout != 0 && debugtimeout < clock()))
			break;
        debug_textinput(cart, inputwin, inbuff, &cursorpos, ch);

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
                terminate("Unexpected DMA header: %c %c %c %c.", outbuff[0], outbuff[1], outbuff[2], outbuff[3]);

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
                terminate("Did not receive completion signal: %c %c %c %c.", outbuff[0], outbuff[1], outbuff[2], outbuff[3]);

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
        {
            #ifndef LINUX
                Sleep(10);
            #else
                usleep(10);
            #endif
        }
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
    Handles text input in the console
    @param A pointer to the cart context
    @param A pointer to the input window
    @param A pointer to the input buffer
    @param A pointer to the cursor position value
    @param The inputted character
==============================*/

void debug_textinput(ftdi_context_t* cart, WINDOW* inputwin, char* buffer, u16* cursorpos, int ch)
{
    static int blinker = 0;
    static int size = 0;

    // Decide what to do on key presses
    if ((ch == CH_ENTER || ch == '\r') && size != 0)
    {
        // Check if we're only sending a file or text (and potentially a file appended)
        if (buffer[0] == '@' && buffer[size-1] == '@')
            debug_filesend(buffer);
        else
            debug_appendfilesend(buffer, size+1);
        memset(buffer, 0, BUFFER_SIZE);
        (*cursorpos) = 0;
        size = 0;
    }
    else if (ch == CH_BACKSPACE || ch == 127 || ch == '\b')
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
    pdprintw_nolog(inputwin, buffer, CRDEF_INPUT);

    // Draw the blinker
    blinker = (blinker++) % (1+BLINKRATE * 2);
    if (blinker >= BLINKRATE)
    {
        int x, y;
        getyx(inputwin, y, x);
        mvwaddch(inputwin, y, (*cursorpos), 219);
        wmove(inputwin, y, x);
    }
    werase(inputwin);
}


/*==============================
    debug_appendfilesend
    Sends the data to the flashcart with a file appended if needed
    @param The data to send
    @param The size of the data
==============================*/

void debug_appendfilesend(char* data, u32 size)
{
    char* finaldata; 
    char* filestart = strchr(data, '@');
    if (filestart != NULL)
    {
        FILE *fp;
        int charcount = 0;
        int filesize = 0;
        char sizestring[8];
        char* filepath = (char*)malloc(512);
        char* lastat;
        char* fileend = strchr(++filestart, '@');

        // Ensure we managed to malloc for the filename
        if (filepath == NULL)
        {
            pdprint("Unable to allocate memory for filepath\n", CRDEF_ERROR);
            return;
        }

        // Check if the filepath is valid
        if (fileend == NULL || (*filestart) == '\0')
        {
            pdprint("Missing terminating '@'\n", CRDEF_ERROR);
            free(filepath);
            return;
        }

        // Store the filepath and its character count
        charcount = fileend-filestart;
        strncpy(filepath, filestart, charcount);
        filepath[charcount] = '\0';
        fileend++;

        // Attempt to open the file
        fp = fopen(filepath, "rb+");
        if (fp == NULL)
        {
            pdprint("Unable to open file '%s'\n", CRDEF_ERROR, filepath);
            free(filepath);
            return;
        }

        // Get the filesize
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        rewind(fp);
        sprintf(sizestring, "%d", filesize);

        // Allocate memory for the new data buffer
        size = (size-charcount)+filesize+strlen(sizestring);
        finaldata = (char*)malloc(size+4); // Plus 4 for byte alignment on the 64Drive (so we don't write past the buffer)
        if (finaldata == NULL)
        {
            pdprint("Unable to allocate memory for USB data\n", CRDEF_ERROR);
            free(filepath);
            fclose(fp);
            return;
        }
        memset(finaldata, 0, size+4);

        // Rewrite the data with the new format
        memcpy(finaldata, data, filestart-data);
        strcat(finaldata, sizestring);
        strcat(finaldata, "@");
        lastat = strchr(finaldata, '@')+strlen(sizestring)+2;
        fread(lastat, 1, filesize, fp);
        strcat(lastat+filesize, fileend);

        // Clean up all the memory we borrowed
        fclose(fp);
        free(filepath);
    }
    else
        finaldata = data;

    // Ensure the data isn't too large
    if (size > 0x800000)
    {
        pdprint("Cannot upload data larger than 8MB\n", CRDEF_ERROR);
        if (filestart != NULL)
            free(finaldata);
        return;
    }

    // Send the data to the connected flashcart
    device_senddata(DATATYPE_TEXT, finaldata, size);

    // Free the final data buffer and print success
    if (filestart != NULL)
        free(finaldata);
    pdprint_replace("Sent command '%s'\n", CRDEF_INFO, data);
}


/*==============================
    debug_filesend
    Sends the file to the flashcart
    @param The filename of the file to send wrapped around @ symbols
==============================*/

void debug_filesend(char* filename)
{
    int size;
    FILE *fp;
    char* buffer;
    char* fixed = strtok(filename, "@");

    // Attempt to open the file
    fp = fopen(fixed, "rb+");
    if (fp == NULL)
    {
        pdprint("Unable to open file '%s'\n", CRDEF_ERROR, fixed);
        return;
    }

    // Get the filesize
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    // Ensure the filesize isn't too large
    if (size > 0x800000)
    {
        pdprint("Cannot upload data larger than 8MB\n", CRDEF_ERROR);
        fclose(fp);
        return;
    }

    // Allocate memory for our file buffer
    buffer = (char*)malloc(size+4); // Plus 4 for byte alignment on the 64Drive (so we don't write past the buffer)
    if (buffer == NULL)
    {
        pdprint("Unable to allocate memory for USB data\n", CRDEF_ERROR);
        fclose(fp);
        return;
    }

    // Fill the buffer with file data
    fread(buffer, 1, size, fp);

    // Send the data to the connected flashcart
    device_senddata(DATATYPE_RAWBINARY, buffer, size);
    pdprint_replace("Sent file '%s'\n", CRDEF_INFO, fixed);
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
        default:                  terminate("Unknown data type.");
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
    char* filename = (char*) malloc(PATH_SIZE);
    char* extraname = gen_filename();
    FILE* fp; 

    // Ensure we malloced successfully
    if (filename == NULL || extraname == NULL)
        terminate("Unable to allocate memory for binary file.");

    // Create the binary file to save data to
    memset(filename, 0, PATH_SIZE);
    #ifndef LINUX
        if (global_exportpath != NULL)
                strcat_s(filename, PATH_SIZE, global_exportpath);
        strcat_s(filename, PATH_SIZE, "binaryout-");
        strcat_s(filename, PATH_SIZE, extraname);
        strcat_s(filename, PATH_SIZE, ".bin");
        fopen_s(&fp, filename, "wb+");
    #else
        if (global_exportpath != NULL)
            strcat(filename, global_exportpath);
        strcat(filename, "binaryout-");
        strcat(filename, extraname);
        strcat(filename, ".bin");
        fp = fopen(filename, "wb+");
    #endif

    // Ensure the file was created
    if (fp == NULL)
        terminate("Unable to create binary file.");

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
    char* filename = (char*) malloc(PATH_SIZE);
    char* extraname = gen_filename();

    // Ensure we got a data header of type screenshot
    if (debug_headerdata[0] != DATATYPE_SCREENSHOT)
        terminate("Unexpected data header for screenshot.");

    // Allocate space for the image
    image = (u8*) malloc(4*w*h);

    // Ensure we malloced successfully
    if (filename == NULL || extraname == NULL || image == NULL)
        terminate("Unable to allocate memory for binary file.");

    // Create the binary file to save data to
    memset(filename, 0, PATH_SIZE);
    #ifndef LINUX
        if (global_exportpath != NULL)
            strcat_s(filename, PATH_SIZE, global_exportpath);
        strcat_s(filename, PATH_SIZE, "screenshot-");
        strcat_s(filename, PATH_SIZE, extraname);
        strcat_s(filename, PATH_SIZE, ".png");
    #else
        if (global_exportpath != NULL)
            strcat(filename, global_exportpath);
        strcat(filename, "screenshot-");
        strcat(filename, extraname);
        strcat(filename, ".png");
    #endif

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