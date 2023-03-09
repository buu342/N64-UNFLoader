/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "term.h"
#include "device.h"
#pragma comment(lib, "Include/FTD2XX.lib")


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
        global_progstate = ShowingHelp;
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
    Prints tht title of the program
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