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
WINDOW* global_window = NULL;
bool    global_usecurses = true;


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

    printf("Hello World!\n");

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
    // Initialize PDCurses
    global_window = initscr();
    if (global_window == NULL)
    {
        fputs("Error: Curses failed to initialize the screen.", stderr);
        exit(EXIT_FAILURE);
    }
    start_color();
    use_default_colors();
    noecho();
    keypad(stdscr, TRUE);

    #ifdef LINUX
        // Initialize signal
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = handle_resize;
        sigaction(SIGWINCH, &sa, NULL);
    #endif

    // Draw a box to border the window
    box(global_window, 0, 0);
    wrefresh(global_window);

    // Loop forever
    while(1)
        ;

    /*
    // Setup our console
    global_window = newpad(local_historysize + global_termsize[0], global_termsize[1]);
    scrollok(global_window, TRUE);
    idlok(global_window, TRUE);
    keypad(global_window, TRUE);

    // Initialize the colors
    init_pair(CR_RED, COLOR_RED, -1);
    init_pair(CR_GREEN, COLOR_GREEN, -1);
    init_pair(CR_BLUE, COLOR_BLUE, -1);
    init_pair(CR_YELLOW, COLOR_YELLOW, -1);
    init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);
    */
}

void handle_resize(int sig)
{
    endwin();
    clear();
    box(global_window, 0, 0);
    wrefresh(global_window);
}