#include "debug.h"
#include <sys/stat.h>
#ifndef LINUX
    #include <shlwapi.h>
#endif


/*********************************
             Globals
*********************************/

static FILE* local_debugoutfile = NULL;
static char* local_binaryoutfolderpath = NULL;


/*==============================
    debug_main
    TODO
==============================*/

void debug_main(FTDIDevice *cart)
{

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