#include "main.h"
#include "helper.h"
#include "term.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifndef LINUX
    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
#else
    #include <curses.h>
    #include <sys/time.h>
    #include <termios.h>
#endif

const char* cart_strings[] = {"64Drive HW1", "64Drive HW2", "EverDrive", "SC64"}; // In order of the CartType enums
const int   cart_strcount = sizeof(cart_strings)/sizeof(cart_strings[0]);
const char* cic_strings[] = {"6101", "6102", "7101", "7102", "X103", "X105", "X106", "5101"}; // In order of the CICType enums
const int   cic_strcount = sizeof(cic_strings)/sizeof(cic_strings[0]);
const char* save_strings[] = {"EEPROM 4Kbit", "EEPROM 16Kbit", "SRAM 256Kbit", "FlashRAM 1Mbit", "SRAM 768Kbit", "FlashRAM 1Mbit (PokeStdm2)"}; // In order of the SaveType enums
const int   save_strcount = sizeof(save_strings)/sizeof(save_strings[0]);

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
    log_colored("\n", CRDEF_ERROR);
    va_end(args);

    // Close output debug file if it exists
    if (global_debugoutptr != NULL)
    {
        fclose(global_debugoutptr);
        global_debugoutptr = NULL;
    }

    // Pause the program
    log_colored("Press any key to continue...\n", CRDEF_INPUT);
    if (!term_isusingcurses())
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


/*==============================
    time_miliseconds
    Retrieves the current system
    time in miliseconds
    @return The time in miliseconds
==============================*/

uint64_t time_miliseconds()
{
    #ifndef LINUX
        SYSTEMTIME st;
        GetSystemTime(&st);
        return (uint64_t)st.wMilliseconds;
    #else
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
    #endif
}


/*==============================
    cart_strtotype
    Reads a string and converts it 
    to a valid CartType enum
    @param  The string with the cart 
            to parse
    @return The CartType enum
==============================*/

CartType cart_strtotype(char* cartstring)
{
    // If the cart string is a single number, then it's pretty easy to get the cart enum
    if (cartstring[0] >= ('0'+((int)CART_64DRIVE1)) && cartstring[0] <= ('0'+((int)CART_SC64)) && cartstring[1] == '\0')
        return (CartType)(cartstring[0]-'0');

    // Check if the user, for some reason, wrote the entire CIC string out
    for (int i=0; i<cart_strcount; i++)
        if (!strcmp(cart_strings[i], cartstring))
            return (CartType)(i+1);

    // Otherwise, stop
    terminate("Unknown flashcart type '%s'", cartstring);
    return CART_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    cart_typetostr
    Reads a CartType enum and converts it
    to a nice string. Assumes a non-NONE
    CartType is given!
    @param  The enum with the cart 
            to convert
    @return The cart name stringified
==============================*/

const char* cart_typetostr(CartType cartenum)
{
    return cart_strings[(int)cartenum-1];
}


/*==============================
    cic_strtotype
    Reads a string and converts it 
    to a valid CICType enum
    @param  The string with the CIC 
            to parse
    @return The CICType enum
==============================*/

CICType cic_strtotype(char* cicstring)
{
    // If the CIC string is a single number, then it's pretty easy to get the CIC enum
    if (cicstring[0] >= ('0'+((int)CIC_6101)) && cicstring[0] <= ('0'+((int)CIC_5101)) && cicstring[1] == '\0')
        return (CICType)(cicstring[0]-'0');

    // Check if the user, for some reason, wrote the entire CIC string out
    for (int i=0; i<cic_strcount; i++)
        if (!strcmp(cic_strings[i], cicstring))
            return (CICType)i;

    // Otherwise, stop
    terminate("Unknown CIC '%s'", cicstring);
    return CIC_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    cic_typetostr
    Reads a CICType enum and converts it
    to a nice string. Assumes a non-NONE
    CICType is given!
    @param  The enum with the CIC 
            to convert
    @return The CIC name stringified
==============================*/

const char* cic_typetostr(CICType cicenum)
{
    return cic_strings[(int)cicenum];
}


/*==============================
    save_strtotype
    Reads a string and converts it 
    to a valid SaveType enum
    @param  The string with the save type
            to parse
    @return The SaveType enum
==============================*/

SaveType save_strtotype(char* savestring)
{
    // If the save string is a single number, then it's pretty easy to get the save enum
    if (savestring[0] >= ('0'+((int)SAVE_EEPROM4K)) && savestring[0] <= ('0'+((int)SAVE_FLASHRAMPKMN)) && savestring[1] == '\0')
        return (SaveType)(savestring[0]-'0');

    // Check if the user, for some reason, wrote the entire save string out
    for (int i=0; i<save_strcount; i++)
        if (!strcmp(save_strings[i], savestring))
            return (SaveType)(i+1);

    // Otherwise, stop
    terminate("Unknown save type '%s'", savestring);
    return SAVE_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    save_typetostr
    Reads a SaveType enum and converts it
    to a nice string. Assumes a non-NONE
    SaveType is given!
    @param  The enum with the save type
            to convert
    @return The save type name stringified
==============================*/

const char* save_typetostr(SaveType saveenum)
{
    return save_strings[(int)saveenum-1];
}