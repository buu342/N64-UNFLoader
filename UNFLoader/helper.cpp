#include "main.h"
#include "helper.h"
#include "term.h"
#include "device.h"
#include "debug.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef LINUX
    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
#else
    #include <curses.h>
    #include <sys/time.h>
    #include <termios.h>
#endif
#include <thread>
#include <chrono>


/*********************************
              Macros
*********************************/

#define SHOULDIE(a) global_badpackets ? terminate(a) : log_colored(a, CRDEF_ERROR)


/*********************************
             Globals
*********************************/

// Useful constants for converting between enums and strings
const char* cart_strings[] = {"64Drive HW1", "64Drive HW2", "EverDrive", "SC64"}; // In order of the CartType enums
const int   cart_strcount = sizeof(cart_strings)/sizeof(cart_strings[0]);
const char* cic_strings[] = {"6101", "6102", "7101", "7102", "X103", "X105", "X106", "5101"}; // In order of the CICType enums
const int   cic_strcount = sizeof(cic_strings)/sizeof(cic_strings[0]);
const char* save_strings[] = {"EEPROM 4Kbit", "EEPROM 16Kbit", "SRAM 256Kbit", "FlashRAM 1Mbit", "SRAM 768Kbit", "FlashRAM 1Mbit (PokeStdm2)"}; // In order of the SaveType enums
const int   save_strcount = sizeof(save_strings)/sizeof(save_strings[0]);


/*==============================
    terminate
    Stops the program and prints "Press any key to continue..."
    (if using curses)
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
    if (debug_getdebugout() != NULL)
        debug_closedebugout();

    // Close the flashcart if it's open
    if (device_isopen())
        device_close();

    // Pause the program
    if (term_isusingcurses())
    {
        uint64_t start = time_miliseconds();
        term_allowinput(false);
        if (get_timeout() != -1 && term_isusingcurses())
            log_colored("Press any key to continue, or wait for timeout.\n", CRDEF_INPUT);
        else if (get_timeout() != -1)
            log_colored("Program exiting in %d seconds.\n", CRDEF_INPUT, get_timeout());
        else
            log_colored("Press any key to continue...\n", CRDEF_INPUT);

        // Pause until timeout or a key is pressed
        while (!term_waskeypressed() && (get_timeout() == -1 || (get_timeout() != -1 && (time_miliseconds()-start)/1000 < (uint32_t)get_timeout())))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // End
    global_terminating = true;
    term_end();
    #ifndef LINUX
        if (strlen(global_gdbaddr) > 0)
            WSACleanup();
        TerminateProcess(GetCurrentProcess(), 0);
    #else
        exit(0);
    #endif
}


/*==============================
    pauseprogram
    Pauses the program and prints "Press any key to continue..."
    without curses
==============================*/

void pauseprogram()
{
    log_colored("Press any key to continue...\n", CRDEF_INPUT);
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

    // End
    global_terminating = true;
    exit(-1);
}


/*==============================
    progressthread
    Draws the upload progress bar
    in a separate thread
    @param The message to print next to the progress bar
==============================*/

