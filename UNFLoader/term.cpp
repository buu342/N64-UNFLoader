/***************************************************************
                            main.cpp
                               
Handles terminal I/O, both with and without curses.
***************************************************************/ 

#include "main.h"
#include "helper.h"
#include "term.h"
#include "debug.h"
#ifndef LINUX
    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
#else
    #include <curses.h>
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
#include <list>
#include <iterator>


/*********************************
              Macros
*********************************/

#define BLINKRATE   500
#define MAXINPUT    256

#define CH_ESCAPE    27
#define CH_ENTER     '\n'
#define CH_BACKSPACE '\b'


/*********************************
            Structures
*********************************/

typedef struct {
    char*   str;
    short   col;
    int32_t y;
    bool    stack;
} Output;


/*********************************
        Function Prototypes
*********************************/

#define ctrl(a) (a & 0x1F)
static void termthread();
static void termthread_simple();
static void handle_input();
static void scroll_output(int value);
static void refresh_output();
static void refresh_input();
static void term_clearinput();
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
int     local_termwforced   = -1;
int     local_termhforced   = -1;
static std::atomic<bool> local_resizesignal (false);
static std::atomic<bool> local_keypressed (false);

// Output window globals
static std::atomic<int> local_padbottom (-1);
static std::atomic<int> local_scrolly (0);
static std::queue<Output*> local_mesgqueue;
static uint32_t local_historysize = DEFAULT_HISTORYSIZE;
static char* local_laststackable = NULL;
static int   local_stackcount = 0;
static bool  local_allowstack = true;

// Input window globals
static std::atomic<bool> local_allowinput(true);
static char     local_input[MAXINPUT];
static int      local_inputcount   = 0;
static int      local_inputcurspos = 0;
static bool     local_showcurs     = true;
static uint64_t local_blinktime    = 0;
static int      local_inputpadleft = 0;

