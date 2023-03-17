#include "debug.h"
#include "term.h"
#include "helper.h"
#pragma warning(push, 0)
    #include "Include/lodepng.h"
#pragma warning(pop)
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#ifndef LINUX
    #include <shlwapi.h>
#endif
#include <list>
#include <queue>
#include <iterator>


/*********************************
              Macros
*********************************/

#define HEADER_SIZE 16
#define PATH_SIZE 512


/*********************************
            Structures
*********************************/

typedef struct {
    byte*       data;
    USBDataType type;
    int32_t     size;
} SendData;

typedef struct {
    char*    str;
    uint32_t strsize;
    byte*    data;
    uint32_t datasize;
    bool     ispath;
} ParseHelper;


/*********************************
        Function Prototypes
*********************************/

static void debug_handle_text(uint32_t size, byte* buffer);
static void debug_handle_rawbinary(uint32_t size, byte* buffer);
static void debug_handle_header(uint32_t size, byte* buffer);
static void debug_handle_screenshot(uint32_t size, byte* buffer);


/*********************************
             Globals
*********************************/

// Output file paths
static FILE* local_debugoutfile = NULL;
static char* local_binaryoutfolderpath = NULL;

// Other
static int debug_headerdata[HEADER_SIZE];
static std::queue<SendData*> local_mesgqueue;


/*==============================
    debug_main
    Handles debug I/O
==============================*/

void debug_main()
{
    byte*    outbuff = NULL;
    uint32_t dataheader = 0;

    // Send data to USB if it exists
    while (!local_mesgqueue.empty())
    {
        SendData* msg = local_mesgqueue.front();
        handle_deviceerror(device_senddata(msg->type, msg->data, msg->size));

        // Cleanup
        local_mesgqueue.pop();
        free(msg->data);
        free(msg);
    }

    // Read from USB
    do
    {
        handle_deviceerror(device_receivedata(&dataheader, &outbuff));
        if (dataheader != 0 && outbuff != NULL)
        {
            uint32_t size = dataheader & 0xFFFFFF;
            USBDataType command = (USBDataType)((dataheader >> 24) & 0xFF);

            // Decide what to do with the data based off the command type
            switch (command)
            {
                case DATATYPE_TEXT:       debug_handle_text(size, outbuff); break;
                case DATATYPE_RAWBINARY:  debug_handle_rawbinary(size, outbuff); break;
                case DATATYPE_HEADER:     debug_handle_header(size, outbuff); break;
                case DATATYPE_SCREENSHOT: debug_handle_screenshot(size, outbuff); break;
                default:                  terminate("Unknown data type '%x'.", (uint32_t)command);
            }

            // Cleanup
            free(outbuff);
            outbuff = NULL;
        }
    }
    while (dataheader > 0);
}


