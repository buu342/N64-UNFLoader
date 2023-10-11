/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "helper.h"
#include "term.h"
#include "device.h"
#include "debug.h"
#include "gdbstub.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <list>
#include <iterator>
#include <thread>
#include <chrono>


/*********************************
              Macros
*********************************/

#define PROGRAM_NAME_LONG  "Universal N64 Flashcart Loader"
#define PROGRAM_NAME_SHORT "UNFLoader"
#define PROGRAM_GITHUB     "https://github.com/buu342/N64-UNFLoader"
#define DEFAULT_GDBADDR    "127.0.0.1"
#define DEFAULT_GDBPORT    "8080"
#define DEFAULT_GDB        (DEFAULT_GDBADDR ":" DEFAULT_GDBPORT)


/*********************************
        Function Prototypes
*********************************/

static void parse_args_priority(std::list<char*>* args);
static void parse_args(std::list<char*>* args);
static void program_loop();
static void autodetect_romheader();
static void show_title();
static void show_args();
static void show_help();
#define nextarg_isvalid(a, b) (((++a) != b->end()) && (*a)[0] != '-')


/*********************************
             Globals
*********************************/

// Program globals
FILE* global_debugoutptr = NULL;
bool  global_badpackets = true;
char* global_gdbaddr = (char*)"";
std::atomic<bool> global_terminating (false);

// Local globals
static bool              local_autodetect = true;
static bool              local_debugmode  = false;
static bool              local_listenmode = false;
static int               local_timeout = -1;
static std::list<char*>  local_args;
static std::atomic<int>  local_esclevel (0);
static std::atomic<bool> local_reupload (false);


/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

int main(int argc, char* argv[])
{
    // Put the arguments in a list to make life easier
    for (int i=1; i<argc; i++)
        local_args.push_back(argv[i]);

    // Parse priority arguments
    parse_args_priority(&local_args);

    // Initialize the program
    device_initialize();
    term_initialize();
    term_allowinput(false);
    show_title();

    // Read program arguments
    parse_args(&local_args);

    // Show the program arguments if the program can't do much else
    if (!local_debugmode && !local_listenmode && device_getrom() == NULL)
    {
        show_args();
        if (term_isusingcurses())
            terminate(NULL);
        else
            pauseprogram();
    }

    // Can't use listen mode if there's no ROM to listen to
    if (local_listenmode && device_getrom() == NULL)
        terminate("Cannot use listen mode if no ROM is given.");

    // Initialize winsock so that we can connect to GDB on Windows
    #ifndef LINUX
        if (strlen(global_gdbaddr) > 0)
        {
            int err;
            WSADATA wsa;
            err = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (err != 0)
                terminate("WSAStartup failed with error: %d\n", err);
            if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
            {
                WSACleanup();
                terminate("Could not find a usable version of Winsock.dll\n");
            }
        }
    #endif

    // Do the program loop
    program_loop();

    // End the program
    terminate(NULL);
    return 0;
}


/*==============================
    parse_args_priority
    Parses priority arguments
    @param A list of arguments
==============================*/

static void parse_args_priority(std::list<char*>* args)
{
    std::list<char*>::iterator it;

    // Check if curses should be disabled
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);
        if (!strcmp(command, "-b"))
        {
            term_usecurses(false);
            args->erase(it);
            break;
        }
    }

    // Check for the forced terminal size
    if (term_isusingcurses())
    {
        for (it = args->begin(); it != args->end(); ++it)
        {
            char* command = (*it);
            if (!strcmp(command, "-w"))
            {
                if (nextarg_isvalid(it, args))
                {
                    int h = atoi((*it));
                    if (nextarg_isvalid(it, args))
                    {
                        int w = atoi((*it));
                        term_initsize(h, w);
                        it = args->erase(it);
                        --it;
                        it = args->erase(it);
                        --it;
                        args->erase(it);
                    }
                    else
                        terminate("Missing parameter(s) for command '%s'.", command);
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            }
        }
    }

    // Check if the help argument was requested
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);
        if (!strcmp(command, "-help"))
        {
            if (term_isusingcurses())
               term_initialize();
            show_title();
            show_help();
            if (term_isusingcurses())
                terminate(NULL);
            else
                pauseprogram();
        }
    }
}


