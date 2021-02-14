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

void __pdprint(short color, const char* str, ...)
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

    // Print to the output debug file if it exists
    if (global_debugoutptr != NULL)
        vfprintf(global_debugoutptr, str, args);
    va_end(args);
}


/*==============================
    __pdprintw
    Prints text using PDCurses to a specific window. Don't use directly.
    @param The window to print to
    @param A color pair to use (use the CR_ macros)
    @param Whether to allow logging
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void __pdprintw(WINDOW *win, short color, char log, const char* str, ...)
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
    if (log)
    {
        refresh();
        wrefresh(win);
    }

    // Print to the output debug file if it exists
    if (log && global_debugoutptr != NULL)
        vfprintf(global_debugoutptr, str, args);
    va_end(args);
}


/*==============================
    __pdprint_v
    va_list version of pdprint. Don't use directly.
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param va_list with the arguments to print
==============================*/

static void __pdprint_v(short color, const char* str, va_list args)
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

    // Print to the output debug file if it exists
    if (global_debugoutptr != NULL)
        vfprintf(global_debugoutptr, str, args);
}


/*==============================
    __pdprint_replace
    Same as pdprint but overwrites the previous line. Don't use directly.
    @param A color pair to use (use the CR_ macros)
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void __pdprint_replace(short color, const char* str, ...)
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

    // Print to the output debug file if it exists
    if (global_debugoutptr != NULL)
        vfprintf(global_debugoutptr, str, args);
    va_end(args);
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
        pdprint("Error: ", CRDEF_ERROR);
        __pdprint_v(CRDEF_ERROR, reason, args);
    }
    pdprint("\n\n", CRDEF_ERROR);
    va_end(args);

    // Close output debug file if it exists
    if (global_debugoutptr != NULL)
    {
        fclose(global_debugoutptr);
        global_debugoutptr = NULL;
    }

    // Close the device if it's open
    if (device_isopen() && !global_closefail)
    {
        global_closefail = true; // Prevent infinite loop
        device_close();
    }

    // Pause the program
    if (global_timeout == 0)
    {
        pdprint("Press any key to continue...", CRDEF_INPUT);
        getchar();
    }

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

static void terminate_v(const char* reason, va_list args)
{
    int i;

    // Print why we're ending
    if (reason != NULL && strcmp(reason, ""))
    {
        pdprint("Error: ", CRDEF_ERROR);
        __pdprint_v(CRDEF_ERROR, reason, args);
    }
    pdprint("\n\n", CRDEF_ERROR);

    // Close output debug file if it exists
    if (global_debugoutptr != NULL)
    {
        fclose(global_debugoutptr);
        global_debugoutptr = NULL;
    }

    // Close the device if it's open
    if (device_isopen() && !global_closefail)
    {
        global_closefail = true; // Prevent infinite loop
        device_close();
    }

    // Pause the program
    if (global_timeout == 0)
    {
        pdprint("Press any key to continue...", CRDEF_INPUT);
        getchar();
    }

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

void testcommand(FT_STATUS status, const char* reason, ...)
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

void progressbar_draw(const char* text, short color, float percent)
{
    int i;
    int prog_size = 16;
	int blocks_done = (int)(percent*prog_size);

    // Print the head of the progress bar
    pdprint_replace("%s [", color, text);

    // Draw the progress bar itself
    #ifndef LINUX
        for(i=0; i<blocks_done; i++) 
            pdprint("%c", color, 219);
        for(; i<prog_size; i++) 
            pdprint("%c", color, 176);
    #else
        for(i=0; i<blocks_done; i++) 
            pdprint("%c", color, '#');
        for(; i<prog_size; i++) 
            pdprint("%c", color, '.');
    #endif

    // Print the butt of the progress bar
    pdprint("] %d%%\n", color, (int)(percent*100.0f));
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


/*==============================
    gen_filename
    Generates a unique ending for a filename
    Remember to free the memory when finished!
    @returns The unique string
==============================*/

