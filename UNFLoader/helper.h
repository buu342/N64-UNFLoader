#ifndef __HELPER_HEADER
#define __HELPER_HEADER

    /*********************************
                  Macros
    *********************************/

    #define PRINT_HISTORY_SIZE 512

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

#endif