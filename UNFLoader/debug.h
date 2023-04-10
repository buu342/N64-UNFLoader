#ifndef __DEBUGMODE_HEADER
#define __DEBUGMODE_HEADER

    #include "device.h"

    // Data types defintions
    #define DATATYPE_TEXT       0x01
    #define DATATYPE_RAWBINARY  0x02
    #define DATATYPE_HEADER     0x03
    #define DATATYPE_SCREENSHOT 0x04
    #define DATATYPE_HEARTBEAT  0x05

    void debug_main(ftdi_context_t *cart);

#endif