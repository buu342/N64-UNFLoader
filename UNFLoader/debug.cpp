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
    TODO
==============================*/

void debug_setdebugout(char* path)
{
    local_debugoutfile = fopen(path, "w+");
}


/*==============================
    debug_setbinaryout
    TODO
==============================*/

void debug_setbinaryout(char* path)
{
    local_binaryoutfolderpath = path;
}


/*==============================
    debug_getdebugout
    TODO
==============================*/

FILE* debug_getdebugout()
{
    return local_debugoutfile;
}


/*==============================
    debug_getbinaryout
    TODO
==============================*/

char* debug_getbinaryout()
{
    return local_binaryoutfolderpath;
}


/*==============================
    debug_closedebugout
    TODO
==============================*/

void debug_closedebugout()
{
    fclose(local_debugoutfile);
    local_debugoutfile = NULL;
}