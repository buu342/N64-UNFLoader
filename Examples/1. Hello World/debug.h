#ifndef UNFL_DEBUG_H
#define UNFL_DEBUG_H

    /*********************************
             Settings macros
    *********************************/

    // Settings
    #define DEBUG_MODE        1   // Enable/Disable debug mode
    #define USE_FAULTTHREAD   1   // Create a fault detection thread 
    #define OVERWRITE_OSPRINT 1   // Replaces osSyncPrintf calls with debug_printf
    
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
            Does not pause the drawing thread!
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


        // Ignore this, use the macro instead
        extern void _debug_assert(const char* expression, const char* file, int line);
        
        // Include usb.h automatically
        #include "usb.h"
    #else
        #define debug_initialize() 
        #define debug_printf(__VA_ARGS__) 
        #define debug_screenshot(a, b, c)
        #define debug_assert(a)
        #define usb_write(a, b, c)
        #define usb_read() 0
    #endif
    
#endif