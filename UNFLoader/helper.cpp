#include "main.h"
#include "helper.h"


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
    log_colored("Press any key to continue...", CRDEF_INPUT);
    #ifndef LINUX
        system("pause");
    #else
        system("read");
    #endif

    // Cleanup curses
    if (global_usecurses && global_terminal != NULL)
    {
        // End the program
        for (i = 0; i < TOTAL_COLORS; i++)
            wattroff(global_outputwin, COLOR_PAIR(i + 1));
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

    // Get the cursor position
    getyx(global_outputwin, y, x);

    // Refresh the output window
    prefresh(global_outputwin, 0, 0, 0, 0, h-2, w-1);
}