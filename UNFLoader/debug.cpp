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


/*********************************
              Macros
*********************************/

#define HEADER_SIZE 16
#define PATH_SIZE 512


/*********************************
        Function Prototypes
*********************************/

void debug_handle_text(uint32_t size, byte* buffer);
void debug_handle_rawbinary(uint32_t size, byte* buffer);
void debug_handle_header(uint32_t size, byte* buffer);
void debug_handle_screenshot(uint32_t size, byte* buffer);


/*********************************
             Globals
*********************************/

// Output file paths
static FILE* local_debugoutfile = NULL;
static char* local_binaryoutfolderpath = NULL;

// Other
static int debug_headerdata[HEADER_SIZE];


/*==============================
    debug_main
    Handles debug I/O
==============================*/

void debug_main()
{
    byte*    outbuff = NULL;
    uint32_t dataheader = 0;

    // Send data to USB if it exists

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
                default:                  terminate("Unknown data type.");
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

void debug_handle_text(uint32_t size, byte* buffer)
{
    char* text;
    text = (char*)malloc(size+1);
    if (text == NULL)
        terminate("Failed to allocate memory for incoming string.");
    memset(text, 0, size+1);
    strncpy(text, (char*)buffer, size);
    log_colored("%s", CRDEF_PRINT, text);
    free(text);
}


/*==============================
    debug_handle_rawbinary
    Handles DATATYPE_RAWBINARY
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

void debug_handle_rawbinary(uint32_t size, byte* buffer)
{
    char* filename = NULL;
    char* extraname = gen_filename();
    FILE* fp = NULL;

    // Ensure we malloced successfully
    if (extraname == NULL)
        terminate("Unable to allocate memory for binary file path.");

    // Create the binary file to save data to
    if (local_binaryoutfolderpath != NULL)
    {
        filename = (char*)calloc(snprintf(NULL, 0, "%sbinaryout-%s.bin", local_binaryoutfolderpath, extraname) + 1, 1);
        if (filename == NULL)
            terminate("Unable to allocate memory for binary file path.");
        sprintf(filename, "%sbinaryout-%s.bin", local_binaryoutfolderpath, extraname);
    }
    else
    {
        filename = (char*)calloc(snprintf(NULL, 0, "binaryout-%s.bin", extraname) + 1, 1);
        if (filename == NULL)
            terminate("Unable to allocate memory for binary file path.");
        sprintf(filename, "binaryout-%s.bin", extraname);
    }
    fp = fopen(filename, "wb+");

    // Ensure the file was created
    if (fp == NULL)
        terminate("Unable to create binary file.");

    // Write the data to the file
    fwrite(buffer, 1, size, fp);
    log_colored("Wrote %d bytes to '%s'.\n", CRDEF_INFO, size, filename);
    // TODO: Clear term console stack

    // Cleanup
    fclose(fp);
    free(filename);
    free(extraname);
}


/*==============================
    debug_handle_header
    Handles DATATYPE_HEADER
    @param The size of the incoming data
    @param The buffer to read from
==============================*/

void debug_handle_header(uint32_t size, byte* buffer)
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

void debug_handle_screenshot(uint32_t size, byte* buffer)
{
    /*
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
    */
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