/***************************************************************
                           debug.cpp

Handles USB I/O.
***************************************************************/

#include <limits.h>
#pragma warning(push, 0)
    #include "Include/lodepng.h"
#pragma warning(pop)
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
#define BLINKRATE   0.5
#define PATH_SIZE   256
#define HISTORY_SIZE 100


/*********************************
       Function Prototypes
*********************************/

void debug_textinput(WINDOW* inputwin, char* buffer, u16* cursorpos, int ch);
void debug_appendfilesend(char* data, u32 size);
void debug_filesend(const char* filename);
void debug_decidedata(ftdi_context_t* cart, u32 info, char* buffer, u32* read);
void debug_handle_text(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_rawbinary(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_header(ftdi_context_t* cart, u32 size, char* buffer, u32* read);
void debug_handle_screenshot(ftdi_context_t* cart, u32 size, char* buffer, u32* read);


/*********************************
         Global Variables
*********************************/

static int debug_headerdata[HEADER_SIZE];
static char** cmd_history;
static int cmd_count = 0;
static int   print_stackcount = 0;
static char* print_lastmessage = NULL;


/*==============================
    debug_main
    The main debug loop for input/output
    @param A pointer to the cart context
==============================*/

void debug_main(ftdi_context_t *cart)
{
    int i;
    int alignment;
    char *outbuff, *inbuff;
    u16 cursorpos = 0;
    DWORD pending = 0;
    WINDOW* inputwin = newwin(1, global_termsize[1], global_termsize[0]-1, 0);

    // Check if this cart supports debug mode
    if (!device_testdebug())
    {
        pdprint("Unable to start debug mode.\n\n", CRDEF_ERROR);
        return;
    }

    // Initialize debug mode keyboard input
    if (global_timeout != 0)
        pdprint("Debug mode started. Press ESC to stop or wait for timeout.\n\n", CRDEF_INPUT, global_timeout);
    else
        pdprint("Debug mode started. Press ESC to stop.\n\n", CRDEF_INPUT);
    timeout(0);
    curs_set(0);
    keypad(inputwin, TRUE);

    // Initialize our buffers
    outbuff = (char*) malloc(BUFFER_SIZE);
    inbuff = (char*) malloc(BUFFER_SIZE);
    if (cmd_history == NULL)
    {
        cmd_history = (char**) malloc(HISTORY_SIZE*sizeof(char*));
        for (i=0; i<HISTORY_SIZE; i++)
            cmd_history[i] = (char*) malloc(BUFFER_SIZE);
    }
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
        default: alignment = 0;
    }

    // Start the debug server loop
    for ( ; ; ) 
	{
        int ch = getch();

        // If ESC is pressed, stop the loop
		if (ch == 27 || (global_timeout != 0 && global_timeouttime < time(NULL)))
			break;
        debug_textinput(inputwin, inbuff, &cursorpos, ch);

        // Check if we have pending data
        FT_GetQueueStatus(cart->handle, &pending);
        if (pending > 0)
        {
            u32 info, read = 0;
            #if VERBOSE
                pdprint("Receiving %d bytes\n", CRDEF_INFO, pending);
            #endif

            // SC64 doesn't follow same debug protocol as 64drive/Everdrive
            if (cart->carttype == CART_SC64)
            {
                u32 packet_size;

                // Ensure we have valid data by reading the header
                FT_Read(cart->handle, outbuff, 4, &cart->bytes_read);
                read += cart->bytes_read;
                if (outbuff[0] != 'P' || outbuff[1] != 'K' || outbuff[2] != 'T' || outbuff[3] != 'U')
                    terminate("Unexpected PKT header: %c %c %c %c.", outbuff[0], outbuff[1], outbuff[2], outbuff[3]);

                // Get packet size
                FT_Read(cart->handle, &packet_size, 4, &cart->bytes_read);
                read += cart->bytes_read;
                packet_size = swap_endian(packet_size);

                // Get information about the incoming data
                FT_Read(cart->handle, &info, 4, &cart->bytes_read);
                read += cart->bytes_read;
                info = swap_endian(info);

                // Check if packet size matches data length specified in information
                if (packet_size != ((info & 0xFFFFFF) + 4))
                    terminate("Packet size doesn't match size specified in header");

                // Decide what to do with the received data
                debug_decidedata(cart, info, outbuff, &read);

                // Check if debug_decidedata consumed whole packet
                if (read != (packet_size + 8))
                    terminate("Function debug_decidedate didn't consume whole packet: %d != %d", read, (packet_size + 8));
            }
            else
            {
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
            }
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
    if (print_lastmessage != NULL)
        free(print_lastmessage);

    print_stackcount = 0;
    print_lastmessage = NULL;
    global_scrolling = false;
    wclear(inputwin);
    wrefresh(inputwin);
    delwin(inputwin);
    curs_set(0);
}


/*==============================
    debug_putconsole
    Puts text on the console, stacking if necessary
    @param The message to print
    @param The color to use
    @param The size of the string we're printing
    @param Whether to replace the previous line or not
==============================*/

static void debug_putconsole(char* printmessage, short color, u32 strsize, bool replace)
{
    // Stack duplicate prints
    if (global_stackprints)
    {
        // If we received the same message, then increment the stack count and print that it was duplicated
        if (print_lastmessage != NULL && !strcmp(printmessage, print_lastmessage))
        {
            print_stackcount++;
            if (print_stackcount == 1)
                pdprint("Previous message duplicated 1 time(s)\n", CRDEF_INFO);
            else
                pdprint_replace("Previous message duplicated %d time(s)\n", CRDEF_INFO, print_stackcount);
            return;
        }
        else // Otherwise, store the new line
        {
            print_stackcount = 0;
            if (print_lastmessage != NULL)
                free(print_lastmessage);
            print_lastmessage = (char*)malloc(sizeof(char) * strsize);
            strcpy(print_lastmessage, printmessage);
        }
    }

    // Print the text
    if (replace)
        pdprint_replace("%s", color, printmessage);
    else
        pdprint("%s", color, printmessage);
}


/*==============================
    debug_clearconsolestack
    Clear's the console message duplicate memory to prevent stacking
==============================*/

static void debug_clearconsolestack()
{
    if (print_lastmessage != NULL)
        free(print_lastmessage);
    print_stackcount = 0;
    print_lastmessage = NULL;
}


/*==============================
    debug_textinput
    Handles text input in the console
    @param A pointer to the input window
    @param A pointer to the input buffer
    @param A pointer to the cursor position value
    @param The inputted character
==============================*/

void debug_textinput(WINDOW* inputwin, char* buffer, u16* cursorpos, int ch)
{
    char refresh = false;
    char cmd_changed = 0;
    static char blinkerstate = 1;
    static clock_t blinkertime = 0;
    static int size = 0;
    static int curcmd = 0;

    // Handle arrow key presses
    if (ch == KEY_DOWN)
    {
        curcmd++;
        if (curcmd > cmd_count)
            curcmd = 0;
        cmd_changed = 1;
        refresh = true;
    }
    else if (ch == KEY_UP)
    {
        curcmd--;
        if (curcmd < 0)
            curcmd = cmd_count;
        cmd_changed = 1;
        refresh = true;
    }
    else if (ch == KEY_LEFT && (*cursorpos) > 0)
    {
        (*cursorpos)--;
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }
    else if (ch == KEY_RIGHT && (*cursorpos) < size)
    {
        (*cursorpos)++;
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }

    // Handle the scrolling keys
    if (ch == KEY_PPAGE)
        scrollpad(-1);
    else if(ch == KEY_NPAGE)
        scrollpad(1);
    else if (ch == KEY_END)
        scrollpad(INT_MAX);
    else if (ch == KEY_HOME)
        scrollpad(INT_MIN);

    // If the up or down arrow was pressed
    if (cmd_changed)
    {
        if (curcmd == 0)
        {
            memset(buffer, 0, BUFFER_SIZE);
            (*cursorpos) = 0;
            size = 0;
        }
        else
        {
            int slen = strlen(cmd_history[curcmd-1]);
            strcpy(buffer, cmd_history[curcmd-1]);
            (*cursorpos) = (u16)slen;
            size = slen;
        }
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
    }

    // Decide what to do on other key presses
    if ((ch == CH_ENTER || ch == '\r') && size != 0)
    {
        if (size >= BUFFER_SIZE-1)
            size = BUFFER_SIZE-1;
        buffer[size] = '\0';

        // Check if we're only sending a file or text (and potentially a file appended)
        if (buffer[0] == '@' && buffer[size-1] == '@')
            debug_filesend(buffer);
        else
            debug_appendfilesend(buffer, size+1);

        // Add the command to the command history
        if (curcmd == 0)
        {
            cmd_count++;
            if (cmd_count >= HISTORY_SIZE)
                cmd_count = HISTORY_SIZE;
            strcpy(cmd_history[cmd_count-1], buffer);
        }
        curcmd = 0;

        // Clear the input bar and reset the cursor position
        memset(buffer, 0, BUFFER_SIZE);
        (*cursorpos) = 0;
        size = 0;
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }
    else if (ch == CH_BACKSPACE || ch == 263)
    {
        if ((*cursorpos) > 0)
        {
            int i;

            // Shift any characters in front of the cursor backwards
            for (i=(*cursorpos); i<size; i++)
                buffer[i-1] = buffer[i];

            // Remove a character from our input buffer
            buffer[size] = 0;
            size--;
            buffer[size] = 0;
            (*cursorpos)--;
            curcmd = 0;
            blinkerstate = 1;
            blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
            refresh = true;
        }
    }
    else if (ch == KEY_DC && (*cursorpos) != size) // DEL key
    {
        int i;

        // Shift any characters in front of the cursor backwards
        for (i=(*cursorpos); i<size; i++)
            buffer[i] = buffer[i+1];

        // Remove a character from our input buffer
        buffer[size] = 0;
        size--;
        buffer[size] = 0;
        curcmd = 0;
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }
    else if (ch != ERR && isascii(ch) && ch > 0x1F && size < BUFFER_SIZE)
    {
        int i;

        // Shift any characters in front of the cursor forwards
        if ((*cursorpos) != size)
            for (i=size; i>=(*cursorpos); i--)
                buffer[i+1] = buffer[i];

        // Add the character to the input buffer
        buffer[(*cursorpos)++] = (char)ch;
        size++;
        curcmd = 0;
        blinkerstate = 1;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }

    // Display what we've written
    werase(inputwin);
    pdprintw_nolog(inputwin, buffer, CRDEF_INPUT);
    
    // Draw the blinker
    if (blinkertime < clock())
    {
        blinkerstate = !blinkerstate;
        blinkertime = clock() + (clock_t)((float)CLOCKS_PER_SEC*BLINKRATE);
        refresh = true;
    }
    if (blinkerstate)
    {
        int x, y;
        getyx(inputwin, y, x);
        mvwaddch(inputwin, y, (*cursorpos), ACS_BLOCK);
        wmove(inputwin, y, x);
    }
    
    // Refresh the input bar window
    if (refresh)
        wrefresh(inputwin);
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
        char* filepath = (char*)malloc(BUFFER_SIZE);
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
    debug_clearconsolestack();
}


/*==============================
    debug_filesend
    Sends the file to the flashcart
    @param The filename of the file to send wrapped around @ symbols
==============================*/

void debug_filesend(const char* filename)
{
    int size;
    FILE *fp;
    char* buffer;
    char* copy = (char*)malloc(strlen(filename));
    char* fixed;
    
    // Make a copy of the filename string because strtok modifies it
    if (copy == NULL)
        terminate("Unable to allocate memory for filename");
    strcpy(copy, filename);
    fixed = strtok(copy, "@");

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
    debug_clearconsolestack();
    free(buffer);
    free(copy);
    fclose(fp);
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
    char* printmessage = (char*)malloc(sizeof(char)*size);

    // Ensure the data fits within our buffer
    if (left > BUFFER_SIZE)
        left = BUFFER_SIZE;

    // Read bytes until we finished
    while (left != 0)
    {
        // Read from the USB and print it
        FT_Read(cart->handle, buffer, left, &cart->bytes_read);
        sprintf(printmessage + total, "%.*s", cart->bytes_read, buffer);

        // Store the amount of bytes read
        (*read) += cart->bytes_read;
        total += cart->bytes_read;

        // Ensure the data fits within our buffer
        left = size - total;
        if (left > BUFFER_SIZE)
            left = BUFFER_SIZE;
    }

    // Print the text to the console
    debug_putconsole(printmessage, CRDEF_PRINT, size, false);
    free(printmessage);
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
    debug_clearconsolestack();
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
                short pixel1 = (texel&0xFFFF0000)>>16;
                short pixel2 = (texel&0x0000FFFF);
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
    debug_clearconsolestack();
    free(image);
    free(filename);
    free(extraname);
}