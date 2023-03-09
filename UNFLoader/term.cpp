/***************************************************************
                            main.cpp
                               
Handles terminal I/O, both with and without curses.
***************************************************************/ 

#include "main.h"
#include "helper.h"
#include "term.h"
#ifndef LINUX
    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
#else
    #include <curses.h>
    #include <termios.h>
#endif
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <thread>
#include <queue>
#include <atomic>
#include <algorithm>
#include <climits>
#include <chrono>


/*********************************
              Macros
*********************************/

#define BLINKRATE   500

#define CH_ESCAPE    27
#define CH_ENTER     '\n'
#define CH_BACKSPACE '\b'


/*********************************
            Structures
*********************************/

typedef struct {
    char* str;
    short col;
} Output;


/*********************************
        Function Prototypes
*********************************/

static void termthread();
static void handle_input();
static void scroll_output(int value);
static void refresh_output();

#ifdef LINUX
static void handle_resize(int sig);
#endif


/*********************************
             Globals
*********************************/

// Thread globals
static std::thread thread_input;
static std::thread thread_terminate;

// NCurses globals
bool    local_usecurses     = true;
WINDOW* local_terminal      = NULL;
WINDOW* local_inputwin      = NULL;
WINDOW* local_outputwin     = NULL;
WINDOW* local_scrolltextwin = NULL;
static std::atomic<bool> local_resizesignal (false);

// Output window globals
static std::atomic<int> local_padbottom (-1);
static std::atomic<int> local_scrolly (0);
static std::queue<Output*> local_mesgqueue;
static uint32_t local_historysize = DEFAULT_HISTORYSIZE;

// Input window globals
static bool     local_allowinput = true;
static char     local_input[255];
static int      local_inputcount = 0;
static bool     local_showcurs   = true;
static uint64_t local_blinktime  = 0;


/*==============================
    term_initialize
    Initializes n/pdcurses for fancy
    terminal control 
==============================*/

void term_initialize()
{
    if (local_usecurses)
    {
        int w, h;

        // Initialize Curses
        setlocale(LC_ALL, "");
        local_terminal = initscr();
        if (local_terminal == NULL)
        {
            fputs("Error: Curses failed to initialize the screen.\n", stderr);
            exit(EXIT_FAILURE);
        }
        start_color();
        use_default_colors();
        noecho();
        keypad(stdscr, TRUE);
        getmaxyx(local_terminal, h, w);
        curs_set(FALSE);

        // Initialize signal if using Linux
#ifdef LINUX
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = handle_resize;
        sigaction(SIGWINCH, &sa, NULL);
#endif

        // Setup our console windows
        local_outputwin = newpad(h + local_historysize, w);
        local_inputwin = newwin(1, w, h - 1, 0);
        scrollok(local_outputwin, TRUE);
        idlok(local_outputwin, TRUE);
        keypad(local_inputwin, TRUE);
        wtimeout(local_inputwin, 0);
#ifdef LINUX
        set_escdelay(0);
#endif
        refresh_output();
        wrefresh(local_inputwin);

        // Initialize the input data
        memset(local_input, 0, 255);

        // Initialize the colors
        init_pair(CR_RED, COLOR_RED, -1);
        init_pair(CR_GREEN, COLOR_GREEN, -1);
        init_pair(CR_BLUE, COLOR_BLUE, -1);
        init_pair(CR_YELLOW, COLOR_YELLOW, -1);
        init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);

        // Create the terminal thread
        thread_input = std::thread(termthread);
    }
}


/*==============================
    term_end
    Stops curses safetly
==============================*/

void term_end()
{
    if (local_terminal != NULL)
    {
        thread_input.join();

        // Remove colors
        for (int i=0; i<TOTAL_COLORS; i++)
            wattroff(local_outputwin, COLOR_PAIR(i + 1));

        // End curses
        curs_set(TRUE);
        endwin();
    }
}


/*==============================
    termthread
    Thread logic for I/O
==============================*/

