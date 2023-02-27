/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "helper.h"
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

void initialize_curses();
void handle_resize(int sig);


/*********************************
             Globals
*********************************/

// Program globals
WINDOW* global_terminal   = NULL;
WINDOW* global_inputwin   = NULL;
WINDOW* global_outputwin  = NULL;
bool    global_usecurses  = true;


/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

int main(int argc, char* argv[])
{
    if (global_usecurses)
        initialize_curses();

    // Loop forever
    while(1)
        ;

    if (global_usecurses)
        endwin();
    return 0;
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
    if (initscr() == NULL)
    {
        fputs("Error: Curses failed to initialize the screen.", stderr);
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
    global_outputwin = newwin(h-1, w, 0, 0);
    global_inputwin = newwin(1, w, h-1, 0);
    scrollok(global_outputwin, TRUE);
    idlok(global_outputwin, TRUE);
    keypad(global_outputwin, TRUE);

    // Draw a box to border the window
    box(global_outputwin, 0, 0);
    wrefresh(global_outputwin);

    whline(global_inputwin, '=', w);
    wrefresh(global_inputwin);

    // Initialize the colors
    init_pair(CR_RED, COLOR_RED, -1);
    init_pair(CR_GREEN, COLOR_GREEN, -1);
    init_pair(CR_BLUE, COLOR_BLUE, -1);
    init_pair(CR_YELLOW, COLOR_YELLOW, -1);
    init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);
}

void handle_resize(int sig)
{
    int w, h;
    endwin();
    clear(); // This re-initializes ncurses, no need to call newwin

    // Get the new terminal size
    getmaxyx(global_terminal, h, w);

    // Refresh the output window
    wresize(global_outputwin, h-1, w);
    wclear(global_outputwin);
    box(global_outputwin, 0, 0);
    wrefresh(global_outputwin);

    // Resize and reposition the input window
    wresize(global_inputwin, 1, w);
    mvwin(global_inputwin, h-1, 0);
    whline(global_inputwin, '=', w);
    wrefresh(global_inputwin);
}