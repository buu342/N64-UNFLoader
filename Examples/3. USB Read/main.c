/***************************************************************
                             main.c
                               
Handles the boot process of the ROM.
***************************************************************/

#include <ultra64.h>
#include <string.h>
#include "debug.h"
#include "osconfig.h"


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
  
    // Initialize the USB library
    usb_initialize();

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
    char newdata = FALSE;
    char echobuffer[1024]; // 1 kilobyte buffer for echoing data back
    char incoming_type = 0;
    int incoming_size = 0;
    
    // Run in a loop
    while (1)
    {
        #if DEBUG_MODE
            // Check if there's data in USB
            // Needs to be a while loop because we can't write to USB if there's data that needs to be read first
            while (usb_poll() != 0)
            {
                int header = usb_poll();
                newdata = TRUE;
                
                // Store the size and type from the header
                incoming_type = USBHEADER_GETTYPE(header);
                incoming_size = USBHEADER_GETSIZE(header);
                
                // If the amount of data is larger than our echo buffer
                if (incoming_size > 1024)
                {
                    // Purge the USB data
                    usb_purge();
                    
                    // Write an error message to buffer instead
                    sprintf(echobuffer, "Incoming data larger than echo buffer!\n");
                    incoming_type = DATATYPE_TEXT;
                    incoming_size = strlen(echobuffer)+1;
                    
                    // Restart the while loop to check if there's more USB data
                    continue;
                }
                
                // Read the data from the USB into our echo buffer
                usb_read(echobuffer, incoming_size);
            }
            
            // If there's no more data in the USB buffers and we have new data to echo
            if (newdata)
            {
                // Write the data to USB
                usb_write(incoming_type, echobuffer, incoming_size);
                newdata = FALSE;
            }
        #endif
    }
}