static void termthread()
{
    while (global_progstate != Terminating)
    {
        bool wroteout = false;

        // Output stuff
        while (!local_mesgqueue.empty())
        {
            Output* msg = local_mesgqueue.front();

            // Disable all the colors
            for (int i=0; i<TOTAL_COLORS; i++)
                wattroff(local_outputwin, COLOR_PAIR(i+1));

            // If a color is specified, use it
            if (msg->col != CR_NONE)
                wattron(local_outputwin, COLOR_PAIR(msg->col));

            // Print the string and its args
            wprintw(local_outputwin, "%s", msg->str);
            wroteout = true;

            // Cleanup
            local_mesgqueue.pop();
            free(msg->str);
        }

        // Refresh if needed
        if (wroteout || local_resizesignal)
            refresh_output();

        // Deal with input
        if (local_allowinput)
            handle_input();

        // Handle the signal flag
        if (local_resizesignal)
            local_resizesignal = false;

        // Sleep for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


/*==============================
    __log_output
    Fancy prints stuff to the output
    pad
    @param The color to use
    @param The string to print
    @param Variable arguments to print
==============================*/

void __log_output(const short color, const char* str, ...)
{
    va_list args;
    va_start(args, str);

    // Perform the print
    if (local_terminal != NULL)
    {
        Output* mesg = (Output*)malloc(sizeof(Output));
        mesg->col = color;
        mesg->str = (char*)malloc(vsnprintf(NULL, 0, str, args) + 1);
        va_end(args); 

        va_start(args, str);
        vsprintf(mesg->str, str, args);
        local_mesgqueue.push(mesg);
    }
    else
        vprintf(str, args);
    va_end(args);

    // Print to the output debug file if it exists
    if (global_debugoutptr != NULL)
    {
        va_start(args, str);
        vfprintf(global_debugoutptr, str, args);
        va_end(args);
    }
}


/*==============================
    terminate
    Stops the program and prints "Press any key to continue..."
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void terminate(const char* reason, ...)
{
    va_list args;
    va_start(args, reason);

    // Print why we're ending
    if (reason != NULL && strcmp(reason, ""))
    {
        char temp[255];
        vsprintf(temp, reason, args);
        log_colored("Error: %s", CRDEF_ERROR, temp);
    }
    log_colored("\n\n", CRDEF_ERROR);
    va_end(args);

    // Close output debug file if it exists
    if (global_debugoutptr != NULL)
    {
        fclose(global_debugoutptr);
        global_debugoutptr = NULL;
    }

    // Pause the program
    log_colored("Press any key to continue...\n", CRDEF_INPUT);
    if (local_terminal == NULL)
    {
        #ifndef LINUX
            system("pause > nul");
        #else
            struct termios info, orig;
            tcgetattr(0, &info);
            tcgetattr(0, &orig);
            info.c_lflag &= ~(ICANON | ECHO);
            info.c_cc[VMIN] = 1;
            info.c_cc[VTIME] = 0;
            tcsetattr(0, TCSANOW, &info);
            getchar();
            tcsetattr(0, TCSANOW, &orig);
        #endif
    }
    else
        getch();

    // End
    global_progstate = Terminating;
    term_end();
    exit(-1);
}


#ifdef LINUX
    /*==============================
        handle_resize
        Handles the resize signal
        @param Unused
    ==============================*/

    static void handle_resize(int)
    {
        endwin();
        clear(); // This re-initializes ncurses, no need to call newwin

        local_resizesignal = true;
    }
#endif


/*==============================
    refresh_output
    Refreshes the output pad to
    deal with scrolling
==============================*/

static void refresh_output()
{
    int w, h, x, y;
    static int scrolltextlen = 0;
    static int scrolltextlen_old = 0;

    // Get the terminal size
    getmaxyx(local_terminal, h, w);

    // Initialize padbottom if it hasn't been yet
    if (local_padbottom == -1)
        local_padbottom = h-2;

    // Get the cursor position
    getyx(local_outputwin, y, x);

    // Perform scrolling if needed
    if (y > local_padbottom)
    {
        int newlinecount = y - local_padbottom;
        local_padbottom += newlinecount;
        if (local_scrolly != 0)
            local_scrolly += newlinecount;
    }

    // Refresh the output window
    prefresh(local_outputwin, local_padbottom - (h-2) - local_scrolly, 0, 0, 0, h-2, w-1);

    // Print scroll text
    if (local_scrolly != 0)
    {
        char scrolltext[40 + 1];

        // Initialize the scroll text and the window to render the text to
        sprintf(scrolltext, "%d/%d", local_padbottom.load() - local_scrolly.load(), local_padbottom.load());
        scrolltextlen = strlen(scrolltext);

        // Initialize the scroll text window if necessary
        if (local_scrolltextwin == NULL)
        {
            local_scrolltextwin = newwin(1, scrolltextlen, h-2, w-scrolltextlen);
            wattron(local_scrolltextwin, COLOR_PAIR(CRDEF_SPECIAL));
        }

        // Handle terminal resize/text size change
        if (local_resizesignal || scrolltextlen_old != scrolltextlen)
        {
            getmaxyx(local_terminal, h, w);
            wresize(local_scrolltextwin, 1, scrolltextlen);
            mvwin(local_scrolltextwin, h-2, w-scrolltextlen);
        }

        // Print the scroll text
        wclear(local_scrolltextwin);
        wprintw(local_scrolltextwin, "%s", scrolltext);
        wrefresh(local_scrolltextwin);
        scrolltextlen_old = scrolltextlen;
    }
    else if (local_scrolltextwin != NULL)
    {
        delwin(local_scrolltextwin);
        local_scrolltextwin = NULL;
        scrolltextlen = 0;
        scrolltextlen_old = 0;
    }
}


/*==============================
    handle_input
    Reads user input using curses
==============================*/

static void handle_input()
{
    int ch;
    bool wrotein = false;

    // Handle key presses
    ch = wgetch(local_inputwin);
    switch (ch)
    {
        case KEY_PPAGE: scroll_output(1); break;
        case KEY_NPAGE: scroll_output(-1); break;
        case KEY_HOME: scroll_output(local_padbottom); break;
        case KEY_END: scroll_output(-local_padbottom); break;
        case CH_ESCAPE: 
            scroll_output(-local_padbottom); 
            global_escpressed = true; 
            local_allowinput = false;
            // Intentional fallthrough
        case '\r':
        case CH_ENTER: 
            memset(local_input, 0, 255);
            local_inputcount = 0; 
            wrotein = true; 
            break;
        default: 
            if (ch != ERR && isascii(ch) && ch > 0x1F && local_inputcount < 255)
            {
                local_input[local_inputcount++] = (char)ch;
                local_showcurs = true;
                local_blinktime = time_miliseconds();
                wrotein = true;
            }
            break;
    }

    // Cursor blinking timer
    if ((time_miliseconds() - local_blinktime) > BLINKRATE)
    {
        wrotein = true;
        local_showcurs = !local_showcurs;
        local_blinktime = time_miliseconds();
    }
    
    // Handle terminal resize
    if (local_resizesignal)
    {
        int w, h;

        // Get the new terminal size
        getmaxyx(local_terminal, h, w);

        // Resize and reposition the input windo
        wresize(local_inputwin, 1, w);
        mvwin(local_inputwin, h-1, 0);
        wrotein = true;
    }

    // Write the input
    if (wrotein)
    {
        wclear(local_inputwin);
        wprintw(local_inputwin, "%s", local_input);

        // Cursor rendering
        if (local_showcurs)
        {
            #ifndef LINUX
                wprintw(local_inputwin, "%c", 219);
            #else
                wprintw(local_inputwin, "\xe2\x96\x88\n");
            #endif
        }

        // Refresh the input pad to show changes
        wrefresh(local_inputwin);
    }
}


/*==============================
    scroll_output
    Scrolls the window pad by a
    given amount.
    @param A number with how much to
           scroll by. Negative numbers
           scroll down, positive up.
==============================*/

static void scroll_output(int value)
{
    int h = getmaxy(local_terminal);

    // Check if we can scroll
    if (local_padbottom < h)
        return;

    // Perform the scrolling
    local_scrolly += value;
    if (local_scrolly > local_padbottom-(h-2))
        local_scrolly = local_padbottom.load()-(h-2);
    else if (local_scrolly < 0)
        local_scrolly = 0;

    // Refresh the output window to see the changes
    refresh_output();
}


/*==============================
    term_sethistorysize
    Sets the number of terminal lines
    to store for scrolling
    @param The scroll history size
==============================*/

void term_sethistorysize(int val)
{
    local_historysize = val;
}


/*==============================
    term_usecurses
    Enables/disable the use of curses
    @param Whether to enable/disable curses
==============================*/

void term_usecurses(bool val)
{
    local_usecurses = val;
}


/*==============================
    term_isusingcurses
    Checks if the program is using curses
    @return Whether the program is 
            using curses or not
==============================*/

bool term_isusingcurses()
{
    return local_usecurses;
}