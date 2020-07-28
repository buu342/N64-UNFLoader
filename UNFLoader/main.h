#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <windows.h> // Needed to prevent a macro redefinition due to curses.h

    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
    #include "Include/ftd2xx.h"


    /*********************************
                  Macros
    *********************************/

    #define false 0
    #define true  1


    /*********************************
                 Typedefs
    *********************************/

    typedef unsigned char  u8;
    typedef unsigned short u16;
    typedef unsigned int   u32;
    typedef char           s8;
    typedef short          s16;
    typedef int            s32;


    /*********************************
                 Globals
    *********************************/

    extern bool global_usecolors;
    extern int  global_cictype;
    extern int  global_savetype;
    extern bool global_listenmode;
    extern bool global_debugmode;
    extern bool global_z64;

#endif