/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "helper.h"
#include <thread>
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
void initialize_curses();
void show_title();
void handle_input();
#define nextarg_isvalid() ((++i)<argc && argv[i][0] != '-')


/*********************************
             Globals
*********************************/

// Program globals
WINDOW* global_terminal    = NULL;
WINDOW* global_inputwin    = NULL;
WINDOW* global_outputwin   = NULL;
FILE*   global_debugoutptr = NULL;
char*   global_rompath     = NULL;
int     global_cictype     = -1;
u32     global_savetype    = 0;
bool    global_usecurses   = true;
bool    global_listenmode  = false;
bool    global_debugmode   = false;
bool    global_z64         = false;

// Local globals
static bool        local_autodetect  = true;
static int         local_historysize = DEFAULT_HISTORYSIZE;
static progState   local_progstate   = Initializing;
//static std::thread thread_input;


/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

int main(int argc, char* argv[])
{
    // Read program arguments
    parse_args(argc, argv);

    // Initialize the program
    if (global_usecurses)
        initialize_curses();
    show_title();
    //thread_input = std::thread(handle_input);

    // Loop forever
    while(local_progstate != Terminating)
        ;

    // End the program
    if (global_usecurses)
        endwin();
    //thread_input.join();
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
        local_progstate = ShowingHelp;
        return;
    }

    // If the first character is not a dash, assume a ROM path
    if (argv[1][0] != '-')
    {
        global_rompath = argv[1];
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
                    global_rompath = argv[i];
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
    initialize_curses
    Initializes n/pdcurses for fancy
    terminal control 
==============================*/

void initialize_curses()
{
    int w, h;

    // Initialize PDCurses
    global_terminal = initscr();
    if (global_terminal == NULL)
    {
        fputs("Error: Curses failed to initialize the screen.\n", stderr);
        exit(EXIT_FAILURE);
    }
    start_color();
    use_default_colors();
    noecho();
    keypad(stdscr, TRUE);
    getmaxyx(global_terminal, h, w);

    #ifdef LINUX
        // Initialize signal
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = handle_resize;
        sigaction(SIGWINCH, &sa, NULL);
    #endif

    // Setup our console
    global_outputwin = newpad(h + local_historysize, w);
    global_inputwin = newwin(1, w, h-1, 0);
    scrollok(global_outputwin, TRUE);
    idlok(global_outputwin, TRUE);
    keypad(global_outputwin, TRUE);
    refresh_output();
    wrefresh(global_inputwin);

    // Initialize the colors
    init_pair(CR_RED, COLOR_RED, -1);
    init_pair(CR_GREEN, COLOR_GREEN, -1);
    init_pair(CR_BLUE, COLOR_BLUE, -1);
    init_pair(CR_YELLOW, COLOR_YELLOW, -1);
    init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);
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

void handle_input()
{
    while (local_progstate != Terminating)
    {
        int ch = wgetch(global_inputwin);
        if (ch == 27)
            local_progstate = Terminating;

        #ifndef LINUX
            Sleep(10);
        #else
            usleep(10);
        #endif
    }
}