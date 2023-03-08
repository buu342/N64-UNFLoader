#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #pragma warning(push, 0)
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <time.h>
        #include <ctype.h>
        #include <stdbool.h>
        #include <locale.h>
        #ifndef LINUX
            #include <windows.h> // Needed to prevent a macro redefinition due to curses.h
        #else
            #include <unistd.h>
            #include <signal.h>
        #endif

        #ifndef LINUX
            #include "Include/curses.h"
            #include "Include/curspriv.h"
            #include "Include/panel.h"
        #else
            #include <curses.h>
        #endif
        #include "Include/ftd2xx.h"
        #include <atomic>
    #pragma warning(pop)


    /*********************************
                  Macros
    *********************************/

    #ifndef false
        #define false 0
    #endif
    #ifndef true
        #define true  1
    #endif


    /*********************************
               Enumerations
    *********************************/
    
    typedef enum {
        Initializing, 
        ShowingHelp, 
        Terminating, 
    } progState;


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

    extern bool    global_usecurses;
    extern FILE*   global_debugoutptr;
    extern int     global_historysize;
    extern std::atomic<progState> global_progstate;
    extern std::atomic<bool> global_escpressed;

#endif