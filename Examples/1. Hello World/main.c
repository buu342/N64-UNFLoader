/***************************************************************
                             main.c
                               
Handles the boot process of the ROM.
***************************************************************/

#include <ultra64.h>
#include "debug.h"
#include "osconfig.h"


/*********************************
           Definitions
*********************************/

// Use printf instead of usb_write
#define USE_PRITNF 1


/*********************************
         Thread Pointers
*********************************/

OSThread idleThread;
OSThread mainThread;


/*********************************
        Parallel Interface
*********************************/

#define NUM_PI_MSGS 8
static OSMesg PiMessages[NUM_PI_MSGS];
static OSMesgQueue PiMessageQ;


/*********************************
        Function Prototypes
*********************************/

static void idleThreadFunction(void *arg);
static void mainThreadFunction(void *arg);


/*==============================
    boot
    Initializes the hardware and creates 
    the idle thread
==============================*/

void boot()
{
    // Initialize the hardware and software
    osInitialize();
    
    // Create the idle thread
    osCreateThread(&idleThread, THREADID_IDLE, idleThreadFunction, (void *)0, idleThreadStack+IDLESTACKSIZE/sizeof(u64), THREADPRI_IDLE);
    osStartThread(&idleThread);
}


/*==============================
    idleThreadFunction
    The code executed by the idle thread
    @param An argument to pass to the thread
==============================*/

static void idleThreadFunction(void *arg)
{
    // Start the PI Manager for cartridge access
    osCreatePiManager((OSPri)OS_PRIORITY_PIMGR, &PiMessageQ, PiMessages, NUM_PI_MSGS);
  
    // Initialize the debug library
    #if USE_PRITNF
        debug_initialize();
    #else
        usb_initialize();
    #endif

    // This will only print if USE_OSRAW is enabled in usb.h
    #if USE_OSRAW
        debug_printf("Printed without the PI manager!\n");
    #endif

    // Create the main thread
    osCreateThread(&mainThread, THREADID_MAIN, mainThreadFunction, (void *)0, mainThreadStack+MAINSTACKSIZE/sizeof(u64), THREADPRI_MAIN);
    osStartThread(&mainThread);
    
    // Spin forever
    while(1)
        ;
}


/*==============================
    mainThreadFunction
    The code executed by the main thread
    @param An argument to pass to the thread
==============================*/

static void mainThreadFunction(void *arg)
{
    // Say hello to the command prompt!
    #if USE_PRITNF
        debug_printf("Hello World!\n");
    #else
        usb_write(DATATYPE_TEXT, "Hello World!\n", 13+1); 
    #endif
    
    // Spin forever
    while(1)
        ;
}
