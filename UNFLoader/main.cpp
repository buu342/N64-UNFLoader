/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/

#include "main.h"
#include "helper.h"
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
void list_args();
void show_help();


/*********************************
             Globals
*********************************/

// Program globals
bool global_usecolors  = true;
int  global_cictype    = 0;
int  global_savetype   = 0;
bool global_listenmode = false;
bool global_debugmode  = false;
bool global_z64        = false;

// Local globals
static int   local_flashcart = CART_NONE;
static char* local_rom = NULL;


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
    initscr();
    start_color();
    use_default_colors();

    // Setup our console
    scrollok(stdscr, 1);
    idlok(stdscr, 1);
    resize_term(40, 80);

    // Initialize the colors
    init_pair(CR_RED, COLOR_RED, -1);
    init_pair(CR_GREEN, COLOR_GREEN, -1);
    init_pair(CR_BLUE, COLOR_BLUE, -1);
    init_pair(CR_YELLOW, COLOR_YELLOW, -1);

    // Start the program
    show_title();
    parse_args(argc, argv);
    device_find(local_flashcart);
    device_open();
    device_sendrom(local_rom);
    device_close();

    // End the program
    pdprint("\nPress any key to continue.\n", CRDEF_INPUT);
    getchar();
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));
    endwin();
    return 0;
}


/*==============================
    parse_args
    Parses the arguments passed to the prgram
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
        char* command;

        // Lowercase the command
        _strlwr_s(argv[i], strlen(argv[i])+1);
        command = argv[i];

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
            if (i<argc) 
            {
                char* value = argv[i];
                local_flashcart = value[0]-'0';

                // Validate that the cart number is between our range
                if (local_flashcart < CART_64DRIVE1 || local_flashcart > CART_EVERDRIVE7) 
                    terminate("Invalid parameter '%s' for command '%s'.\n", value, command);
            } 
            else
                terminate("Missing parameter(s) for command '%s'.\n", command);
        }
        else if (!strcmp(command, "-r")) // Upload ROM command
        {
            i++;

            // If we have an argument after this one, then set the ROM path, otherwise terminate
            if (i<argc) 
                local_rom = argv[i];
            else 
                terminate("Missing parameter(s) for command '%s'.\n", command);
        }
        else if (!strcmp(command, "-c")) // Set CIC command
        {
            i++;

            // If we have an argument after this one, then set the ROM path, otherwise terminate
            if (i<argc) 
                global_cictype = strtol(argv[i], NULL, 0);
            else 
                terminate("Missing parameter(s) for command '%s'.\n", command);
        }
        else if (!strcmp(command, "-w")) // Disable terminal colors command
            global_usecolors = false;
        else if (!strcmp(command, "-l")) // Listen mode
        {
            global_listenmode = true;
            pdprint("Listen mode enabled.\n", CRDEF_PROGRAM);
        }
        else if (!strcmp(command, "-d")) // Debug mode
        {
            global_debugmode = true;
            pdprint("Debug mode enabled.\n", CRDEF_PROGRAM);
        }
        else 
            terminate("Unknown command '%s'.\n", command);
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
    int titlesize = sizeof(title)/sizeof(title[0])-1;
 
    // Print the title
    for (i=0; i<titlesize; i++)
    {
        char str[2] = {title[i], '\0'};
        pdprint(str, 1+(i)%TOTAL_COLORS);
    }

    // Print other stuff
    pdprint("\n--------------------------------------------\n", CR_NONE);
    pdprint("Cobbled together by Buu342\n", CR_NONE);
    pdprint("Compiled on %s\n\n", CR_NONE, __DATE__); //TODO: date is always null?!
}


/*==============================
    list_args
    Lists all the program arugments
==============================*/

void list_args() //TODO: access violation issue is present in the commented text:
{
    pdprint("Parameters: <required> [optional]\n", CRDEF_PROGRAM);
    pdprint("  -help\t\t\t   Learn how to use this tool\n", CRDEF_PROGRAM);
    pdprint("  -r <file>\t\t   Upload ROM\n", CRDEF_PROGRAM);
    pdprint("  -f <int>\t\t   Force flashcart type (skips autodetection)\n", CRDEF_PROGRAM);
    //pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_64DRIVE1, "64Drive HW1");
    //pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_64DRIVE2, "64Drive HW2");
    //pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_EVERDRIVE3, "EverDrive 3.0");
    //pdprint("  \t %d - %s\n", CRDEF_PROGRAM, CART_EVERDRIVE7, "EverDrive X7");
    pdprint("  -c <int>\t\t   Set CIC emulation type (64Drive HW2 only)\n", CRDEF_PROGRAM);
    pdprint("  \t 0 - %s\t 1 - %s\n", CRDEF_PROGRAM, "6101 (NTSC)", "6102 (NTSC)");
    pdprint("  \t 2 - %s\t 3 - %s\n", CRDEF_PROGRAM, "7101 (NTSC)", "7102 (PAL)");
    pdprint("  \t 4 - %s\t\t 5 - %s\n", CRDEF_PROGRAM, "x101 (All)", "x102 (All)");
    pdprint("  \t 6 - %s\t\t 7 - %s\n", CRDEF_PROGRAM, "x106 (All)", "5101 (NTSC)");
    pdprint("  -s <int>\t\t   Set save emulation type (64Drive only)\n", CRDEF_PROGRAM);
    pdprint("  -d\t\t\t   Debug mode\n", CRDEF_PROGRAM);
    pdprint("  -l\t\t\t   Listen mode (reupload ROM when changed)\n", CRDEF_PROGRAM);
    pdprint("  -w\t\t\t   Disable terminal colors\n\n", CRDEF_PROGRAM);
}


