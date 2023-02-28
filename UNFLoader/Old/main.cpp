/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/

#include "main.h"
#include "helper.h"
#include "device.h"
#include <signal.h>
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
void autodetect_romheader();
void show_title();
void list_args();
void show_help();


/*********************************
             Globals
*********************************/

// Program globals
bool    global_usecolors   = true;
int     global_cictype     = -1;
u32     global_savetype    = 0;
bool    global_listenmode  = false;
bool    global_debugmode   = false;
bool    global_z64         = false;
char*   global_debugout    = NULL;
FILE*   global_debugoutptr = NULL;
char*   global_exportpath  = NULL;
time_t  global_timeout     = 0;
time_t  global_timeouttime = 0;
bool    global_closefail   = false;
char*   global_filename    = NULL;
WINDOW* global_window      = NULL;
int     global_termsize[2] = {DEFAULT_TERMROWS, DEFAULT_TERMCOLS};
int     global_padpos      = 0;
bool    global_scrolling   = false;
bool    global_stackprints = true;

// Local globals
static int   local_flashcart = CART_NONE;
static char* local_rom = NULL;
static bool  local_autodetect = true;
static int   local_historysize = DEFAULT_HISTORYSIZE;



/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

int main(int argc, char* argv[])
{
    int i;

    // Initialize PDCurses
    #ifdef LINUX
        setlocale(LC_ALL, "");
    #endif
    initscr();
    start_color();
    use_default_colors();
    noecho();
    keypad(stdscr, TRUE);

    // Setup our console
    global_window = newpad(local_historysize + global_termsize[0], global_termsize[1]);
    scrollok(global_window, TRUE);
    idlok(global_window, TRUE);
    resize_term(global_termsize[0], global_termsize[1]);
    keypad(global_window, TRUE);

    // Initialize the colors
    init_pair(CR_RED, COLOR_RED, -1);
    init_pair(CR_GREEN, COLOR_GREEN, -1);
    init_pair(CR_BLUE, COLOR_BLUE, -1);
    init_pair(CR_YELLOW, COLOR_YELLOW, -1);
    init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);

    // Start the program
    show_title();
    parse_args(argc, argv);
    autodetect_romheader();

    if (local_rom == NULL)
        terminate("Missing ROM argument (-r <ROM NAME HERE>)\n");

    // Upload the ROM and start debug mode if necessary
    device_find(local_flashcart);
    device_open();
    device_sendrom(local_rom);
    device_close();

    // End the program
    if (global_timeout == 0)
    {
        pdprint("\nPress any key to continue.\n", CRDEF_INPUT);
        getchar();
    }
    else
        handle_timeout();
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));
    endwin();
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
    int i;

    // If no arguments were given, print the args
    if (argc == 1) 
    {
        list_args();
        terminate("");
    }

    // Check every argument
    for (i=1; i<argc; i++) 
    {
        char* command = argv[i];

        // Check through possible commands
        if (!strcmp(command, "-help")) // Help command
        {
            show_help();
            terminate("");
        }
        else if (!strcmp(command, "-f")) // Force flashcart command
        {
            i++;

            // If we have an argument after this one, set it as our flashcart, otherwise terminate
            if (i<argc && argv[i][0] != '-') 
            {
                char* value = argv[i];
                local_flashcart = value[0]-'0';

                // Validate that the cart number is between our range
                if (local_flashcart < CART_64DRIVE1 || local_flashcart > CART_EVERDRIVE) 
                    terminate("Invalid parameter '%s' for command '%s'.", value, command);
            } 
            else
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-r") || command[0] != '-') // Upload ROM command (or assume filepath for ROM)
        {
            // Increment i only if '-' was found
            if (command[0] == '-')
                i++;

            // If we have an argument after this one, then set the ROM path, otherwise terminate
            if ((i<argc && argv[i][0] != '-') || command[0] != '-') 
                local_rom = argv[i];
            else 
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-c")) // Set CIC command
        {
            i++;

            // If we have an argument after this one, then set the ROM path, otherwise terminate
            if (i<argc && argv[i][0] != '-') 
            {
                if (isdigit(argv[i][0]))
                    global_cictype = strtol(argv[i], NULL, 0);
                else
                    global_cictype = strtol(argv[i]+1, NULL, 0);
            }
            else 
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-s")) // Set save type command
        {
            i++;

            // If we have an argument after this one, then set the save type, otherwise terminate
            if (i < argc && argv[i][0] != '-')
                global_savetype = strtol(argv[i], NULL, 0);
            else
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-a")) // Disable automatic header parsing
            local_autodetect = false;
        else if (!strcmp(command, "-w")) // Set terminal size
        {
            i++;

            // If we have an argument after this one, then set the terminal height, otherwise terminate
            if (i<argc && argv[i][0] != '-')
            {
                i++;
                global_termsize[0] = strtol(argv[i], NULL, 0);

                // If we have an argument after this one, then set the terminal width, otherwise terminate
                if (i < argc && argv[i][0] != '-')
                {
                    global_termsize[1] = strtol(argv[i], NULL, 0);
                    resize_term(global_termsize[0], global_termsize[1]);
                    wresize(global_window, local_historysize + global_termsize[0], global_termsize[1]);
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
            }
            else
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-h")) // Set command history size
        {
            i++;

            // If we have an argument after this one, then set the terminal height, otherwise terminate
            if (i < argc && argv[i][0] != '-')
            {
                local_historysize = strtol(argv[i], NULL, 0);
                wresize(global_window, local_historysize + global_termsize[0], global_termsize[1]);
            }
            else
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-b")) // Disable terminal colors command
            global_usecolors = false;
        else if (!strcmp(command, "-e")) // Export directory
        {
            i++;

            // If we have an argument after this one, then set the export path
            if (i<argc && argv[i][0] != '-')
            {
                u32 len = strlen(argv[i]);
                if (argv[i][len-1] == '/')
                    global_exportpath = argv[i];
                else
                    terminate("Incorrect path '%s'.", argv[i]);
                pdprint("Set export path to '%s'.\n", CRDEF_PROGRAM, global_exportpath);
            }
            else
                terminate("Missing parameter(s) for command '%s'.", command);
        }
        else if (!strcmp(command, "-m")) // Debug message stacking
        global_stackprints = false;
        else if (!strcmp(command, "-l")) // Listen mode
        {
            global_listenmode = true;
            pdprint("Listen mode enabled.\n", CRDEF_PROGRAM);
        }
        else if (!strcmp(command, "-d")) // Debug mode
        {
            global_debugmode = true;
            pdprint("Debug mode enabled.", CRDEF_PROGRAM);

            // If we have an argument after this one, then set the output directory
            if (i+1<argc && argv[i+1][0] != '-')
            {
                i++;
                if (global_exportpath != NULL)
                {
                    char* filepath = (char*)malloc(256);
                    memset(filepath, 0 ,256);
                    strcat(filepath, global_exportpath);
                    strcat(filepath, argv[i]);
                    global_debugout = filepath;
                }
                else
                    global_debugout = argv[i];
                pdprint(" Writing output to %s.", CRDEF_PROGRAM, global_debugout);
            }
            pdprint("\n", CRDEF_PROGRAM);
        }
        else if (!strcmp(command, "-t")) // Timeout command
        {
            i++;

            // If we have an argument after this one, then set the timeout, otherwise terminate
            if (i<argc && argv[i][0] != '-') 
                global_timeout = atoi(argv[i]);
            else 
                terminate("Missing parameter(s) for command '%s'.", command);
            pdprint("Set timeout to %d seconds.\n", CRDEF_PROGRAM, global_timeout);
        }
        else 
            terminate("Unknown command '%s'.", command);
    }
}


/*==============================
    autodetect_romheader
    Reads the ROM header and sets the save/RTC value
    using the ED format.
==============================*/

void autodetect_romheader()
{
    FILE *file;
    char buff[0x40];
    if (!local_autodetect)
        return;

    // Open the file
    file = fopen(local_rom, "rb");
    if (file == NULL)
    {
        device_close();
        terminate("Unable to open file '%s'.\n", local_rom);
    }

    // Check for a valid header
    fread(buff, 1, 0x40, file);
    if (buff[0x3C] != 'E' || buff[0x3D] != 'D')
        return;

    // If the savetype hasn't been forced
    if (global_savetype == 0)
    {
        switch (buff[0x3F])
        {
            case 0x10: global_savetype = 1; break;
            case 0x20: global_savetype = 2; break;
            case 0x30: global_savetype = 3; break;
            case 0x50: global_savetype = 4; break;
            case 0x40: global_savetype = 5; break;
            case 0x60: global_savetype = 6; break;
        }
        if (global_savetype != 0)
            pdprint("Auto set save type to %d from ED header.\n", CRDEF_PROGRAM, global_savetype);
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
        pdprint(str, 1+(i)%(TOTAL_COLORS-1));
    }

    // Print other stuff
    pdprint("\n--------------------------------------------\n", CR_NONE);
    pdprint("Cobbled together by Buu342\n", CR_NONE);
    pdprint("Compiled on %s\n\n", CR_NONE, __DATE__);
}


/*==============================
    list_args
    Lists all the program arugments
==============================*/

void list_args()
{
    pdprint("Parameters: <required> [optional]\n", CRDEF_PROGRAM);
    pdprint("  -help\t\t\t   Learn how to use this tool.\n", CRDEF_PROGRAM);
    pdprint("  -r <file>\t\t   Upload ROM.\n", CRDEF_PROGRAM);
    pdprint("  -a\t\t\t   Disable ED ROM header autodetection.\n", CRDEF_PROGRAM);
    pdprint("  -f <int>\t\t   Force flashcart type (skips autodetection).\n", CRDEF_PROGRAM);
    pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_64DRIVE1, "64Drive HW1");
    pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_64DRIVE2, "64Drive HW2");
    pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_EVERDRIVE, "EverDrive 3.0 or X7");
    pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_SC64, "SC64");
    pdprint("  -c <int>\t\t   Set CIC emulation (64Drive HW2 only).\n", CRDEF_PROGRAM);
    pdprint("  \t 0 - %s\t 1 - %s\n", CRDEF_PROGRAM, "6101 (NTSC)", "6102 (NTSC)");
    pdprint("  \t 2 - %s\t 3 - %s\n", CRDEF_PROGRAM, "7101 (NTSC)", "7102 (PAL)");
    pdprint("  \t 4 - %s\t\t 5 - %s\n", CRDEF_PROGRAM, "x103 (All)", "x105 (All)");
    pdprint("  \t 6 - %s\t\t 7 - %s\n", CRDEF_PROGRAM, "x106 (All)", "5101 (NTSC)");
    pdprint("  -s <int>\t\t   Set save emulation.\n", CRDEF_PROGRAM);    
    pdprint("  \t 1 - %s\t 2 - %s\n", CRDEF_PROGRAM, "EEPROM 4Kbit", "EEPROM 16Kbit");
    pdprint("  \t 3 - %s\t 4 - %s\n", CRDEF_PROGRAM, "SRAM 256Kbit", "FlashRAM 1Mbit");
    pdprint("  \t 5 - %s\t 6 - %s\n", CRDEF_PROGRAM, "SRAM 768Kbit", "FlashRAM 1Mbit (PokeStdm2)");
    pdprint("  -d [filename]\t\t   Debug mode. Optionally write output to a file.\n", CRDEF_PROGRAM);
    pdprint("  -l\t\t\t   Listen mode (reupload ROM when changed).\n", CRDEF_PROGRAM);
    pdprint("  -t <seconds>\t\t   Enable timeout (disables key press checks).\n", CRDEF_PROGRAM);
    pdprint("  -e <directory>\t   File export directory (Folder must exist!).\n", CRDEF_PROGRAM);
    pdprint(            "\t\t\t   Example:  'folder/path/' or 'c:/folder/path'.\n", CRDEF_PROGRAM);
    pdprint("  -w <int> <int>\t   Force terminal size (number rows + columns).\n", CRDEF_PROGRAM);
    pdprint("  -h <int>\t\t   Max window history (default %d).\n", CRDEF_PROGRAM, DEFAULT_HISTORYSIZE);
    pdprint("  -m \t\t\t   Always show duplicate prints in debug mode.\n", CRDEF_PROGRAM, DEFAULT_HISTORYSIZE);
    pdprint("  -b\t\t\t   Disable terminal colors.\n", CRDEF_PROGRAM);
    pdprint("\n", CRDEF_PROGRAM);
}


/*==============================
    show_help
    Prints the help information
==============================*/

void show_help()
{
    int category = 0;

    // Print the introductory message
    pdprint("Welcome to the %s!\n", CRDEF_PROGRAM, PROGRAM_NAME_LONG);
    pdprint("This tool is designed to upload ROMs to your N64 Flashcart via USB, to allow\n"
            "homebrew developers to debug their ROMs in realtime with the help of the\n"
            "library provided with this tool.\n\n", CRDEF_PROGRAM);
    pdprint("Which category are you interested in?\n"
            " 1 - Uploading ROMs on the 64Drive\n"
            " 2 - Uploading ROMs on the EverDrive\n"
            " 3 - Uploading ROMs on the SC64\n"
            " 4 - Using Listen mode\n"
            " 5 - Using Debug mode\n", CRDEF_PROGRAM);

    // Get the category
    pdprint("\nCategory: ", CRDEF_INPUT);
    category = getchar();
    pdprint("%c\n\n", CRDEF_INPUT, category);

    // Print the category contents
    switch (category)
    {
        case '1':
            pdprint(" 1) Ensure your device is on the latest firmware/version.\n"
                    " 2) Plug your 64Drive USB into your PC, ensuring the console is turned OFF.\n"
                    " 3) Run this program to upload a ROM. Example:\n" 
                    " \t unfloader.exe -r myrom.n64\n", CRDEF_PROGRAM);
            pdprint(" 4) If using 64Drive HW2, your game might not boot if you do not state the\n"
                    "    correct CIC as an argument. Most likely, you are using CIC 6102, so simply\n"
                    "    append that to the end of the arguments. Example:\n"
                    " \t unfloader.exe -r myrom.n64 -c 6102\n"
                    " 5) Once the upload process is finished, turn the console on. Your ROM should\n"
                    "    execute.\n", CRDEF_PROGRAM);
            break;
        case '2':
            pdprint(" 1) Ensure your device is on the latest firmware/version for your cart model.\n"
                    " 2) Plug your EverDrive USB into your PC, ensuring the console is turned ON and\n"
                    "    in the main menu.\n"
                    " 3) Run this program to upload a ROM. Example:\n" 
                    " \t unfloader.exe -r myrom.n64\n"
                    " 4) Once the upload process is finished, your ROM should execute.\n", CRDEF_PROGRAM);
            break;
        case '3':
            pdprint(" 1) Plug the SC64 USB into your PC.\n"
                    " 2) Run this program to upload a ROM. Example:\n" 
                    " \t unfloader.exe -r myrom.n64\n"
                    " 3) Once the upload process is finished, your ROM should execute.\n", CRDEF_PROGRAM);
            break;
        case '4':
            pdprint("Listen mode automatically re-uploads the ROM via USB when it is modified. This\n"
                    "saves you the trouble of having to restart this program every recompile of your\n"
                    "homebrew. It is on YOU to ensure the cart is prepared to receive another ROM.\n"
                    "That means that the console must be switched OFF if you're using the 64Drive or\n"
                    "be in the menu if you're using an EverDrive. In SC64 case ROM can be uploaded\n"
                    "while console is running but if currently running code is actively accessing\n"
                    "ROM space this can result in glitches or even crash, proceed with caution.\n", CRDEF_PROGRAM);
            break;
        case '5':
            pdprint("In order to use the debug mode, the N64 ROM that you are executing must already\n"
                    "have implented the USB or debug library that comes with this tool. Otherwise,\n"
                    "debug mode will serve no purpose.\n\n", CRDEF_PROGRAM);
            pdprint("During debug mode, you are able to type commands, which show up in ", CRDEF_PROGRAM);
            pdprint(                                                                    "green", CRDEF_INPUT);
            pdprint(                                                                          " on\n"
                    "the bottom of the terminal. You can press ENTER to send this command to the N64\n"
                    "as the ROM executes. The command you send must obviously be implemented by the\n"
                    "debug library, and can do various things, such as upload binary files, take\n"
                    "screenshots, or change things in the game. If you wrap a part of your command\n"
                    "with the '@' symbol, the tool will treat that part as a file and will upload it\n"
                    "along with the rest of the data.\n\n", CRDEF_PROGRAM);
            pdprint("During execution, the ROM is free to print things to the console where this\n"
                    "program is running. Messages from the console will appear in ", CRDEF_PROGRAM);
            pdprint(                                                              "yellow", CRDEF_PRINT);
            pdprint(                                                                     ".\n\n"
                    "For more information on how to implement the debug library, check the GitHub\n"
                    "page where this tool was uploaded to, there should be plenty of examples there.\n"
                    PROGRAM_GITHUB"\n", CRDEF_PROGRAM);
            break;
        default:
            terminate("Unknown category."); 
    }
}