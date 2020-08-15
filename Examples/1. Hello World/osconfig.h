#ifndef UNFLEX_OSCONFIG_H
#define UNFLEX_OSCONFIG_H

    /**********************************
                  Threads
    **********************************/

    // Thread ID's
    #define THREADID_IDLE 1
    #define THREADID_MAIN 2
    
    // Thread Priorities
    #define THREADPRI_IDLE 0
    #define THREADPRI_MAIN 10
    
    // Thread pointers
    extern OSThread idleThread;
    extern OSThread mainThread;
    
    
    /**********************************
                  Stacks
    *********************************/
    
    // Stack sizes
    #define	BOOTSTACKSIZE   0x2000
    #define	IDLESTACKSIZE   0x2000
    #define	MAINSTACKSIZE   0x2000
    
    // Stack pointers
    #ifdef	NOT_SPEC  // For Linux Compatibility
        extern u64 bootStack[BOOTSTACKSIZE/sizeof(u64)];
        extern u64 idleThreadStack[IDLESTACKSIZE/sizeof(u64)];
        extern u64 mainThreadStack[MAINSTACKSIZE/sizeof(u64)];
    #endif
    
#endif