/***************************************************************
                            helper.cpp
                               
Useful functions to use in conjunction with the program
***************************************************************/

#include "main.h"
#include "device.h"
#include "helper.h"


/*********************************
             Globals
*********************************/

static char* local_printhistory[PRINT_HISTORY_SIZE];


/*==============================
    __pdprint
    Prints text using PDCurses. Don't use directly.
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void __pdprint(short color, char* str, ...)
{
    int i;
    va_list args;
    va_start(args, str);

    // Disable all the colors
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));

    // If a color is specified, use it
    if (global_usecolors && color != CR_NONE)
        attron(COLOR_PAIR(color));

    // Print the string
    vw_printw(stdscr, str, args);
    refresh();
    va_end(args);
}


/*==============================
    __pdprintw
    Prints text using PDCurses to a specific window. Don't use directly.
    @param The window to print to
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void __pdprintw(WINDOW *win, short color, char* str, ...)
{
    int i;
    va_list args;
    va_start(args, str);

    // Disable all the colors
    for (i=0; i<TOTAL_COLORS; i++)
        wattroff(win, COLOR_PAIR(i+1));

    // If a color is specified, use it
    if (global_usecolors && color != CR_NONE)
        wattron(win, COLOR_PAIR(color));

    // Print the string
    vw_printw(win, str, args);
    refresh();
    wrefresh(win);
    va_end(args);
}


/*==============================
    __pdprint_v
    va_list version of pdprint. Don't use directly.
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param va_list with the arguments to print
==============================*/

static void __pdprint_v(short color, char* str, va_list args)
{
    int i;

    // Disable all the colors
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));

    // If a color is specified, use it
    if (global_usecolors && color != CR_NONE)
        attron(COLOR_PAIR(color));

    // Print the string
    vw_printw(stdscr, str, args);
    refresh();
}


/*==============================
    __pdprint_replace
    Same as pdprint but overwrites the previous line. Don't use directly.
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void __pdprint_replace(short color, char* str, ...)
{
    int i, xpos, ypos;
    va_list args;
    va_start(args, str);

    // Disable all the colors
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));

    // If a color is specified, use it
    if (global_usecolors && color != CR_NONE)
        attron(COLOR_PAIR(color));

    // Move the cursor back a line
    getsyx(ypos, xpos);
    move(ypos-1, 0);

    // Print the string
    vw_printw(stdscr, str, args);
    refresh();
    va_end(args);
}


/*==============================
    terminate
    Stops the program and prints "Press any key to continue..."
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void terminate(char* reason, ...)
{
    int i;
    va_list args;
    va_start(args, reason);

    // Print why we're ending
    if (reason != NULL && strcmp(reason, ""))
        __pdprint_v(CRDEF_ERROR, reason, args);

    // Close the device if it's open
    if (device_isopen())
        device_close();

    // Pause the program
    pdprint("\nPress any key to continue...", CRDEF_INPUT);
    va_end(args);
    getchar();

    // End the program
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));
    endwin();
    exit(-1);
}


/*==============================
    terminate_v
    va_list version of terminate
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

static void terminate_v(char* reason, va_list args)
{
    int i;

    // Print why we're ending
    if (reason != NULL && strcmp(reason, ""))
        __pdprint_v(CRDEF_ERROR, reason, args);

    // Close the device if it's open
    if (device_isopen())
        device_close();

    // Pause the program
    pdprint("\nPress any key to continue...", CRDEF_INPUT);
    getchar();

    // End the program
    for (i=0; i<TOTAL_COLORS; i++)
        attroff(COLOR_PAIR(i+1));
    endwin();
    exit(-1);
}


/*==============================
    testcommand
    Terminates the program if the command fails
    @param The return value from an FTDI function
    @param Text to print if the command failed
    @param Variadic arguments to print as well
==============================*/

void testcommand(FT_STATUS status, char* reason, ...)
{
    va_list args;
    va_start(args, reason);

    // Test the command
    if (status != FT_OK)
        terminate_v(reason, args);
    va_end(args);
}


/*==============================
    swap_endian
    Swaps the endianess of the data
    @param   The data to swap the endianess of
    @returns The data with endianess swapped
==============================*/

u32 swap_endian(u32 val)
{
	return ((val<<24) ) | 
		   ((val<<8)  & 0x00ff0000) |
		   ((val>>8)  & 0x0000ff00) | 
		   ((val>>24) );
}


/*==============================
    progressbar
    Draws a fancy progress bar
    @param The text to print before the progress bar
    @param The percentage of completion
==============================*/

void progressbar_draw(char* text, float percent)
{
    int i;
    int prog_size = 16;
	int blocks_done = (int)(percent*prog_size);
	int blocks_left = prog_size-blocks_done;

    // Print the head of the progress bar
    pdprint_replace("%s [", CRDEF_PROGRAM, text);

    // Draw the progress bar itself
	for(i=0; i<blocks_done; i++) 
        pdprint("%c", CRDEF_PROGRAM, 219);
	for(; i<prog_size; i++) 
        pdprint("%c", CRDEF_PROGRAM, 176);

    // Print the butt of the progress bar
    pdprint("] %d%%\n", CRDEF_PROGRAM, (int)(percent*100.0f));
}


/*==============================
    calc_padsize
    Returns the correct size a ROM should be. Code taken from:
    https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    @param The current ROM filesize
    @returns the correct ROM filesize
==============================*/

u32 calc_padsize(u32 size)
{
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
    return size;
}