void progressthread(const char* msg)
{
    int esclevel = get_escapelevel();
    float lastprog = 0;

    // Wait for the upload to finish
    while(device_getuploadprogress() < 99.99f && !device_uploadcancelled())
    {
        // If the device was closed, stop
        if (!device_isopen())
            return;

        // Draw the progress bar
        if (device_getuploadprogress() != lastprog)
        {
            progressbar_draw(msg, CRDEF_INPUT, device_getuploadprogress() / 100.0f);
            lastprog = device_getuploadprogress();
        }

        // Handle upload cancelling
        if (get_escapelevel() < esclevel)
        {
            device_cancelupload();
            break;
        }

        // Sleep for a bit to be kind to the CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


/*==============================
    progressbar_draw
    Draws a fancy progress bar
    @param The text to print before the progress bar
    @param The color to draw the progress bar with
    @param The percentage of completion, from 0 to 1
==============================*/

void progressbar_draw(const char* text, short color, float percent)
{
    int i;
    int prog_size = 16;
    int blocks_done = (int)(percent*prog_size);

    // Print the head of the progress bar
    log_replace("%s [", color, text);

    // Draw the progress bar itself
    for(i=0; i<blocks_done; i++) 
    {
        #ifndef LINUX
            log_colored(u8"\u2588", color);
        #else
            log_colored("\xe2\x96\x88", color);
        #endif
    }
    for(; i<prog_size; i++) 
    {
        #ifndef LINUX
            log_colored(u8"\u2591", color);
        #else
            log_colored("\xe2\x96\x91", color);
        #endif
    }

    // Print the butt of the progress bar
    log_colored("] %.02f%%\n", color, percent*100.0f);
}


/*==============================
    time_miliseconds
    Retrieves the current system
    time in miliseconds.
    Needed because clock() wasn't
    working properly???
    @return The time in miliseconds
==============================*/

uint64_t time_miliseconds()
{
    #ifndef LINUX
        return (uint64_t)GetTickCount();
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
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

CartType cart_strtotype(const char* cartstring)
{
    // If the cart string is a single number, then it's pretty easy to get the cart enum
    if (cartstring[0] >= ('0'+((int)CART_64DRIVE1)) && cartstring[0] <= ('0'+((int)CART_SC64)) && cartstring[1] == '\0')
        return (CartType)(cartstring[0]-'0');

    // Check if the user, for some reason, wrote the entire cart string out
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

CICType cic_strtotype(const char* cicstring)
{
    // If the CIC string is a single number, then it's pretty easy to get the CIC enum
    if (cicstring[0] >= ('0'+((int)CIC_6101)) && cicstring[0] <= ('0'+((int)CIC_5101)) && cicstring[1] == '\0')
    {
        return (CICType)(cicstring[0]-'0');
    }

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

SaveType save_strtotype(const char* savestring)
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


/*==============================
    file_lastmodtime
    Gets the last modification time of
    a file. This is usually done via stat,
    but it is broken on WinXP:
    https://stackoverflow.com/questions/32452777/visual-c-2015-express-stat-not-working-on-windows-xp
    @param  Path to the file to check the 
            timestamp of
    @return The file modification time
==============================*/

time_t file_lastmodtime(const char* path)
{
    struct stat finfo;
    #ifndef LINUX
        LARGE_INTEGER lt;
        WIN32_FILE_ATTRIBUTE_DATA fdata;
        GetFileAttributesExA(path, GetFileExInfoStandard, &fdata);
        lt.LowPart = fdata.ftLastWriteTime.dwLowDateTime;
        lt.HighPart = (long)fdata.ftLastWriteTime.dwHighDateTime;
        finfo.st_mtime = (time_t)(lt.QuadPart*1e-7);
    #else
        stat(path, &finfo);
    #endif
    return finfo.st_mtime;
}


/*==============================
    gen_filename
    Generates a unique ending for a filename
    Remember to free the memory when finished!
    @param  The filename
    @param  The file extension
    @return The unique string
==============================*/

#define DATESIZE 64//7*2+1
char* gen_filename(const char* filename, const char* fileext)
{
    static int increment = 0;
    static int lasttime = 0;
    char* finalname = NULL;
    char* extraname = NULL;
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

    // Generate the unique string
    extraname = (char*)malloc(DATESIZE);
    if (extraname == NULL)
        return NULL;
    sprintf(extraname, "%02d%02d%02d%02d%02d%02d%02d", 
                 (tmp->tm_year+1900)%100, tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, increment%100);

    // Generate the final name
    if (debug_getbinaryout() != NULL)
    {
        finalname = (char*)calloc(snprintf(NULL, 0, "%s%s-%s.%s", debug_getbinaryout(), filename, extraname, fileext) + 1, 1);
        if (finalname == NULL)
            return NULL;
        sprintf(finalname, "%s%s-%s.%s", debug_getbinaryout(), filename, extraname, fileext);
    }
    else
    {
        finalname = (char*)calloc(snprintf(NULL, 0, "%s-%s.%s", filename, extraname, fileext) + 1, 1);
        if (finalname == NULL)
            return NULL;
        sprintf(finalname, "%s-%s.%s", filename, extraname, fileext);
    }
    free(extraname);
    return finalname;
}


/*==============================
    trimwhitespace
    Removes the trailing whitespace from
    the start and end of a string.
    @param  The filename
    @param  The file extension
    @return The starting pointer of the string
==============================*/

char* trimwhitespace(char* str)
{
    char* end;
    size_t size = strlen(str);

    // Ignore zero length strings
    if (size == 0)
        return str;

    // Trim whitespace at the back of the string
    end = str + size - 1;
    while (end >= str && isspace(*end))
        end--;
    *(end + 1) = '\0';

    // Find the first non-whitespace character
    while (*str && isspace(*str))
        str++;

    // Return the new starting pointer
    return str;
}


/*==============================
    handle_deviceerror
    Stops the program with a useful
    error message if the deive encounters
    an error
==============================*/

void handle_deviceerror(DeviceError err)
{
    switch(err)
    {
        case DEVICEERR_USBBUSY:
            terminate("USB Device not ready.");
            break;
        case DEVICEERR_NODEVICES:
            terminate("No FTDI USB devices found.");
            break;
        case DEVICEERR_CARTFINDFAIL:
            if (device_getcart() == CART_NONE)
            {
                #ifndef LINUX
                    terminate("No flashcart detected");
                #else
                    terminate("No flashcart detected. Are you running sudo?");
                #endif
            }
            else
                terminate("Requested flashcart not detected.");
            break;
        case DEVICEERR_CANTOPEN:
            terminate("Could not open USB device.");
            break;
        case DEVICEERR_RESETFAIL:
            terminate("Unable to reset USB device.");
            break;
        case DEVICEERR_RESETPORTFAIL:
            terminate("Unable to reset USB port.");
            break;
        case DEVICEERR_TIMEOUTSETFAIL:
            terminate("Unable to set flashcart timeouts.");
            break;
        case DEVICEERR_PURGEFAIL:
            terminate("Unable to purge USB contents.");
            break;
        case DEVICEERR_READFAIL:
            SHOULDIE("Unable to read from flashcart.");
            break;
        case DEVICEERR_WRITEFAIL:
            SHOULDIE("Unable to write to flashcart.");
            break;
        case DEVICEERR_WRITEZERO:
            SHOULDIE("Zero bytes were written to flashcart.");
            break;
        case DEVICEERR_CLOSEFAIL:
            terminate("Unable to close flashcart.");
            break;
        case DEVICEERR_FILEREADFAIL:
            terminate("Unable to read ROM contents.");
            break;
        case DEVICEERR_BITMODEFAIL_RESET:
            terminate("Unable to set reset bitmode.");
            break;
        case DEVICEERR_BITMODEFAIL_SYNCFIFO:
            terminate("Unable to set syncfifo bitmode.");
            break;
        case DEVICEERR_SETDTRFAIL:
            terminate("Unable to set DTR line.");
            break;
        case DEVICEERR_CLEARDTRFAIL:
            terminate("Unable to clear DTR line.");
            break;
        case DEVICEERR_TXREPLYMISMATCH:
            SHOULDIE("Actual bytes written amount is different than desired.");
            break;
        case DEVICEERR_READCOMPSIGFAIL:
            SHOULDIE("Unable to read completion signal.");
            break;
        case DEVICEERR_NOCOMPSIG:
            SHOULDIE("Did not receive completion signal.");
            break;
        case DEVICEERR_READPACKSIZEFAIL:
            terminate("Unable to read packet size.");
            break;
        case DEVICEERR_BADPACKSIZE:
            SHOULDIE("Wrong read packet size.");
            break;
        case DEVICEERR_MALLOCFAIL:
            terminate("Malloc failure.");
            return;
        case DEVICEERR_UPLOADCANCELLED:
            log_replace("Upload cancelled by the user.\n", CRDEF_ERROR);
            return;
        case DEVICEERR_TIMEOUT:
            SHOULDIE("Flashcart timed out.");
            break;
        case DEVICEERR_POLLFAIL:
            SHOULDIE("Flashcart polling failed.");
            break;
        case DEVICEERR_64D_8303USB:
            terminate("The 8303 CIC is not supported through USB.");
            break;
        case DEVICEERR_64D_BADCMP:
            SHOULDIE("Received bad CMP signal.");
            break;
        case DEVICEERR_64D_CANTDEBUG:
            terminate("Please upgrade to firmware 2.05 or higher to access USB debugging.");
            break;
        case DEVICEERR_64D_BADDMA:
            SHOULDIE("Unexpected DMA header.");
            break;
        case DEVICEERR_64D_DATATOOBIG:
            log_colored("Data must be under 8MB.\n", CRDEF_ERROR);
            return;
        case DEVICEERR_SC64_CMDFAIL:
            terminate("SC64 command response error");
            break;
        case DEVICEERR_SC64_COMMFAIL:
            terminate("SC64 communication error");
            break;
        case DEVICEERR_SC64_CTRLRELEASEFAIL:
            terminate("Couldn't release SC64 controller reset.");
            break;
        case DEVICEERR_SC64_CTRLRESETFAIL:
            terminate("Couldn't perform SC64 controller reset.");
            break;
        case DEVICEERR_SC64_FIRMWARECHECKFAIL:
            terminate("Couldn't get SC64 firmware version.");
            break;
        case DEVICEERR_SC64_FIRMWAREUNSUPPORTED:
            terminate("Unsupported SC64 firmware version, please upgrade to firmware 2.14.0 or higher.");
            break;
        default:
            if (err != DEVICEERR_OK && err != DEVICEERR_NOTCART)
                log_colored("Unhandled device error '%d'.\n", CRDEF_ERROR, err);
            return;
    }
}