/*==============================
    parse_args
    Parses the arguments passed to the program
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

static void parse_args(std::list<char*>* args)
{
    std::list<char*>::iterator it;

    // If the list is empty, nothing left to parse
    if (args->empty())
        return;

    // Handle the rest of the program arguments
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);

        // Only allow valid command formats
        if ((command[0] != '-' || command[1] == '\0') && device_getrom() != NULL)
            terminate("Unknown command '%s'", command);

        // Handle the rest of the commands
        switch(command[1])
        {
            case 'r': // ROM to upload
                if (nextarg_isvalid(it, args))
                {
                    if (!device_setrom(*it))
                        terminate("'%s' is not a file.");
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'f': // Set flashcart
                if (nextarg_isvalid(it, args))
                {
                    CartType cart = cart_strtotype(*it);
                    device_setcart(cart);
                    log_simple("Flashcart forced to '%s'\n", cart_typetostr(cart));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'c': // Set CIC
                if (nextarg_isvalid(it, args))
                {
                    CICType cic = cic_strtotype(*it);
                    device_setcic(cic);
                    log_simple("CIC forced to '%s'\n", cic_typetostr(cic));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 's': // Set save type
                if (nextarg_isvalid(it, args))
                {
                    SaveType save = save_strtotype(*it);
                    device_setsave(save);
                    log_simple("Save type set to '%s'\n", save_typetostr(save));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'd': // Set debug mode
                local_debugmode = true;
                if (nextarg_isvalid(it, args))
                {
                    debug_setdebugout(*it);
                    log_simple("Debug logging to file '%s'\n", *it);
                }
                else
                    --it;
                break;
            case 'l': // Set listen mode
                local_listenmode = true;
                break;
            case 'g': // GDB address
                global_gdbaddr = (char*)DEFAULT_GDB;
                if (nextarg_isvalid(it, args))
                {
                    char* addr = (char*)DEFAULT_GDBADDR;
                    char* port = (char*)DEFAULT_GDBPORT;

                    // If a colon exists assume a full address
                    if (strchr(*it, ':') != NULL)
                    {
                        char* token = strtok(*it, ":");

                        // Everything to the left of the colon is the address, everything to the right is the port
                        if (strlen(token) > 0)
                            addr = token;
                        token = strtok(NULL, ":");
                        if (strlen(token) > 0)
                            port = token;
                    }
                    else if (strchr(*it, '.') != NULL || !strcmp(*it, "localhost")) // If a dot exists (or the string is localhost), then assume an address
                        addr = *it;
                    else // Otherwise, assume just a port
                        port = *it;

                    // Generate the new address
                    global_gdbaddr = (char*) malloc((strlen(addr) + strlen(port) + strlen(":") + 1)*sizeof(char));
                    sprintf(global_gdbaddr, "%s:%s", addr, port);
                }
                else
                    --it;
                break;
            case 'a': // Disable ED ROM header autodetection
                local_autodetect = false;
                break;
            case 'e': // File export directory
                if (nextarg_isvalid(it, args))
                {
                    debug_setbinaryout(*it);
                    log_simple("File export path set to '%s'\n", *it);
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 't': // File export directory
                if (nextarg_isvalid(it, args))
                {
                    local_timeout = atoi(*it);
                    if (local_timeout < 0)
                        terminate("Timeout must be larger than zero.");
                    log_simple("Timeout set to %d seconds.\n", *it);
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'h': // Set history size
                if (nextarg_isvalid(it, args))
                    term_sethistorysize(atoi(*it));
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'm': // Allow message stacking
                term_allowinput(false);
                break;
            case 'p':
                global_badpackets = false;
                break;
            default:
                if (device_getrom() == NULL)
                {
                    if (!device_setrom(*it))
                        terminate("'%s' is not a file.", *it);
                }
                else
                    terminate("Unknown command '%s'", command);
                break;
        }
    }
}


/*==============================
    show_title
    Prints the title of the program
==============================*/

static void show_title()
{
    int i;
    char title[] = PROGRAM_NAME_SHORT;
    int titlesize = sizeof(title)/sizeof(title[0]);
 
    // Print the title
    for (i=0; i<titlesize-1; i++)
    {
        char str[2];
        str[0] = title[i];
        str[1] = '\0';
        log_colored(str, 1+(i)%(TOTAL_COLORS-1));
    }

    // Print info stuff
    log_simple("\n--------------------------------------------\n");
    log_simple("Cobbled together by Buu342\n");
    log_simple("Compiled on %s\n\n", __DATE__);
}


/*==============================
    program_loop
    The main UNFLoader program 
    flow
==============================*/

