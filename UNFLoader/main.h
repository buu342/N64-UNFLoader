#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #pragma warning(push, 0)
        #include <stdio.h>
        #include <ctype.h>
        #include <stdbool.h>
        #ifndef LINUX
            #include <windows.h> // Needed to prevent a macro redefinition due to curses.h
        #else
            #include <unistd.h>
        #endif
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

    extern FILE*   global_debugoutptr;
    extern std::atomic<progState> global_progstate;
    extern std::atomic<bool> global_escpressed;

#endif