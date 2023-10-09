#ifndef __MAIN_HEADER
#define __MAIN_HEADER

    #pragma warning(push, 0)
        #include <stdio.h>
        #include <ctype.h>
        #include <stdbool.h>
        #ifndef LINUX
            #include <winsock2.h>
            #include <windows.h> // Needed to prevent a macro redefinition due to curses.h
        #else
            #include <unistd.h>
        #endif
        #include <atomic>
    #pragma warning(pop)


    /*********************************
           Macros and Typedefs
    *********************************/

    typedef uint8_t byte;
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
    extern bool    global_badpackets;
    extern char*   global_gdbaddr;
    extern std::atomic<bool> global_terminating;


    /*********************************
            Function Prototypes
    *********************************/

    void program_event(ProgEvent key);
    int  get_escapelevel();
    void increment_escapelevel();
    void decrement_escapelevel();
    int  get_timeout();

#endif