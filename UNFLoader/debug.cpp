#include "debug.h"
#include "term.h"
#include "helper.h"
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#ifndef LINUX
    #include <shlwapi.h>
#endif


/*********************************
              Macros
*********************************/
#define PATH_SIZE 512


/*********************************
        Function Prototypes
*********************************/

void debug_handle_text(uint32_t size, byte* buffer);
void debug_handle_rawbinary(uint32_t size, byte* buffer);


/*********************************
             Globals
*********************************/

// Output file paths
static FILE* local_debugoutfile = NULL;
static char* local_binaryoutfolderpath = NULL;


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
                /*
                case DATATYPE_HEADER:     debug_handle_header(size, outbuff); break;
                case DATATYPE_SCREENSHOT: debug_handle_screenshot(size, outbuff); break;
                */
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