/*==============================
    show_help
    Prints the help information
==============================*/

void show_help()
{
    char category = 0;

    // Print the introductory message
    pdprint("Welcome to the %s!\n", CRDEF_PROGRAM, PROGRAM_NAME_LONG);
    pdprint("This tool is designed to upload ROMs to your N64 Flashcart via USB, to allow\n"
            "homebrew developers to debug their ROMs in realtime with the help of the\n"
            "library provided with this tool.\n\n", CRDEF_PROGRAM);
    pdprint("Which category are you interested in?\n"
            " 1 - Uploading ROMs on the 64Drive\n"
            " 2 - Uploading ROMs on the EverDrive\n"
            " 3 - Using Listen mode\n"
            " 4 - Using Debug mode\n"
            " 5 - How to diagnose a thread crash\n", CRDEF_PROGRAM);

    // Get the category
    pdprint("\nCategory: ", CRDEF_INPUT);
    category = getchar();
    pdprint("%c\n\n", CRDEF_INPUT, category);

    // Print the category contents
    switch (category)
    {
        case '1':
            pdprint(" 1) Plug your 64Drive USB into your PC, ensuring the console is turned OFF.\n"
                    " 2) Run this program to upload a ROM. Example:\n" 
                    " \t unfloader.exe -r myrom.n64\n"
                    " 3) If using 64Drive HW2, your game might not boot if you do not state the\n"
                    "    correct CIC as an argument. Most likely, you are using CIC 6102, so simply\n"
                    "    append that to the end of the arguments. Example:\n"
                    " \t unfloader.exe -r myrom.n64 -c 6102\n"
                    " 4) Once the upload process is finished, turn the console on. Your ROM should\n"
                    "    execute.\n", CRDEF_PROGRAM);
            break;
        case '2':
            pdprint(" 1) Plug your EverDrive USB into your PC, ensuring the console is turned ON and\n"
                    "    in the main menu.\n"
                    " 2) Run this program to upload a ROM. Example:\n" 
                    " \t unfloader.exe -r myrom.n64\n"
                    " 3) Once the upload process is finished, your ROM should execute.\n", CRDEF_PROGRAM);
            break;
        case '3':
            pdprint("Listen mode automatically re-uploads the ROM via USB when it is modified. This\n"
                    "saves you the trouble of having to restart this program every recompile of your\n"
                    "homebrew. It is on YOU to ensure the cart is prepared to receive another ROM.\n"
                    "That means that the console must be switched OFF if you're using the 64Drive or\n"
                    "be in the menu if you're using an EverDrive.\n", CRDEF_PROGRAM);
            break;
        case '4':
            pdprint("In order to use the debug mode, the N64 ROM that you are executing must already\n"
                    "have implented the debug library that comes with this tool. Otherwise, the\n"
                    "debug mode will serve no purpose.\n\n", CRDEF_PROGRAM);
            pdprint("During debug mode, you are able to type commands, which show up in ", CRDEF_PROGRAM);
            pdprint(                                                                    "green", CRDEF_INPUT);
            pdprint(                                                                          " on\n"
                    "the bottom of the terminal. You can press ENTER to send this command to the N64\n"
                    "as the ROM executes. The command you send must obviously be implemented by the\n"
                    "debug library, and can do various things, such as upload binary files, take\n"
                    "screenshots, or change things in the game. It is recommended that developers\n"
                    "implement a command to list implemented commands, which is '", CRDEF_PROGRAM);
            pdprint(                                                            "list", CRDEF_INFO);
            pdprint(                                                                 "' by default.\n\n", CRDEF_PROGRAM);
            pdprint("During execution, the ROM is free to print things to the console where this\n"
                    "program is running. Messages from the console will appear in ", CRDEF_PROGRAM);
            pdprint(                                                              "yellow", CRDEF_PRINT);
            pdprint(                                                                     ".\n\n"
                    "For more information on how to implement the debug library, check the GitHub\n"
                    "page where this tool was uploaded too, there should be plenty of examples there.\n"
                    PROGRAM_GITHUB"\n", CRDEF_PROGRAM);
            break;
        case '5':
            // TODO: Write this section
            pdprint("Todo", CRDEF_PROGRAM);
            break;
        default:
            terminate("Unknown category.\n"); 
    }
}