static void program_loop()
{
    bool firstupload = true;
    bool autocart = (device_getcart() == CART_NONE);
    time_t lastmodtime = 0;

    // Check if we have a flashcart
    if (autocart)
        log_simple("Attempting flashcart autodetection\n");
    handle_deviceerror(device_find());
    if (autocart)
        log_replace("%s autodetected\n", CRDEF_PROGRAM, cart_typetostr(device_getcart()));

    // Explicit CIC checking
    if (device_getrom() != NULL && device_explicitcic())
        log_simple("CIC set automatically to '%s'.\n", cic_typetostr(device_getcic()));

    // Autodetect ROM header
    if (local_autodetect)
        autodetect_romheader();

    // Open the flashcart
    handle_deviceerror(device_open());
    log_simple("USB connection opened.\n");

    // Create a GDB Thread
    if (strlen(global_gdbaddr) > 0 && !gdb_isconnected())
    {
        if (!local_debugmode)
        {
            local_debugmode = true;
            log_simple("Forcing debug mode for remote debugging to work\n");
        }
        log_simple("Starting GDB server on %s\n", global_gdbaddr);
        std::thread t;
        t = std::thread(gdb_thread, global_gdbaddr);
        t.detach();
    }

    // Check if debug mode is possible
    if (local_debugmode)
        handle_deviceerror(device_testdebug());

    // If listen or debug mode is enabled, increment escape level so that
    // The user must press esc to exit
    if (local_listenmode || local_debugmode)
        increment_escapelevel();
    // Loop if debug mode or listen mode is enabled, and esc hasn't been pressed
    do 
    {
        time_t newmodtime = 0;
        if (device_getrom() != NULL)
            newmodtime = file_lastmodtime(device_getrom());

        // Listen mode
        if (!firstupload && local_listenmode && lastmodtime != newmodtime)
        {
            log_simple("ROM change detected. Reuploading.\n");
            local_reupload = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // If we have a ROM, upload it
        if (device_getrom() != NULL && (firstupload || local_reupload))
        {
            FILE* fp = NULL;
            uint64_t uploadtime;
            uint32_t filesize = 0; // I could use stat, but it doesn't work in WinXP (more info below)
            local_reupload = false;

            // Try multiple times to open the file, because sometimes does not work the first time in Listen mode
            for (int i=0; i<5; i++) 
            {
                fp = fopen(device_getrom(), "rb");
                if (fp != NULL)
                    break;
                else
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (fp == NULL)
                terminate("Unable to open file '%s'", device_getrom());
            
            // Get the filesize and reset the seek position
            // Workaround for https://stackoverflow.com/questions/32452777/visual-c-2015-express-stat-not-working-on-windows-xp
            fseek(fp, 0, SEEK_END);
            filesize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // File size checks
            if (filesize < 1*1024*1024)
                log_simple("ROM is smaller than 1MB, it might not boot properly.\n");
            if (filesize > device_getmaxromsize())
                terminate("The %s only supports ROMs up to %d bytes.", cart_typetostr(device_getcart()), device_getmaxromsize());
            if (device_rompadding(filesize) != filesize)
                log_simple("ROM will be padded by %d bytes to %dMB\n", device_rompadding(filesize) - filesize, device_rompadding(filesize)/(1024*1024));

            // Upload the ROM
            increment_escapelevel();
            uploadtime = time_miliseconds();
            if (term_isusingcurses()) // If curses is being used, spawn a thread to draw a progress bar
            {
                std::thread t;
                log_colored("Uploading ROM (ESC to cancel)\n", CRDEF_INPUT);
                t = std::thread(progressthread, "Uploading ROM (ESC to cancel)");
                handle_deviceerror(device_sendrom(fp, filesize));
                t.join();
            }
            else
            {
                log_simple("Uploading ROM (Type 'cancel' to stop).\n");
                handle_deviceerror(device_sendrom(fp, filesize));
            }

            // Success?
            if (!device_uploadcancelled())
            {
                decrement_escapelevel();
                log_replace("ROM successfully uploaded in %.02lf seconds!\n", CRDEF_PROGRAM, ((double)(time_miliseconds() - uploadtime)) / 1000.0f);
            }
            else
                log_replace("ROM upload cancelled by the user.\n", CRDEF_ERROR);
            
            // Update variables and close the file
            lastmodtime = newmodtime;
            fclose(fp);
        }

        // If this was the first run through the loop, initialize some stuff
        if (firstupload == true)
        {
            bool printed = false;
            if (local_debugmode)
            {
                log_colored("Debug mode started. ", CRDEF_INPUT);
                printed = true;
                term_allowinput(true);
            }
            if (local_listenmode)
            {
                log_colored("Listening for file changes.", CRDEF_INPUT);
                printed = true;
            }
            if (printed)
            {
                log_simple("\n");
                if (term_isusingcurses())
                {
                    if (device_getrom() != NULL)
                        log_colored("Press CTRL+R to force a reupload. ", CRDEF_INPUT);
                    log_colored("Press ESC to exit.\n\n", CRDEF_INPUT);
                }
                else
                {
                    if (device_getrom() != NULL)
                        log_simple("Type 'reupload' to force a reupload. ");
                    log_simple("Type 'exit' to exit.\n-----------------------------\n\n");
                }
            }
            firstupload = false;
        }

        // Handle debug mode
        debug_main();

        // Sleep to be kind to the CPU
        if (local_debugmode)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        else if (local_listenmode)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    while ((local_debugmode || local_listenmode) && get_escapelevel() > 0);
    term_allowinput(false);
    if (gdb_isconnected())
        gdb_disconnect();

    // Close the flashcart
    handle_deviceerror(device_close());
    log_simple("\nUSB connection closed.\n");
}


/*==============================
    program_event
    Sends important events to the
    program loop
    @param The program event
==============================*/

void program_event(ProgEvent key)
{
    switch (key)
    {
        case PEV_ESCAPE:
            decrement_escapelevel();
            break;
        case PEV_REUPLOAD:
            local_reupload = true;
            break;
    }
}


/*==============================
    get_escapelevel
    Gets the amount of times ESC
    needs to be pressed to exit.
    @return The ESC level
==============================*/

int get_escapelevel()
{
    return local_esclevel.load();
}


/*==============================
    increment_escapelevel
    Increments the amount of times
    ESC needs to be pressed to exit.
==============================*/

void increment_escapelevel()
{
    local_esclevel++;
}


/*==============================
    decrement_escapelevel
    Decrements the amount of times
    ESC needs to be pressed to exit.
==============================*/

void decrement_escapelevel()
{
    local_esclevel--;
}


/*==============================
    get_timeout
    Gets the current timeout value.
    @return -1 for no timeout, any other
            positive value for number of
            seconds for a timeout.
==============================*/

int get_timeout()
{
    return local_timeout;
}


/*==============================
    autodetect_romheader
    Reads the ROM header and sets the save/RTC value
    using the ED format.
==============================*/

static void autodetect_romheader()
{
    FILE* fp;
    byte* buff;
    if (!local_autodetect || device_getsave() != SAVE_NONE || device_getrom() == NULL)
        return;

    // Open the file
    buff = (byte*) malloc(0x40);
    fp = fopen(device_getrom(), "rb");
    if (buff == NULL || fp == NULL)
        terminate("Unable to open file '%s'.\n", device_getrom());

    // Check for a valid header
    if (fread(buff, 1, 0x40, fp) != 0x40)
        return;
    if (buff[0x3C] != 'E' || buff[0x3D] != 'D')
        return;
    fseek(fp, 0, SEEK_SET);

    // If the savetype hasn't been forced
    switch (buff[0x3F])
    {
        case 0x10: device_setsave(SAVE_EEPROM4K); break;
        case 0x20: device_setsave(SAVE_EEPROM16K); break;
        case 0x30: device_setsave(SAVE_SRAM256); break;
        case 0x50: device_setsave(SAVE_FLASHRAM); break;
        case 0x40: device_setsave(SAVE_SRAM768); break;
        case 0x60: device_setsave(SAVE_FLASHRAMPKMN); break;
    }
    if (device_getsave() != SAVE_NONE)
        log_simple("Auto set save type to '%s' from ED header.\n", save_typetostr(device_getsave()));

    // Cleanup
    free(buff);
}


/*==============================
    show_args
    Prints the arguments of the program
==============================*/

static void show_args()
{
    if (term_isusingcurses())
    {
        #ifndef LINUX
            int w = term_getw() < 80 ? 80 : term_getw();
            int h = term_geth() < 40 ? 40 : term_geth();
            term_setsize(h, w);
        #endif
    }

    // Print the program arguments
    log_simple("Parameters: <required> [optional]\n");
    log_simple("  -help\t\t\t   Learn how to use this tool.\n");
    log_simple("  -r <file>\t\t   Upload ROM.\n");
    log_simple("  -a\t\t\t   Disable ED ROM header autodetection.\n");
    log_simple("  -f <int>\t\t   Force flashcart type (skips autodetection).\n");
    log_simple("  \t %d - %s\n", (int)CART_64DRIVE1, "64Drive HW1");
    log_simple("  \t %d - %s\n", (int)CART_64DRIVE2, "64Drive HW2");
    log_simple("  \t %d - %s\n", (int)CART_EVERDRIVE, "EverDrive 3.0 or X7");
    log_simple("  \t %d - %s\n", (int)CART_SC64, "SC64");
    log_simple("  -c <int>\t\t   Set CIC emulation (64Drive HW2 only).\n");
    log_simple("  \t %d - %s\t %d - %s\n", (int)CIC_6101, "6101 (NTSC)", (int)CIC_6102, "6102 (NTSC)");
    log_simple("  \t %d - %s\t %d - %s\n", (int)CIC_7101, "7101 (NTSC)", (int)CIC_7102, "7102 (PAL)");
    log_simple("  \t %d - %s\t\t %d - %s\n", (int)CIC_X103, "x103 (All)", (int)CIC_X105, "x105 (All)");
    log_simple("  \t %d - %s\t\t %d - %s\n", (int)CIC_X106, "x106 (All)", (int)CIC_5101, "5101 (NTSC)");
    log_simple("  -s <int>\t\t   Set save emulation.\n");
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_EEPROM4K, "EEPROM 4Kbit", (int)SAVE_EEPROM16K, "EEPROM 16Kbit");
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_SRAM256, "SRAM 256Kbit", (int)SAVE_FLASHRAM, "FlashRAM 1Mbit");
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_SRAM768, "SRAM 768Kbit", (int)SAVE_FLASHRAMPKMN, "FlashRAM 1Mbit (PokeStdm2)");
    log_simple("  -d [filename]\t\t   Debug mode. Optionally write output to a file.\n");
    log_simple("  -l\t\t\t   Listen mode (reupload ROM when changed).\n");
    log_simple("  -g [addr][:][port]\t   Open a socket to GDB (default: %s:%s).\n", DEFAULT_GDBADDR, DEFAULT_GDBPORT);
    log_simple("  -t <seconds>\t\t   Set timeout for program exit.\n");
    log_simple("  -e <directory>\t   File export directory (Folder must exist!).\n");
    log_simple(            "\t\t\t   Example:  'folder/path/' or 'c:/folder/path'.\n");
    log_simple("  -w <int> <int>\t   Force terminal size (number rows + columns).\n");
    log_simple("  -h <int>\t\t   Max window history (default %d).\n", DEFAULT_HISTORYSIZE);
    log_simple("  -m\t\t\t   Always show duplicate prints in debug mode.\n");
    log_simple("  -p\t\t\t   Do not terminate on bad USB packets.\n");
    log_simple("  -b\t\t\t   Disable ncurses.\n");
}


