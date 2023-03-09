/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "term.h"
#include "device.h"


/*********************************
              Macros
*********************************/

#define PROGRAM_NAME_LONG  "Universal N64 Flashcart Loader"
#define PROGRAM_NAME_SHORT "UNFLoader"
#define PROGRAM_GITHUB     "https://github.com/buu342/N64-UNFLoader"


/*********************************
        Function Prototypes
*********************************/

void parse_args(int argc, char* argv[]);
void show_title();
void show_args();
#define nextarg_isvalid() ((++i)<argc && argv[i][0] != '-')


/*********************************
             Globals
*********************************/

// Program globals
FILE* global_debugoutptr = NULL;
std::atomic<progState> global_progstate (Initializing);
std::atomic<bool> global_escpressed (false);

// Local globals
static bool                   local_autodetect  = true;


/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

#include <thread>
#include <chrono>
int main(int argc, char* argv[])
{
    // Read program arguments
    parse_args(argc, argv);

    // Initialize the program
    term_initialize();
    show_title();

    if (global_progstate == ShowingArgs)
    {
        show_args();
        terminate(NULL);
    }

    // Loop forever
    while (!global_escpressed)
    {
        static int i = 0;   
        log_colored("Hello %d\n", CRDEF_PRINT, i++);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // End the program
    terminate(NULL);
    return 0;
}


/*==============================
    parse_args
    Parses the arguments passed to the program
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

void parse_args(int argc, char* argv[])
{
    // If no arguments were given, print the args
    if (argc == 1) 
    {
        global_progstate = ShowingArgs;
        return;
    }

    // If the first character is not a dash, assume a ROM path
    if (argv[1][0] != '-')
    {
        device_setrom(argv[1]);
        return;
    }

    // Handle the rest of the program arguments
    for (int i=1; i<argc; i++) 
    {
        char* command = argv[i];
        switch(argv[i][1])
        {
            case 'r':
                if (nextarg_isvalid())
                    device_setrom(argv[i]);
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            default:
                terminate("Unknown command '%s'", command);
                break;
        }
    }
}


/*==============================
    show_title
    Prints the title of the program
==============================*/

void show_title()
{
    int i;
    char title[] = PROGRAM_NAME_SHORT;
    int titlesize = sizeof(title)/sizeof(title[0]);
 
    // Print the title
    for (i=0; i<titlesize-1; i++)
    {
        char str[2];
        str[0] = title[i];
        str[1] = '\0';
        log_colored(str, 1+(i)%(TOTAL_COLORS-1));
    }

    // Print info stuff
    log_simple("\n--------------------------------------------\n");
    log_simple("Cobbled together by Buu342\n");
    log_simple("Compiled on %s\n\n", __DATE__);
}


/*==============================
    show_args
    Prints the arguments of the program
==============================*/

void show_args()
{
    log_simple("Parameters: <required> [optional]\n", CRDEF_PROGRAM);
    log_simple("  -help\t\t\t   Learn how to use this tool.\n", CRDEF_PROGRAM);
    log_simple("  -r <file>\t\t   Upload ROM.\n", CRDEF_PROGRAM);
    log_simple("  -a\t\t\t   Disable ED ROM header autodetection.\n", CRDEF_PROGRAM);
    log_simple("  -f <int>\t\t   Force flashcart type (skips autodetection).\n", CRDEF_PROGRAM);
    log_simple("  \t %d - %s\n", CRDEF_PROGRAM, (int)CART_64DRIVE1, "64Drive HW1");
    /*
    log_simple("  \t %d - %s\n", CRDEF_PROGRAM, (int)CART_64DRIVE2, "64Drive HW2");
    log_simple("  \t %d - %s\n", CRDEF_PROGRAM, (int)CART_EVERDRIVE, "EverDrive 3.0 or X7");
    log_simple("  \t %d - %s\n", CRDEF_PROGRAM, (int)CART_SC64, "SC64");
    log_simple("  -c <int>\t\t   Set CIC emulation (64Drive HW2 only).\n", CRDEF_PROGRAM);
    log_simple("  \t %d - %s\t %d - %s\n", CRDEF_PROGRAM, (int)CIC_6101, "6101 (NTSC)", (int)CIC_6102, "6102 (NTSC)");
    log_simple("  \t %d - %s\t %d - %s\n", CRDEF_PROGRAM, (int)CIC_7101, "7101 (NTSC)", (int)CIC_7102, "7102 (PAL)");
    log_simple("  \t %d - %s\t\t %d - %s\n", CRDEF_PROGRAM, (int)CIC_X103, "x103 (All)", (int)CIC_X105, "x105 (All)");
    log_simple("  \t %d - %s\t\t %d - %s\n", CRDEF_PROGRAM, (int)CIC_X106, "x106 (All)", (int)CIC_5101, "5101 (NTSC)");
    log_simple("  -s <int>\t\t   Set save emulation.\n", CRDEF_PROGRAM);    
    log_simple("  \t %d - %s\t %d - %s\n", CRDEF_PROGRAM, (int)SAVE_EEPROM4K, "EEPROM 4Kbit", (int)SAVE_EEPROM16K, "EEPROM 16Kbit");
    log_simple("  \t %d - %s\t %d - %s\n", CRDEF_PROGRAM, (int)SAVE_SRAM256, "SRAM 256Kbit", (int)SAVE_FLASHRAM, "FlashRAM 1Mbit");
    log_simple("  \t %d - %s\t %d - %s\n", CRDEF_PROGRAM, (int)SAVE_SRAM768, "SRAM 768Kbit", (int)SAVE_FLASHRAMPKMN, "FlashRAM 1Mbit (PokeStdm2)");
    log_simple("  -d [filename]\t\t   Debug mode. Optionally write output to a file.\n", CRDEF_PROGRAM);
    log_simple("  -l\t\t\t   Listen mode (reupload ROM when changed).\n", CRDEF_PROGRAM);
    log_simple("  -t <seconds>\t\t   Enable timeout (disables key press checks).\n", CRDEF_PROGRAM);
    log_simple("  -e <directory>\t   File export directory (Folder must exist!).\n", CRDEF_PROGRAM);
    log_simple(            "\t\t\t   Example:  'folder/path/' or 'c:/folder/path'.\n", CRDEF_PROGRAM);
    log_simple("  -w <int> <int>\t   Force terminal size (number rows + columns).\n", CRDEF_PROGRAM);
    log_simple("  -h <int>\t\t   Max window history (default %d).\n", CRDEF_PROGRAM, DEFAULT_HISTORYSIZE);
    log_simple("  -m \t\t\t   Always show duplicate prints in debug mode.\n", CRDEF_PROGRAM, DEFAULT_HISTORYSIZE);
    log_simple("  -b\t\t\t   Disable terminal colors.\n", CRDEF_PROGRAM);
    log_simple("\n", CRDEF_PROGRAM);*/
}