#define DATESIZE 7*2+1
char* gen_filename()
{
    static int increment = 0;
    static int lasttime = 0;
    char* str = (char*) malloc(DATESIZE);
    int curtime = 0;
    time_t t = time(NULL);
    struct tm tm;
    struct tm* tmp = &tm;

    // Get the time
    tm = *localtime(&t);
    curtime = tmp->tm_hour*60*60+tmp->tm_min*60+tmp->tm_sec;

    // Increment the last value if two files were created at the same second
    if (lasttime != curtime)
    {
        increment = 0;
        lasttime = curtime;
    }
    else
        increment++;

    // Generate the string and return it
    sprintf(str, "%02d%02d%02d%02d%02d%02d%02d", 
                 (tmp->tm_year+1900)%100, tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, increment%100);
    return str;
}


/*==============================
    md5
    Returns a string with an MD5 hash of the inputted data
    Code from https://en.wikipedia.org/wiki/MD5#Pseudocode
    The hash calculation is wrong but it works for this purpose
    @param The data to hash
    @param The size of the data
    @returns A string with the hash
==============================*/

// Final hash to print
static char cichash[32+1];

// 's' specifies the per-round shift amounts
static u32 s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

// Use binary integer part of the sines of integers (Radians) as constants:
static u32 K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

// Helper function for hash calculation
static u32 leftrotate(u32 x, u32 c)
{
    return (x << c) | (x >> (32-c));
}

char* md5(u8 *buff, u32 len) 
{
    u32 i;
    u8 *msg = NULL;

    u32 a0 = 0x67452301;
    u32 b0 = 0xefcdab89;
    u32 c0 = 0x98badcfe;
    u32 d0 = 0x10325476;

    u32 finallen = ((((len + 8) / 64) + 1) * 64) - 8;
    u32 nbits = 8*len;
    u32 offset;

    msg = (u8*) calloc(finallen + 64, 1);
    if (msg == NULL)
        terminate("Unable to allocate memory for MD5 calculation.");
    memcpy(msg, buff, len);
    msg[len] = 128;
    memcpy(msg + finallen, &nbits, 4);

    for (offset=0; offset<finallen; offset += 64) 
    {
        u32* w = (u32*) (msg + offset);

        // Initialize hash value for this chunk:
        u32 A = a0;
        u32 B = b0;
        u32 C = c0;
        u32 D = d0;

        for (i=0; i<64; i++) 
        {
            u32 F, g;

            if (i < 16) 
            {
                F = (B & C) | ((~B) & D);
                g = i;
            }
            else if (i < 32)
            {
                F = (D & B) | ((~D) & C);
                g = (5*i + 1) % 16;
            }
            else if (i < 48)
            {
                F = B ^ C ^ D;
                g = (3*i + 5) % 16;          
            }
            else
            {
                F = C ^ (B | (~D));
                g = (7*i) % 16;
            }
            
            // Be wary of the below definitions of a,b,c,d
            F = F + A + K[i] + w[g];
            A = D;
            D = C;
            C = B;
            B = B + leftrotate(F, s[i]);
        }

        // Add this chunk's hash to result so far:
        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }

    // Cleanup and return the hash
    free(msg);
    sprintf(cichash, "%08X%08X%08X%08X", a0, b0, c0, d0);
    return cichash;
}

/*==============================
    cic_from_hash
    Returns a CIC value from the hash string
    Please note that, since my MD5 function is wrong, these
    hashes are too.
    @param The string with the hash
    @returns The global_cictype value
==============================*/

s8 cic_from_hash(char* hash)
{
    if (!strcmp(hash, "0D21AD663C036767A0169918FF66DFE6"))
        return 0;
    if (!strcmp(hash, "FCEA1DADA68056B7A5540BC333865792"))
        return 1;
    if (!strcmp(hash, "0D21AD663C036767A0169918FF66DFE6"))
        return 2;
    if (!strcmp(hash, "405C84158EE479B5E9F381F853697686"))
        return 3;
    if (!strcmp(hash, "D0F36A72A1724A25389CAF95CD94BA98"))
        return 4;
    if (!strcmp(hash, "6501ED9FBDA950C672130BF14035AC57"))
        return 5;
    if (!strcmp(hash, "920275BB13E7566DF6F834651BB4A0F9"))
        return 6;
    if (!strcmp(hash, "FC75BA10B1F3708EE5439FDDC68806C0"))
        return 7;
    return -1;
}