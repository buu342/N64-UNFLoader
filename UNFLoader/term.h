#ifndef __TERM_HEADER
#define __TERM_HEADER 


    /*********************************
                  Macros
    *********************************/

    #define DEFAULT_TERMCOLS 80
    #define DEFAULT_TERMROWS 40
    #define DEFAULT_HISTORYSIZE 1000

    // Color macros
    #define TOTAL_COLORS 5
    #define CR_NONE    0
    #define CR_RED     1
    #define CR_GREEN   2
    #define CR_BLUE    3
    #define CR_YELLOW  4
    #define CR_MAGENTA 5

    // Program colors
    #define CRDEF_PROGRAM CR_NONE
    #define CRDEF_ERROR   CR_RED
    #define CRDEF_INPUT   CR_GREEN
    #define CRDEF_PRINT   CR_YELLOW
    #define CRDEF_INFO    CR_BLUE
    #define CRDEF_SPECIAL CR_MAGENTA


    /*********************************
            Function Prototypes
    *********************************/

    // Terminal control
    void term_initialize();
    void term_initsize(int h, int w);
    void term_setsize(int h, int w);
    void term_sethistorysize(int val);
    void term_usecurses(bool val);
    void term_allowinput(bool val);
    void term_enablestacking(bool val);
    void term_end();

    // Terminal checking
    bool term_isusingcurses();
    int  term_getw();
    int  term_geth();
    bool term_waskeypressed();

    // Printing
    #include "term_internal.h"
    #define log_simple(string, ...) __log_output(CRDEF_PROGRAM, 0, false, string, ##__VA_ARGS__)
    #define log_colored(string, color, ...) __log_output(color, 0, false, string, ##__VA_ARGS__)
    #define log_stackable(string, color, ...) __log_output(color, 0, true, string, ##__VA_ARGS__)
    #define log_replace(string, color, ...) __log_output(color, 1, false, string, ##__VA_ARGS__)

#endif