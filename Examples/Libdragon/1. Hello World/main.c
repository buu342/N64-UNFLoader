/***************************************************************
                             main.c
                               
Handles the boot process of the ROM.
***************************************************************/

#include <libdragon.h>
#include "debug.h"


/*********************************
           Definitions
*********************************/

// Use printf instead of usb_write
#define USE_PRITNF 1


/*==============================
    main
    Initializes the ROM
==============================*/

int main(void)
{
    // Initialize the debug library and say hello to the command prompt!
    #if USE_PRITNF
        debug_initialize();
        debug_printf("Hello World!\n");
    #else
        usb_initialize();
        usb_write(DATATYPE_TEXT, "Hello World!\n", 13+1); 
    #endif

    // Spin forever
    while(1)
        ;
}