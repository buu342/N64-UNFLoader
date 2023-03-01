#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #pragma warning(push, 0)
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <time.h>
        #include <ctype.h>
        #include <stdbool.h>
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
            #include <locale.h>
            #include <curses.h>
        #endif
        #include "Include/ftd2xx.h"
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

    #define CH_ESCAPE    27
    #define CH_ENTER     '\n'
    #define CH_BACKSPACE '\b'

    #define DEFAULT_TERMCOLS 80
    #define DEFAULT_TERMROWS 40
    #define DEFAULT_HISTORYSIZE 1000


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
    extern WINDOW* global_terminal;
    extern WINDOW* global_inputwin;
    extern WINDOW* global_outputwin;
    extern FILE*   global_debugoutptr;

#endif