// Input window history
static std::list<char*> local_inputhistory;
static std::list<char*>::iterator local_currhistory;


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
        if (local_termwforced > 0 || local_termwforced > -1)
        {
            w = local_termwforced;
            h = local_termhforced;
        }
        else
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
        local_outputwin = newpad(h+local_historysize, w);
        local_inputwin = newpad(1, MAXINPUT);
        scrollok(local_outputwin, TRUE);
        idlok(local_outputwin, TRUE);
        keypad(local_inputwin, TRUE);
        wtimeout(local_inputwin, 0);
        #ifdef LINUX
            set_escdelay(0);
        #endif
        refresh_output();
        refresh_input();

        // Initialize local variables
        memset(local_input, 0, MAXINPUT);
        local_inputhistory.push_back((char*)"");
        local_currhistory = local_inputhistory.begin();

        // Initialize the colors
        init_pair(CR_RED, COLOR_RED, -1);
        init_pair(CR_GREEN, COLOR_GREEN, -1);
        init_pair(CR_BLUE, COLOR_BLUE, -1);
        init_pair(CR_YELLOW, COLOR_YELLOW, -1);
        init_pair(CR_MAGENTA, -1, COLOR_MAGENTA);
        wattron(local_inputwin, COLOR_PAIR(CR_GREEN));

        // Create the terminal thread
        thread_input = std::thread(termthread);
    }
    else
    {
        thread_input = std::thread(termthread_simple);
        thread_input.detach();
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
    while (!global_terminating)
    {
        bool wroteout = false;

        // If a resize message was received, clear the screen to redraw it all again
        if (local_resizesignal)
            refresh();

        // Output stuff
        while (!local_mesgqueue.empty())
        {
            Output* msg = local_mesgqueue.front();

            // Disable all the colors
            for (int i=0; i<TOTAL_COLORS; i++)
                wattroff(local_outputwin, COLOR_PAIR(i+1));

            // Handle message stacking
            if (local_allowstack && msg->stack)
            {
                if (local_laststackable != NULL && !strcmp(local_laststackable, msg->str))
                {
                    local_stackcount++;
                    free(msg->str);
                    if (local_stackcount == 1)
                    {
                        msg->str = (char*)malloc(snprintf(NULL, 0, "\nPrevious message duplicated 1 time(s)\n") + 1);
                        sprintf(msg->str, "\nPrevious message duplicated 1 time(s)\n");
                        msg->y = 0;
                    }
                    else
                    {
                        msg->str = (char*)malloc(snprintf(NULL, 0, "Previous message duplicated %d time(s)\n", local_stackcount) + 1);
                        sprintf(msg->str, "Previous message duplicated %d time(s)\n", local_stackcount);
                        msg->y = 1;
                    }

                    msg->col = CRDEF_INFO;
                }
                else
                {
                    if (local_laststackable != NULL)
                        free(local_laststackable);
                    local_laststackable = (char*)malloc(strlen(msg->str) + 1);
                    strcpy(local_laststackable, msg->str);
                    local_stackcount = 0;
                }
            }
            else
            {
                if (local_laststackable != NULL)
                    free(local_laststackable);
                local_laststackable = NULL;
                local_stackcount = 0;
            }

            // If a color is specified, use it
            if (msg->col != CR_NONE)
                wattron(local_outputwin, COLOR_PAIR(msg->col));

            // If a y offset is given, then perform a replacement
            if (msg->y != 0)
            {
                int y, x;
                getyx(local_outputwin, y, x);
                wmove(local_outputwin, y-msg->y, x);
                refresh_output();
            }

            // Print the string and its args
            wprintw(local_outputwin, "%s", msg->str);
            wroteout = true;

            // Cleanup
            local_mesgqueue.pop();
            free(msg->str);
            free(msg);
        }

        // Refresh if needed
        if (wroteout || local_resizesignal)
            refresh_output();

        // Deal with input
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
    @param The Y offset to replace
    @param Allow stacking this message
    @param The string to print
    @param Variable arguments to print
==============================*/

void __log_output(const short color, const int32_t y, const bool allowstack, const char* str, ...)
{
    va_list args;
    va_start(args, str);

    // Perform the print
    if (local_terminal != NULL)
    {
        Output* mesg = (Output*)malloc(sizeof(Output));
        if (mesg == NULL)
            return;
        mesg->col = color;
        mesg->y = y;
        mesg->stack = allowstack;
        mesg->str = (char*)malloc(vsnprintf(NULL, 0, str, args) + 1);
        va_end(args); 
        if (mesg->str == NULL)
            return;

        va_start(args, str);
        vsprintf(mesg->str, str, args);
        local_mesgqueue.push(mesg);
    }
    else
        vprintf(str, args);
    va_end(args);

    // Print to the output debug file if it exists
    if (debug_getdebugout() != NULL)
    {
        va_start(args, str);
        vfprintf(debug_getdebugout(), str, args);
        va_end(args);
    }
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
        // Clear doesn't do anything until refresh(), which can't be called here because threads

        local_resizesignal = true;
        local_padbottom = -1;
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
        sprintf(scrolltext, "%d/%d", local_padbottom.load() - local_scrolly.load()-h+2, local_padbottom.load()-h+2);
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
    refresh_input
    Refreshes the input pad to
    deal with scrolling
==============================*/

static void refresh_input()
{
    int w, h;

    // Get the terminal size
    getmaxyx(local_terminal, h, w);

    // Correct the pad left bound
    if (local_inputcurspos < local_inputpadleft)
        local_inputpadleft = local_inputcurspos;
    if (local_inputcurspos > w+local_inputpadleft-1)
        local_inputpadleft = local_inputcurspos-w+1;

    // Refresh
    prefresh(local_inputwin, 0, local_inputpadleft, h-1, 0, h-1, w-1);
}


/*==============================
    handle_input
    Reads user input using curses
==============================*/

static void handle_input()
{
    int ch;
    char* s = NULL;
    bool wrotein = false;

    // Handle key presses
    ch = wgetch(local_inputwin);
    local_keypressed = (ch > 0);
    switch (ch)
    {
        case KEY_PPAGE: scroll_output(1); break;
        case KEY_NPAGE: scroll_output(-1); break;
        case KEY_HOME: scroll_output(local_padbottom); break;
        case KEY_END: scroll_output(-local_padbottom); break;
        case KEY_DOWN:
            if (!local_allowinput)
                break;
            local_currhistory++;
            if (local_currhistory == local_inputhistory.end())
                local_currhistory = local_inputhistory.begin();
            term_clearinput();
            strcpy(local_input, *local_currhistory);
            local_inputcount = strlen(local_input);
            local_inputcurspos = local_inputcount;
            local_inputpadleft = 0;
            wrotein = true;
            local_showcurs = true;
            local_blinktime = time_miliseconds();
            break;
        case KEY_UP:
            if (!local_allowinput)
                break;
            if (local_currhistory == local_inputhistory.begin())
                local_currhistory = local_inputhistory.end();
            local_currhistory--;
            term_clearinput();
            strcpy(local_input, *local_currhistory);
            local_inputcount = strlen(local_input);
            local_inputcurspos = local_inputcount;
            local_inputpadleft = 0;
            wrotein = true;
            local_showcurs = true;
            local_blinktime = time_miliseconds();
            break;
        case KEY_LEFT:
            if (!local_allowinput)
                break;
            local_inputcurspos--;
            if (local_inputcurspos < 0)
                local_inputcurspos = 0;
            wrotein = true;
            local_showcurs = true;
            local_blinktime = time_miliseconds();
            break;
        case KEY_RIGHT:
            if (!local_allowinput)
                break;
            local_inputcurspos++;
            if (local_inputcurspos > local_inputcount)
                local_inputcurspos = local_inputcount;
            wrotein = true;
            local_showcurs = true;
            local_blinktime = time_miliseconds();
            break;
        case KEY_DC: // Del key
            if (!local_allowinput)
                break;
            if (local_inputcurspos != local_inputcount)
            {
                for (int i= local_inputcurspos; i<local_inputcount; i++)
                    local_input[i] = local_input[i+1];
                local_input[local_inputcount] = 0;
                local_inputcount--;
                local_input[local_inputcount] = 0;
                wrotein = true;
                local_showcurs = true;
                local_blinktime = time_miliseconds();
            }
            break;
        case 263:
        case CH_BACKSPACE:
            if (!local_allowinput)
                break;
            if (local_inputcurspos > 0)
            {
                for (int i=local_inputcurspos; i<local_inputcount; i++)
                    local_input[i-1] = local_input[i];
                local_input[local_inputcount] = 0;
                local_inputcount--;
                local_input[local_inputcount] = 0;
                local_inputcurspos--;
                wrotein = true;
                local_showcurs = true;
                local_blinktime = time_miliseconds();
            }
            break;
        case CH_ESCAPE:
            scroll_output(-local_padbottom);
            program_event(PEV_ESCAPE);
            local_keypressed = false;
            term_clearinput();
            wrotein = true;
            break;
        case '\r':
        case CH_ENTER:
            if (!local_allowinput || local_inputcount == 0)
                break;
            s = (char*)malloc(local_inputcount+1);
            strcpy(s, local_input);
            if (local_currhistory == local_inputhistory.begin())
                local_inputhistory.push_back(s);
            local_currhistory = local_inputhistory.begin();
            debug_sendtext(local_input);
            term_clearinput();
            wrotein = true;
            break;
        default:
            if (!local_allowinput)
                break;
            if (ch == ctrl('r'))
            {
                program_event(PEV_REUPLOAD);
                break;
            }
            if (ch != ERR && isascii(ch) && ch > 0x1F && local_inputcount < MAXINPUT-1)
            {
                if (local_inputcurspos != local_inputcount)
                    for (int i=local_inputcount; i> local_inputcurspos; i--)
                        local_input[i] = local_input[i-1];
                local_currhistory = local_inputhistory.begin();
                local_input[local_inputcurspos] = (char)ch;
                local_showcurs = true;
                local_blinktime = time_miliseconds();
                wrotein = true;
                local_inputcount++;
                local_inputcurspos++;
            }
            break;
    }

    // No point in reading further if input is disabled
    if (!local_allowinput)
        return;

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
        int w = getmaxx(local_terminal);
        wresize(local_inputwin, 1, w);
        wrotein = true;
    }

    // Write the input
    if (wrotein)
    {
        wclear(local_inputwin);
        if (local_allowinput)
        {
            int y, x;
            wprintw(local_inputwin, "%s", local_input);

            // Move the cursor
            getyx(local_inputwin, y, x);
            wmove(local_inputwin, y, local_inputcurspos);

            // Cursor rendering
            if (local_showcurs)
            {
                #ifndef LINUX
                    wprintw(local_inputwin, u8"\u2588");
                #else
                    wprintw(local_inputwin, "\xe2\x96\x88");
                #endif
            }
            wmove(local_inputwin, y, x);
        }

        // Refresh the input pad to show changes
        refresh_input();
    }
}


/*==============================
    termthread_simple
    Reads user input without using curses
==============================*/

static void termthread_simple()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    while (!global_terminating)
    {
        // Block until the user presses enter
        if (local_allowinput && fgets(local_input, MAXINPUT-1, stdin) != NULL && !global_terminating)
        {
            // Remove trailing newline
            local_input[strcspn(local_input, "\r\n")] = 0;

            // Handle input
            if (strlen(local_input) > 0)
            {
                if (!strcmp(local_input, "exit") || !strcmp(local_input, "cancel"))
                    program_event(PEV_ESCAPE);
                else if (!strcmp(local_input, "reupload"))
                    program_event(PEV_REUPLOAD);
                else
                    debug_sendtext(local_input);
            }

            // Cleanup
            memset(local_input, 0, MAXINPUT);
        }
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
    term_clearinput
    Clears the terminal input
==============================*/

static void term_clearinput()
{
    memset(local_input, 0, MAXINPUT);
    local_inputcurspos = 0;
    local_inputcount = 0;
    local_inputpadleft = 0;
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
    Enables/disables the use of curses
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


/*==============================
    term_allowinput
    Enables/disables input reading
    @param Whether to enable/disable 
           input reading
==============================*/

void term_allowinput(bool val)
{
    local_allowinput = val;
    term_clearinput();
    wclear(local_inputwin);
    refresh_input();
}


/*==============================
    term_initsize
    Forces the terminal to initialize with
    the given size.
    @param The initial height
    @param The initial width
==============================*/

void term_initsize(int h, int w)
{
    if (!local_usecurses)
        return;
    local_termwforced = w;
    local_termhforced = h;
}


/*==============================
    term_setsize
    Forces the terminal to a specific size
    @param The number of rows
    @param The number of columns
==============================*/

void term_setsize(int h, int w)
{
    if (!local_usecurses)
        return;
    resize_term(h, w);
    wresize(local_terminal, h, w);
    wresize(local_outputwin, h + local_historysize, w);
    wresize(local_inputwin, 1, w);
    mvwin(local_inputwin, h-1, 0);
    wrefresh(local_terminal);
    refresh_input();
    local_padbottom = -1;
    local_scrolly = 0;
    local_resizesignal = true;
}


/*==============================
    term_enablestacking
    Enables/disables stacking printf
    messages
==============================*/

void term_enablestacking(bool val)
{
    local_allowstack = val;
}


/*==============================
    term_getw
    Gets the terminal's width
    @return The terminal's width
==============================*/

int term_getw()
{
    return getmaxx(local_terminal);
}


/*==============================
    term_geth
    Gets the terminal's height
    @return The terminal's height
==============================*/

int term_geth()
{
    return getmaxy(local_terminal);
}


/*==============================
    term_waskeypressed
    Checks if a key was pressed recently
    @return Whether a key was pressed recently
==============================*/

bool term_waskeypressed()
{
    return local_keypressed.load();
}