/*==============================
    show_help
    Prints the help information
==============================*/

static void show_help()
{
    int category = 0;

    // Make the screen nicer
    if (term_isusingcurses())
    {
        #ifndef LINUX
            int w = term_getw() < 80 ? 80 : term_getw();
            int h = term_geth() < 40 ? 40 : term_geth();
            term_setsize(h, w);
        #endif
    }

    // Print the introductory message
    log_simple("Welcome to the %s!\n", PROGRAM_NAME_LONG);
    log_simple("This tool is designed to upload ROMs to your N64 Flashcart via USB, to allow\n"
               "homebrew developers to debug their ROMs in realtime with the help of the\n"
               "library provided with this tool.\n\n");
    log_simple("Which category are you interested in?\n"
               " 1 - Uploading ROMs on the 64Drive\n"
               " 2 - Uploading ROMs on the EverDrive\n"
               " 3 - Uploading ROMs on the SC64\n"
               " 4 - Using Listen mode\n"
               " 5 - Using Debug mode\n"
               " 6 - Using GDB\n");

    // Get the category
    log_colored("\nCategory: ", CRDEF_INPUT);
    category = getchar();
    if (term_isusingcurses())
        log_colored("%c\n\n", CRDEF_INPUT, category);

    // Print the category contents
    switch (category)
    {
        case '1':
            log_simple(" 1) Ensure your device is on the latest firmware/version.\n"
                       " 2) Plug your 64Drive USB into your PC, ensuring the console is turned OFF.\n"
                       " 3) Run this program to upload a ROM. Example:\n" 
                       " \t UNFLoader.exe -r myrom.n64\n");
            log_simple(" 4) If using 64Drive HW2, your game might not boot if you do not state the\n"
                       "    correct CIC as an argument. UNFLoader will try to autodetect the CIC from\n"
                       "    the ROM header. If this fails, you can specify the CIC as a program\n"
                       "    argument. Example:\n"
                       " \t UNFLoader.exe -r myrom.n64 -c 6102\n"
                       " 5) Once the upload process is finished, turn the console on. Your ROM should\n"
                       "    execute.\n");
            break;
        case '2':
            log_simple(" 1) Ensure your device is on the latest firmware/version for your cart model.\n"
                       " 2) Plug your EverDrive USB into your PC, ensuring the console is turned ON and\n"
                       "    in the main menu.\n"
                       " 3) Run this program to upload a ROM. Example:\n" 
                       " \t UNFLoader.exe -r myrom.n64\n"
                       " 4) Once the upload process is finished, your ROM should execute.\n");
            break;
        case '3':
            log_simple(" 1) Plug the SC64 USB into your PC.\n"
                       " 2) Run this program to upload a ROM. Example:\n" 
                       " \t UNFLoader.exe -r myrom.n64\n"
                       " 3) Once the upload process is finished, your ROM should execute.\n");
            break;
        case '4':
            log_simple("Listen mode automatically re-uploads the ROM via USB when it is modified. This\n"
                       "saves you the trouble of having to restart this program every recompile of your\n"
                       "homebrew. It is on YOU to ensure the cart is prepared to receive another ROM.\n"
                       "That means that the console must be switched OFF if you're using the 64Drive or\n"
                       "be in the menu if you're using an EverDrive. In the SC64's case, the ROM can be\n"
                       "uploaded while console is running, but if currently running code is actively\n"
                       "accessing ROM space, this can result in glitches or even crash, proceed with\n"
                       "caution.\n");
            break;
        case '5':
            log_simple("In order to use debug mode, the N64 ROM that you are executing must already\n"
                       "have implented the USB or debug library that comes with this tool. Otherwise,\n"
                       "debug mode will serve no purpose.\n\n");
            log_simple("During debug mode, you are able to type commands, which show up in ");
            log_colored(                                                                   "green", CRDEF_INPUT);
            log_simple(                                                                          " on\n"
                       "the bottom of the terminal. You can press ENTER to send this command to the N64\n"
                       "as the ROM executes. The command you send must obviously be implemented by the\n"
                       "debug library, and can do various things, such as upload binary files, take\n"
                       "screenshots, or change things in the game. If you wrap a part of your command\n"
                       "with the '@' symbol, the tool will treat that part as a file and will upload it\n"
                       "along with the rest of the data.\n\n");
            log_simple("During execution, the ROM is free to print things to the console where this\n"
                       "program is running. Messages from the console will appear in ");
            log_colored(                                                             "yellow", CRDEF_PRINT);
            log_simple(                                                                     ".\n\n"
                       "For more information on how to implement the debug library, check the GitHub\n"
                       "page where this tool was uploaded to, there should be plenty of examples there.\n"
                       PROGRAM_GITHUB"\n");
            break;
        case '6':
            log_simple("1) If you haven't already, you must first install gdb-multiarch.\n");
            log_simple("2) In a separate terminal, start GDB by calling:\n");
            log_colored("   gdb-multiarch PATH/TO/ROMNAME.out\n", CRDEF_PRINT);
            log_simple("   This should boot GDB with the ELF file of your ROM.\n"
                       "3) Now start UNFLoader with the -g argument. You can optionally provide an\n"
                       "   address and port pair separated by a colon, or just the port number. By\n"
                       "   default, the -g command will use " DEFAULT_GDBADDR " and port " DEFAULT_GDBPORT ".\n"
                       "4) Once UNFLoader is in debug mode, switch back to the terminal with gdb and\n"
                       "   call:");
            log_colored(" target remote ADDRESS:PORT\n", CRDEF_PRINT);
            log_simple("   where \"ADDRESS:PORT\" obviously match your -g argument.\n");
            log_simple("5) Now, any commands you type in GDB will be piped through UNFLoader and sent\n"
                       "   to the N64 through USB. This requires your ROM to have the UNFLoader debug\n"
                       "   library included, with the USE_RDBTHREAD flag enabled, for GDB to work.\n");
            break;
        default:
            terminate("Unknown category."); 
    }
    
    // Clear leftover input
    if (!term_isusingcurses())
        while ((category = getchar()) != '\n' && category != EOF);
}