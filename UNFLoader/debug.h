#ifndef __DEBUGMODE_HEADER
#define __DEBUGMODE_HEADER

    #include "device.h"
    #include <stdlib.h>

    void  debug_main();
    void  debug_send(USBDataType type, char* data, size_t size);
    void  debug_sendtext(char* data);
    void  debug_setdebugout(char* path);
    void  debug_setbinaryout(char* path);
    FILE* debug_getdebugout();
    char* debug_getbinaryout();
    void  debug_closedebugout();

#endif 