/*==============================
    debug_handle_text
    Handles DATATYPE_TEXT
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

static void debug_handle_text(uint32_t size, byte* buffer)
{
    char* text;
    text = (char*)malloc(size+1);
    if (text == NULL)
        terminate("Failed to allocate memory for incoming string.");
    memset(text, 0, size+1);
    strncpy(text, (char*)buffer, size);
    log_stackable("%s", CRDEF_PRINT, text);
    free(text);
}


/*==============================
    debug_handle_rawbinary
    Handles DATATYPE_RAWBINARY
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

static void debug_handle_rawbinary(uint32_t size, byte* buffer)
{
    FILE* fp = NULL;
    char* filename = gen_filename("binaryout", "bin");

    // Ensure we malloced successfully
    if (filename == NULL)
        terminate("Unable to allocate memory for binary file path.");

    // Create the binary file to save data to
    fp = fopen(filename, "wb+");

    // Ensure the file was created
    if (fp == NULL)
        terminate("Unable to create binary file.");

    // Write the data to the file
    fwrite(buffer, 1, size, fp);
    log_colored("Wrote %d bytes to '%s'.\n", CRDEF_INFO, size, filename);

    // Cleanup
    fclose(fp);
    free(filename);
}


/*==============================
    debug_handle_header
    Handles DATATYPE_HEADER
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

static void debug_handle_header(uint32_t size, byte* buffer)
{
    // Ensure the data fits within our buffer
    if (size > HEADER_SIZE)
        size = HEADER_SIZE;

    // Read bytes until we finished
    for (uint32_t i=0; i<size; i+=4)
        debug_headerdata[i/4] = swap_endian(buffer[i + 3] << 24 | buffer[i + 2] << 16 | buffer[i + 1] << 8 | buffer[i]);
}


/*==============================
    debug_handle_screenshot
    Handles DATATYPE_SCREENSHOT
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

static void debug_handle_screenshot(uint32_t size, byte* buffer)
{
    byte*    image = NULL;
    uint32_t written = 0;
    uint32_t w = debug_headerdata[2];
    uint32_t h = debug_headerdata[3];
    char*    filename = gen_filename("screenshot", "png");

    // Ensure we got a data header of type screenshot
    if (debug_headerdata[0] != (uint8_t)DATATYPE_SCREENSHOT)
        terminate("Unexpected data header for screenshot.");

    // Allocate space for the image
    image = (byte*)malloc(4*w*h);

    // Ensure we malloced successfully
    if (filename == NULL || image == NULL)
        terminate("Unable to allocate memory for binary file.");

    // Generate the image
    for (uint32_t i=0; i<size; i+=4)
    {
        int texel = swap_endian(((buffer[i+3]<<24)&0xFF000000) | ((buffer[i+2]<<16)&0xFF0000) | ((buffer[i+1]<<8)&0xFF00) | (buffer[i]&0xFF));
        if (debug_headerdata[1] == 2) 
        {
            short pixel1 = (texel&0xFFFF0000)>>16;
            short pixel2 = (texel&0x0000FFFF);
            image[written++] = 0x08*((pixel1>>11) & 0x001F); // R1
            image[written++] = 0x08*((pixel1>>6) & 0x001F);  // G1
            image[written++] = 0x08*((pixel1>>1) & 0x001F);  // B1
            image[written++] = 0xFF;

            image[written++] = 0x08*((pixel2>>11) & 0x001F); // R2
            image[written++] = 0x08*((pixel2>>6) & 0x001F);  // G2
            image[written++] = 0x08*((pixel2>>1) & 0x001F);  // B2
            image[written++] = 0xFF;
        }
        else
        {
            // TODO: Test this because I sure as hell didn't >:V
            image[written++] = (texel>>24) & 0xFF; // R
            image[written++] = (texel>>16) & 0xFF; // G
            image[written++] = (texel>>8)  & 0xFF; // B
            image[written++] = (texel>>0)  & 0xFF; // Alpha
        }
    }

    // Close the file and free the dynamic memory used
    lodepng_encode32_file(filename, image, w, h);
    log_colored("Wrote %dx%d pixels to '%s'.\n", CRDEF_INFO, w, h, filename);
    free(image);
    free(filename);
}


/*==============================
    debug_send
    Sends data to the flashcart
    TODO: Does not handle edge case of 
    two @ between different files not
    separated by a space
    @param The string with the data 
           to send
==============================*/

