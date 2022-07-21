#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #pragma warning(push, 0)
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <time.h>
        #include <ctype.h>
        #ifndef LINUX
            #include <windows.h> // Needed to prevent a macro redefinition due to curses.h
        #else
            #include <unistd.h>
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

    #define false 0
    #define true  1

    #define CH_ESCAPE    27
    #define CH_ENTER     '\n'
    #define CH_BACKSPACE '\b'

    #define DEFAULT_TERMCOLS 80
    #define DEFAULT_TERMROWS 40
    #define DEFAULT_HISTORYSIZE 1000


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

    extern bool    global_usecolors;
    extern int     global_cictype;
    extern u32     global_savetype;
    extern bool    global_listenmode;
    extern bool    global_debugmode;
    extern bool    global_z64;
    extern char*   global_debugout;
    extern FILE*   global_debugoutptr;
    extern char*   global_exportpath;
    extern time_t  global_timeout;
    extern time_t  global_timeouttime;
    extern bool    global_closefail;
    extern char*   global_filename;
    extern WINDOW* global_window;
    extern int     global_termsize[2];
    extern int     global_padpos;
    extern bool    global_scrolling;
    extern bool    global_stackprints;

#endif