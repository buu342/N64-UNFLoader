#ifndef __HELPER_HEADER
#define __HELPER_HEADER


    /*********************************
                  Macros
    *********************************/

    #define PRINT_HISTORY_SIZE 512

    // Color macros
    #define TOTAL_COLORS 4
    #define CR_NONE   0
    #define CR_RED    1
    #define CR_GREEN  2
    #define CR_BLUE   3
    #define CR_YELLOW 4

    // Program colors
    #define CRDEF_PROGRAM CR_NONE
    #define CRDEF_ERROR   CR_RED
    #define CRDEF_INPUT   CR_GREEN
    #define CRDEF_PRINT   CR_YELLOW
    #define CRDEF_INFO    CR_BLUE


    /*********************************
            Function Prototypes
    *********************************/

    // Printing
    void pdprint(char* str, short color, short num_args, ...);
    void pdprint_replace(char* str, short color, ...);
    void terminate(char* reason, ...);
    void progressbar_draw(char* text, float percent);

    // Useful functions
    void testcommand(FT_STATUS status, char* reason, ...);
    u32  swap_endian(u32 val);
    u32  calc_padsize(u32 size);
    #define SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) // From https://graphics.stanford.edu/~seander/bithacks.html#SwappingValuesXOR

#endif