void debug_send(char* data)
{
    byte*     copy;
    char*     token;
    SendData* mesg;
    uint32_t  datasize;
    uint32_t  tokcount = 0;
    bool      ispath = false;
    std::list<ParseHelper*> datasplit;

    // Start by removing trailing whitespace
    data = trimwhitespace(data);
    datasize = strlen(data);

    // Start by counting the number of '@' characters
    for (uint32_t i=0; i<datasize; i++)
        if (data[i] == '@')
            tokcount++;

    // We need an even number of '@' to be valid
    if (tokcount%2 != 0)
    {
        log_colored("Error: Missing closing '@'\n", CRDEF_ERROR);
        return;
    }

    // Initialize the message to send
    mesg = (SendData*)malloc(sizeof(SendData));
    if (mesg == NULL)
        terminate("Unable to malloc message for debug send.");
    mesg->type = DATATYPE_TEXT;

    // If there is a '@' symbol at both the start and end, then send a raw binary
    if (tokcount == 2 && data[0] == '@' && data[datasize-1] == '@')
        mesg->type = DATATYPE_RAWBINARY;
    if (data[0] == '@')
        ispath = true;

    // Parse the text and append data as needed
    token = strtok(data, "@");
    datasize = 0;
    while (token != NULL)
    {
        ParseHelper* help = (ParseHelper*)calloc(sizeof(ParseHelper), 1);
        if (help == NULL)
            terminate("Unable to malloc data for debug send.");

        // Read the string into the message
        if (!ispath)
        {
            help->strsize = strlen(token);
            help->str = (char*)malloc(help->strsize+1);
            if (help->str == NULL)
                terminate("Unable to malloc data for debug send.");
            strcpy(help->str, token);
        }
        else
        {
            uint32_t size;
            FILE* fp = fopen(token, "rb");
            if (fp == NULL)
            {
                log_colored("Error: Unable to open file '%s'.\n", CRDEF_ERROR, token);
                for (std::list<ParseHelper*>::iterator it = datasplit.begin(); it != datasplit.end(); ++it)
                {
                    ParseHelper* destroy = *it;
                    if (destroy->data != NULL)
                        free(destroy->data);
                    free(destroy->str);
                    free(destroy);
                }
                free(mesg);
                return;
            }

            // Get the data size
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            help->strsize = snprintf(NULL, 0, "@%d@", size);
            help->datasize = size;

            // Make a copy of the data
            help->data = (byte*)malloc(size);
            help->str = (char*)malloc(help->strsize+1);
            if (help->data == NULL || help->str == NULL)
                terminate("Unable to malloc data for debug send.");
            sprintf(help->str, "@%d@", size);
            if (fread(help->data, 1, size, fp) != size)
            {
                log_colored("Error: Unable to read file '%s'.\n", CRDEF_ERROR, token);
                for (std::list<ParseHelper*>::iterator it = datasplit.begin(); it != datasplit.end(); ++it)
                {
                    ParseHelper* destroy = *it;
                    if (destroy->data != NULL)
                        free(destroy->data);
                    free(destroy->str);
                    free(destroy);
                }
                free(mesg);
                return;
            }

            // Cleanup
            fclose(fp);
        }
        datasplit.push_back(help);
        if (mesg->type == DATATYPE_TEXT)
            datasize += help->strsize;
        help->ispath = ispath;
        if (ispath)
            datasize += help->datasize;

        // Get the next token
        token = strtok(NULL, "@");
        ispath = !ispath;
    }

    // Now we have a list of strings and data blocks, we combine all into one
    // We can already assign these values to the mesg
    copy = (byte*)calloc(datasize+1, 1);
    if (copy == NULL)
        terminate("Unable to malloc data for debug send.");
    mesg->size = datasize;
    mesg->data = copy;

    // Iterate the list, copy onto the copy buffer, and free the allocated memory
    for (std::list<ParseHelper*>::iterator it = datasplit.begin(); it != datasplit.end(); ++it)
    {
        ParseHelper* help = *it;
        if (mesg->type == DATATYPE_TEXT)
        {
            strcpy((char*)copy, help->str);
            copy += help->strsize;
        }
        if (help->ispath)
        {
            memcpy(copy, help->data, help->datasize);
            copy += help->datasize;
        }
    }
    
    // Done!
    local_mesgqueue.push(mesg);
    for (std::list<ParseHelper*>::iterator it = datasplit.begin(); it != datasplit.end(); ++it)
    {
        ParseHelper* help = *it;
        if (help->data != NULL)
            free(help->data);
        free(help->str);
        free(help);
    }
}


/*==============================
    debug_setdebugout
    Sets the file where debug logs are
    written to.
    @param The path to the debug log file
==============================*/

void debug_setdebugout(char* path)
{
    local_debugoutfile = fopen(path, "w+");
}


/*==============================
    debug_setbinaryout
    Sets the folder where debug files are
    written to.
    @param The path for debug files
==============================*/

void debug_setbinaryout(char* path)
{
    local_binaryoutfolderpath = path;
}


/*==============================
    debug_getdebugout
    Gets the file where debug logs are
    written to.
    @param The file pointer to the debug log file
==============================*/

FILE* debug_getdebugout()
{
    return local_debugoutfile;
}


/*==============================
    debug_getbinaryout
    Gets the folder where debug files are
    written to.
    @return The path for debug files
==============================*/

char* debug_getbinaryout()
{
    return local_binaryoutfolderpath;
}


/*==============================
    debug_closedebugout
    Closes the debug log file
==============================*/

void debug_closedebugout()
{
    fclose(local_debugoutfile);
    local_debugoutfile = NULL;
}