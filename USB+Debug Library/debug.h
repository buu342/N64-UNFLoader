#ifndef P64_DEBUG_H
#define P64_DEBUG_H

    /*********************************
             Settings macros
    *********************************/

    // Settings
    #define DEBUG_MODE        1   // Enable/Disable debug mode
    #define USE_OSRAW         0   // Use if you're doing USB operations without the PI Manager
    #define USE_FAULTTHREAD   1   // Create a fault detection thread 
    #define OVERWRITE_OSPRINT 1   // Replaces osSyncPrintf calls with debug_printf
    
    // Fault thread definitions
    #define FAULT_THREAD_ID    13
    #define FAULT_THREAD_PRI   251
    #define FAULT_THREAD_STACK 0x2000
    
    
    /*********************************
             Debug Functions
    *********************************/
    
    // The accessible debug functions
    extern void debug_printf(const char* message, ...);
    extern void debug_poll();
    #define debug_assert(expr) (expr) ? ((void)0) : _debug_assert(#expr, __FILE__, __LINE__)
    
    // Ignore these, use the macros instead
    extern void _debug_assert(const char* expression, const char* file, int line);
    
#endif
