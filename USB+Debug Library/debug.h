#ifndef UNFL_DEBUG_H
#define UNFL_DEBUG_H

    /*********************************
             Settings macros
    *********************************/

    // Settings
    #define DEBUG_MODE        1   // Enable/Disable debug mode
    #define DEBUG_INIT_MSG    1   // Print a messgae when debug mode has initialized
    #define USE_FAULTTHREAD   1   // Create a fault detection thread 
    #define OVERWRITE_OSPRINT 1   // Replaces osSyncPrintf calls with debug_printf
    #define MAX_COMMANDS      25  // The max amount of user defined commands possible
    
    // Fault thread definitions
    #define FAULT_THREAD_ID    13
    #define FAULT_THREAD_PRI   251
    #define FAULT_THREAD_STACK 0x2000
    
    // USB thread definitions
    #define USB_THREAD_ID    14
    #define USB_THREAD_PRI   252
    #define USB_THREAD_STACK 0x2000
    
    
    /*********************************
             Debug Functions
    *********************************/
    
    #if DEBUG_MODE
        
        /*==============================
            debug_initialize
            Initializes the debug and USB library
        ==============================*/
        
        extern void debug_initialize();
        
        
        /*==============================
            debug_printf
            Prints a formatted message to the developer's command prompt.
            Supports up to 256 characters.
            @param A string to print
            @param variadic arguments to print as well
        ==============================*/
        
        extern void debug_printf(const char* message, ...);
        
        
        /*==============================
            debug_screenshot
            Sends the currently displayed framebuffer through USB.
            @param The size of each pixel of the framebuffer in bytes
                   Typically 4 if 32-bit or 2 if 16-bit
            @param The width of the framebuffer
            @param The height of the framebuffer
        ==============================*/
        
        extern void debug_screenshot(int size, int w, int h);
        
        
        /*==============================
            debug_assert
            Halts the program if the expression fails
            @param The expression to test
        ==============================*/
        
        #define debug_assert(expr) (expr) ? ((void)0) : _debug_assert(#expr, __FILE__, __LINE__)
        
        
        /*==============================
            debug_pollcommands
            Check the USB for incoming commands
        ==============================*/
        
        extern void debug_pollcommands();
        
        
        /*==============================
            debug_addcommand
            Adds a command for the USB to read
            @param The command name
            @param The command description
            @param The function pointer to execute
        ==============================*/
        
        extern void debug_addcommand(char* command, char* description, void(*execute)());

        
        /*==============================
            debug_parsecommand
            Gets the next part of the incoming command
            @return A pointer to the next part of the command, or NULL
        ==============================*/
        
        //extern char* debug_parsecommand();
                
        
        /*==============================
            debug_sizecommand
            Returns the size of the data from this part of the command
            @return The size of the data in bytes, or -1
        ==============================*/
        
        //extern int debug_sizecommand();
        
        
        /*==============================
            debug_trashcommand
            Trash the incoming data if it's not needed anymore
        ==============================*/
        
        //extern void debug_trashcommand();
        
        
        /*==============================
            debug_printcommands
            Prints a list of commands to the developer's command prompt.
        ==============================*/
        
        extern void debug_printcommands();

        
        // Ignore this, use the macro instead
        extern void _debug_assert(const char* expression, const char* file, int line);
        
        // Include usb.h automatically
        #include "usb.h"
        
    #else
        
        // Overwrite library functions with useless macros if debug mode is disabled
        #define debug_initialize() 
        #define debug_printf(__VA_ARGS__) 
        #define debug_screenshot(a, b, c)
        #define debug_assert(a)
        #define debug_pollcommands()
        #define debug_addcommand(a, b, c)
        #define debug_parsecommand() NULL
        #define debug_sizecommand() 0
        #define debug_trashcommand()
        #define debug_printcommands()
        #define usb_write(a, b, c)
        #define usb_poll() 0
        #define usb_read() 0
        
    #endif
    
#endif