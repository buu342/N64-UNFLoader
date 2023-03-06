#include "main.h"
#include "helper.h"
#include <atomic>
#include <algorithm>
#ifdef LINUX
    #include <termios.h>
#endif

static std::atomic<int> local_padbottom (-1);
static std::atomic<int> local_scrolly (0);


/*==============================
    __log_output
    Fancy prints stuff to the output
    pad
    @param The color to use
    @param The string to print
    @param Variable arguments to print
==============================*/

void __log_output(short color, const char* str, ...)
{
    va_list args;
    va_start(args, str);

    // Perform the print
    if (global_usecurses && global_terminal != NULL)
    {
        int x, y;

        // Disable all the colors
        for (int i=0; i<TOTAL_COLORS; i++)
            wattroff(global_outputwin, COLOR_PAIR(i+1));

        // If a color is specified, use it
        if (color != CR_NONE)
            wattron(global_outputwin, COLOR_PAIR(color));

        // Print the string and its args
        vw_printw(global_outputwin, str, args);
        refresh_output();
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
    int i;
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
    if (global_terminal == NULL)
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

    // Cleanup curses
    if (global_usecurses && global_terminal != NULL)
    {
        // End the program
        for (i = 0; i < TOTAL_COLORS; i++)
            wattroff(global_outputwin, COLOR_PAIR(i + 1));
        curs_set(TRUE);
        endwin();
    }
    exit(-1);
}


/*==============================
    handle_resize
    Handles the resize signal
    @param Unused
==============================*/

void handle_resize(int)
{
    int w, h;
    endwin();
    clear(); // This re-initializes ncurses, no need to call newwin

    // Get the new terminal size
    getmaxyx(global_terminal, h, w);

    // Refresh the output window
    refresh_output();

    // Resize and reposition the input window
    wresize(global_inputwin, 1, w);
    mvwin(global_inputwin, h-1, 0);
    wrefresh(global_inputwin);
}


/*==============================
    refresh_output
    Refreshes the output pad to
    deal with scrolling
==============================*/

void refresh_output()
{
    int w, h, x, y;

    // Get the terminal size
    getmaxyx(global_terminal, h, w);

    // Initialize padbottom if it hasn't been yet
    if (local_padbottom == -1)
        local_padbottom = h-2;

    // Get the cursor position
    getyx(global_outputwin, y, x);

    // Perform scrolling if needed
    if (y > local_padbottom)
    {
        int newlinecount = y - local_padbottom;
        local_padbottom += newlinecount;
        if (local_scrolly != 0)
            local_scrolly += newlinecount;
    }

    // Refresh the output window
    prefresh(global_outputwin, local_padbottom - (h-2) - local_scrolly, 0, 0, 0, h-2, w-1);

    // Print scroll text
    if (local_scrolly != 0)
    {
        int textlen;
        char scrolltext[40 + 1];
        WINDOW* scrolltextwin;

        // Initialize the scroll text and the window to render the text to
        sprintf(scrolltext, "%d/%d", local_padbottom.load() - local_scrolly.load(), local_padbottom.load());
        textlen = strlen(scrolltext);
        scrolltextwin = newwin(1, textlen, h-2, w-textlen);

        // Set the scroll text color
        wattron(scrolltextwin, COLOR_PAIR(CRDEF_SPECIAL));

        // Print the scroll text
        wprintw(scrolltextwin, "%s", scrolltext);
        wrefresh(scrolltextwin);
        delwin(scrolltextwin);
    }
}


/*==============================
    scroll_output
    TODO
==============================*/

void scroll_output(int value)
{
    int w, h;

    // Check if we can scroll
    getmaxyx(global_terminal, h, w);
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