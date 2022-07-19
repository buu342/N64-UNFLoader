#ifndef __HELPER_HEADER
#define __HELPER_HEADER

    #include "helper_internal.h"


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


    /*********************************
            Function Prototypes
    *********************************/

    // Printing
    #define pdprint(string, color, ...) __pdprint(color, string, ##__VA_ARGS__)
    #define pdprintw(window, string, color, ...) __pdprintw(window, color, 1, string, ##__VA_ARGS__)
    #define pdprintw_nolog(window, string, color, ...) __pdprintw(window, color, 0, string, ##__VA_ARGS__)
    #define pdprint_replace(string, color, ...) __pdprint_replace(color, string, ##__VA_ARGS__)
    void    terminate(const char* reason, ...);
    void    progressbar_draw(const char* text, short color, float percent);

    // Useful functions
    void    refresh_pad();
    void    scrollpad(int amount);
    void    testcommand(FT_STATUS status, const char* reason, ...);
    u32     swap_endian(u32 val);
    u32     calc_padsize(u32 size);
    char*   gen_filename();
    #define SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) // From https://graphics.stanford.edu/~seander/bithacks.html#SwappingValuesXOR
    u32     romhash(u8 *buff, u32 len);
    s16     cic_from_hash(u32 hash);
    void    handle_timeout();

#endif