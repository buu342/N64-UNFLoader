#ifndef __DEBUGMODE_HEADER
#define __DEBUGMODE_HEADER

    #include "device.h"
    #include <stdlib.h>

    // Data types defintions
    #define DATATYPE_TEXT       0x01
    #define DATATYPE_RAWBINARY  0x02
    #define DATATYPE_HEADER     0x03
    #define DATATYPE_SCREENSHOT 0x04

    void  debug_main();
    void  debug_setdebugout(char* path);
    void  debug_setbinaryout(char* path);
    FILE* debug_getdebugout();
    char* debug_getbinaryout();
    void  debug_closedebugout();

#endif 
