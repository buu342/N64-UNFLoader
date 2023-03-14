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
        PEV_ESCAPE,
        PEV_REUPLOAD
    } ProgEvent;


    /*********************************
                 Globals
    *********************************/

    extern FILE*   global_debugoutptr;
    extern std::atomic<bool> global_terminating;


    /*********************************
            Function Prototypes
    *********************************/

    void program_event(ProgEvent key);

#endif