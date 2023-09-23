/***************************************************************
                            debug.c
                               
A basic debug library that makes use of the USB library for N64
flashcarts. 
https://github.com/buu342/N64-UNFLoader
***************************************************************/

#include "debug.h"
#ifndef LIBDRAGON
    #include <ultra64.h> 
    #include <PR/os_internal.h> // Needed for Crash's Linux toolchain
#else
    #include <libdragon.h>
    #include <stdio.h>
    #include <math.h>
#endif
#include <stdarg.h>
#include <string.h>

#if DEBUG_MODE
    
    /*********************************
               Definitions
    *********************************/
    
    // USB thread messages
    #define MSG_FAULT  0x10
    #define MSG_READ   0x11
    #define MSG_WRITE  0x12
    
    #define USBERROR_NONE     0
    #define USBERROR_NOTTEXT  1
    #define USBERROR_UNKNOWN  2
    #define USBERROR_TOOMUCH  3
    #define USBERROR_CUSTOM   4
    
    // RDB thread messages
    #define MSG_RDB_PACKET  0x10
    #define MSG_RDB_BPHIT   0x11
    #define MSG_RDB_PAUSE   0x12
    
    // Breakpoints
    #define BPOINT_COUNT   10
    #define MAKE_BREAKPOINT_INDEX(indx) (0x0000000D | ((indx) << 6))
    #define GET_BREAKPOINT_INDEX(addr)  ((((addr) >> 6) & 0x0000FFFF))
    
    // Helpful stuff
    #define HASHTABLE_SIZE  7
    #define COMMAND_TOKENS  10
    #define BUFFER_SIZE     256
    
    
    /*********************************
      Libultra types (for libdragon)
    *********************************/
    
    #ifdef LIBDRAGON
        typedef unsigned char      u8;
        typedef unsigned short     u16;
        typedef unsigned long      u32;
        typedef unsigned long long u64;
        
        typedef signed char s8;
        typedef short       s16;
        typedef long        s32;
        typedef long long   s64;
        
        typedef volatile unsigned char      vu8;
        typedef volatile unsigned short     vu16;
        typedef volatile unsigned long      vu32;
        typedef volatile unsigned long long vu64;
        
        typedef volatile signed char vs8;
        typedef volatile short       vs16;
        typedef volatile long        vs32;
        typedef volatile long long   vs64;
        
        typedef float  f32;
        typedef double f64;
        
        typedef void* OSMesg;
    #endif
    
    
    /*********************************
                 Structs
    *********************************/
    
    // Register struct
    typedef struct 
    {
        u32 mask;
        u32 value;
        char *string;
    } regDesc;
    
    // Because of the thread context's messy struct, this'll come in handy
    typedef struct {
        int size;
        void* ptr;
    } regType;
    
    // Thread message struct
    typedef struct 
    {
        int msgtype;
        int datatype;
        void* buff;
        int size;
    } usbMesg;
        
    // Debug command struct
    typedef struct 
    {
        char* command;
        char* description;
        char* (*execute)();
        void* next;
    } debugCommand;
    
    // Breakpoint struct
    typedef struct
    {
        u32* addr;
        u32  instruction;
        u32* next_addr;
    } bPoint;
    
    // Remote debugger packet lookup table
    typedef struct
    {
        char* command;
        void  (*func)();
    } RDBPacketLUT;
    
    
    /*********************************
            Function Prototypes
    *********************************/
    
    // Threads
    static void debug_thread_usb(void *arg);
    #ifndef LIBDRAGON
        #if USE_FAULTTHREAD
            static void debug_thread_fault(void *arg);
        #endif
        #if USE_RDBTHREAD
            static void debug_thread_rdb(void *arg);
            static void debug_rdb_qsupported(OSThread* t);
            static void debug_rdb_halt(OSThread* t);
            static void debug_rdb_dumpregisters(OSThread* t);
            static void debug_rdb_writeregisters(OSThread* t);
            static void debug_rdb_readmemory(OSThread* t);
            static void debug_rdb_writememory(OSThread* t);
            static void debug_rdb_togglebpoint(OSThread* t);
            static void debug_rdb_addbreakpoint(OSThread* t);
            static void debug_rdb_removebreakpoint(OSThread* t);
            static void debug_rdb_continue(OSThread* t);
        #endif
    
        // Other
        #if OVERWRITE_OSPRINT
            static void* debug_osSyncPrintf_implementation(void *unused, const char *str, size_t len);
        #endif
    #endif
    static inline void debug_handle_64drivebutton();
    
    
    /*********************************
                 Globals
    *********************************/
    
    // Function pointers
    #ifndef LIBDRAGON
        extern int _Printf(void *(*copyfunc)(void *, const char *, size_t), void*, const char*, va_list);
        #if OVERWRITE_OSPRINT
            extern void* __printfunc;
        #endif
    #endif
    
    // Debug globals
    static char  debug_initialized = 0;
    static char  debug_buffer[BUFFER_SIZE];
    
    // Commands hashtable related
    static debugCommand* debug_commands_hashtable[HASHTABLE_SIZE];
    static debugCommand  debug_commands_elements[MAX_COMMANDS];
    static int           debug_commands_count = 0;
    
    // Command parsing related
    static int   debug_command_current = 0;
    static int   debug_command_totaltokens = 0;
    static int   debug_command_incoming_start[COMMAND_TOKENS];
    static int   debug_command_incoming_size[COMMAND_TOKENS];
    static char* debug_command_error = NULL;
    
    // Assertion globals
    static int         assert_line = 0;
    static const char* assert_file = NULL;
    static const char* assert_expr = NULL;
    
    // 64Drive button functions
    static void  (*debug_64dbut_func)() = NULL;
    static u64   debug_64dbut_debounce = 0;
    static u64   debug_64dbut_hold = 0;
    
    #ifndef LIBDRAGON
        
        // USB thread globals
        static OSMesgQueue usbMessageQ;
        static OSMesg      usbMessageBuf;
        static OSThread    usbThread;
        static u64         usbThreadStack[USB_THREAD_STACK/sizeof(u64)];
        
        // Fault thread globals
        #if USE_FAULTTHREAD
            static OSMesgQueue faultMessageQ;
            static OSMesg      faultMessageBuf;
            static OSThread    faultThread;
            static u64         faultThreadStack[FAULT_THREAD_STACK/sizeof(u64)];
        
            // List of error causes
            static regDesc causeDesc[] = {
                {CAUSE_BD,      CAUSE_BD,    "BD"},
                {CAUSE_IP8,     CAUSE_IP8,   "IP8"},
                {CAUSE_IP7,     CAUSE_IP7,   "IP7"},
                {CAUSE_IP6,     CAUSE_IP6,   "IP6"},
                {CAUSE_IP5,     CAUSE_IP5,   "IP5"},
                {CAUSE_IP4,     CAUSE_IP4,   "IP4"},
                {CAUSE_IP3,     CAUSE_IP3,   "IP3"},
                {CAUSE_SW2,     CAUSE_SW2,   "IP2"},
                {CAUSE_SW1,     CAUSE_SW1,   "IP1"},
                {CAUSE_EXCMASK, EXC_INT,     "Interrupt"},
                {CAUSE_EXCMASK, EXC_MOD,     "TLB modification exception"},
                {CAUSE_EXCMASK, EXC_RMISS,   "TLB exception on load or instruction fetch"},
                {CAUSE_EXCMASK, EXC_WMISS,   "TLB exception on store"},
                {CAUSE_EXCMASK, EXC_RADE,    "Address error on load or instruction fetch"},
                {CAUSE_EXCMASK, EXC_WADE,    "Address error on store"},
                {CAUSE_EXCMASK, EXC_IBE,     "Bus error exception on instruction fetch"},
                {CAUSE_EXCMASK, EXC_DBE,     "Bus error exception on data reference"},
                {CAUSE_EXCMASK, EXC_SYSCALL, "System call exception"},
                {CAUSE_EXCMASK, EXC_BREAK,   "Breakpoint exception"},
                {CAUSE_EXCMASK, EXC_II,      "Reserved instruction exception"},
                {CAUSE_EXCMASK, EXC_CPU,     "Coprocessor unusable exception"},
                {CAUSE_EXCMASK, EXC_OV,      "Arithmetic overflow exception"},
                {CAUSE_EXCMASK, EXC_TRAP,    "Trap exception"},
                {CAUSE_EXCMASK, EXC_VCEI,    "Virtual coherency exception on intruction fetch"},
                {CAUSE_EXCMASK, EXC_FPE,     "Floating point exception (see fpcsr)"},
                {CAUSE_EXCMASK, EXC_WATCH,   "Watchpoint exception"},
                {CAUSE_EXCMASK, EXC_VCED,    "Virtual coherency exception on data reference"},
                {0,             0,           ""}
            };
            
            // List of register descriptions
            static regDesc srDesc[] = {
                {SR_CU3,      SR_CU3,     "CU3"},
                {SR_CU2,      SR_CU2,     "CU2"},
                {SR_CU1,      SR_CU1,     "CU1"},
                {SR_CU0,      SR_CU0,     "CU0"},
                {SR_RP,       SR_RP,      "RP"},
                {SR_FR,       SR_FR,      "FR"},
                {SR_RE,       SR_RE,      "RE"},
                {SR_BEV,      SR_BEV,     "BEV"},
                {SR_TS,       SR_TS,      "TS"},
                {SR_SR,       SR_SR,      "SR"},
                {SR_CH,       SR_CH,      "CH"},
                {SR_CE,       SR_CE,      "CE"},
                {SR_DE,       SR_DE,      "DE"},
                {SR_IBIT8,    SR_IBIT8,   "IM8"},
                {SR_IBIT7,    SR_IBIT7,   "IM7"},
                {SR_IBIT6,    SR_IBIT6,   "IM6"},
                {SR_IBIT5,    SR_IBIT5,   "IM5"},
                {SR_IBIT4,    SR_IBIT4,   "IM4"},
                {SR_IBIT3,    SR_IBIT3,   "IM3"},
                {SR_IBIT2,    SR_IBIT2,   "IM2"},
                {SR_IBIT1,    SR_IBIT1,   "IM1"},
                {SR_KX,       SR_KX,      "KX"},
                {SR_SX,       SR_SX,      "SX"},
                {SR_UX,       SR_UX,      "UX"},
                {SR_KSU_MASK, SR_KSU_USR, "USR"},
                {SR_KSU_MASK, SR_KSU_SUP, "SUP"},
                {SR_KSU_MASK, SR_KSU_KER, "KER"},
                {SR_ERL,      SR_ERL,     "ERL"},
                {SR_EXL,      SR_EXL,     "EXL"},
                {SR_IE,       SR_IE,      "IE"},
                {0,           0,          ""}
            };
            
            // List of floating point registers descriptions
            static regDesc fpcsrDesc[] = {
                {FPCSR_FS,      FPCSR_FS,    "FS"},
                {FPCSR_C,       FPCSR_C,     "C"},
                {FPCSR_CE,      FPCSR_CE,    "Unimplemented operation"},
                {FPCSR_CV,      FPCSR_CV,    "Invalid operation"},
                {FPCSR_CZ,      FPCSR_CZ,    "Division by zero"},
                {FPCSR_CO,      FPCSR_CO,    "Overflow"},
                {FPCSR_CU,      FPCSR_CU,    "Underflow"},
                {FPCSR_CI,      FPCSR_CI,    "Inexact operation"},
                {FPCSR_EV,      FPCSR_EV,    "EV"},
                {FPCSR_EZ,      FPCSR_EZ,    "EZ"},
                {FPCSR_EO,      FPCSR_EO,    "EO"},
                {FPCSR_EU,      FPCSR_EU,    "EU"},
                {FPCSR_EI,      FPCSR_EI,    "EI"},
                {FPCSR_FV,      FPCSR_FV,    "FV"},
                {FPCSR_FZ,      FPCSR_FZ,    "FZ"},
                {FPCSR_FO,      FPCSR_FO,    "FO"},
                {FPCSR_FU,      FPCSR_FU,    "FU"},
                {FPCSR_FI,      FPCSR_FI,    "FI"},
                {FPCSR_RM_MASK, FPCSR_RM_RN, "RN"},
                {FPCSR_RM_MASK, FPCSR_RM_RZ, "RZ"},
                {FPCSR_RM_MASK, FPCSR_RM_RP, "RP"},
                {FPCSR_RM_MASK, FPCSR_RM_RM, "RM"},
                {0,             0,           ""}
            };
        #endif
        
        
        // Remote debugger thread globals
        #if USE_RDBTHREAD
            static OSMesgQueue rdbMessageQ;
            static OSMesg      rdbMessageBuf;
            static OSThread    rdbThread;
            static u64         rdbThreadStack[RDB_THREAD_STACK/sizeof(u64)];
            
            // RDB status globals
            static u8        debug_rdbpaused = FALSE;
            static bPoint    debug_bpoints[BPOINT_COUNT];

            // Remote debugger packet lookup table
            RDBPacketLUT lut_rdbpackets[] = {
                // Due to the use of strncmp, the order of strings matters!
                {"qSupported", debug_rdb_qsupported},
                {"?", debug_rdb_halt},
                {"g", debug_rdb_dumpregisters},
                {"G", debug_rdb_writeregisters},
                {"m", debug_rdb_readmemory},
                {"M", debug_rdb_writememory},
                {"Z0", debug_rdb_addbreakpoint},
                {"z0", debug_rdb_removebreakpoint},
                //{"s", debug_rdb_step},
                {"c", debug_rdb_continue},
            };
        #endif
    #endif
    
    
    /*********************************
             Debug functions
    *********************************/
    
    /*==============================
        debug_initialize
        Initializes the debug library
    ==============================*/
    
    void debug_initialize()
    {        
        // Initialize the USB functions
        if (!usb_initialize())
            return;
        
        // Initialize globals
        memset(debug_commands_hashtable, 0, sizeof(debugCommand*)*HASHTABLE_SIZE);
        memset(debug_commands_elements, 0, sizeof(debugCommand)*MAX_COMMANDS);
        
        // Libultra functions
        #ifndef LIBDRAGON
            // Overwrite osSyncPrintf
            #if OVERWRITE_OSPRINT
                __printfunc = (void*)debug_osSyncPrintf_implementation;
            #endif
            
            // Initialize the USB thread
            osCreateThread(&usbThread, USB_THREAD_ID, debug_thread_usb, 0, 
                            (usbThreadStack+USB_THREAD_STACK/sizeof(u64)), 
                            USB_THREAD_PRI);
            osStartThread(&usbThread);
            
            // Initialize the fault thread
            #if USE_FAULTTHREAD
                osCreateThread(&faultThread, FAULT_THREAD_ID, debug_thread_fault, 0, 
                                (faultThreadStack+FAULT_THREAD_STACK/sizeof(u64)), 
                                FAULT_THREAD_PRI);
                osStartThread(&faultThread);
            #endif
            
            // Initialize the remote debugger thread
            #if USE_RDBTHREAD
                osCreateThread(&rdbThread, RDB_THREAD_ID, debug_thread_rdb, (void*)osGetThreadId(NULL), 
                                (rdbThreadStack+RDB_THREAD_STACK/sizeof(u64)), 
                                RDB_THREAD_PRI);
                osStartThread(&rdbThread);
                
                // Pause the main thread
                usb_purge();
                usb_write(DATATYPE_TEXT, "Pausing main thread until GDB connects and resumes\n", 51+1);
                osSendMesg(&rdbMessageQ, (OSMesg)MSG_RDB_PAUSE, OS_MESG_BLOCK);
            #endif
        #endif
        
        // Mark the debug mode as initialized
        debug_initialized = 1;
        #if DEBUG_INIT_MSG
            debug_printf("Debug mode initialized!\n\n");
        #endif
    }
    
    
    #ifndef LIBDRAGON
        /*==============================
            printf_handler
            Handles printf memory copying
            @param The buffer to copy the partial string to
            @param The string to copy
            @param The length of the string
            @returns The end of the buffer that was written to
        ==============================*/
        
        static void* printf_handler(void *buf, const char *str, size_t len)
        {
            return ((char *) memcpy(buf, str, len) + len);
        }
    #endif
     
     
    /*==============================
        debug_printf
        Prints a formatted message to the developer's command prompt.
        Supports up to 256 characters.
        @param A string to print
        @param variadic arguments to print as well
    ==============================*/
    
    void debug_printf(const char* message, ...)
    {
        int len = 0;
        usbMesg msg;
        va_list args;
        
        // Use the internal libultra printf function to format the string
        va_start(args, message);
        #ifndef LIBDRAGON
            len = _Printf(&printf_handler, debug_buffer, message, args);
        #else
            len = vsprintf(debug_buffer, message, args);
        #endif
        va_end(args);
        
        // Attach the '\0' if necessary
        if (0 <= len)
            debug_buffer[len] = '\0';
        
        // Send the printf to the usb thread
        msg.msgtype = MSG_WRITE;
        msg.datatype = DATATYPE_TEXT;
        msg.buff = debug_buffer;
        msg.size = len+1;
        #ifndef LIBDRAGON
            osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
        #else
            debug_thread_usb(&msg);
        #endif
    }
    
    
    /*==============================
        debug_dumpbinary
        Dumps a binary file through USB
        @param The file to dump
        @param The size of the file
    ==============================*/
    
    void debug_dumpbinary(void* file, int size)
    {
        usbMesg msg;
        
        // Send the binary file to the usb thread
        msg.msgtype = MSG_WRITE;
        msg.datatype = DATATYPE_RAWBINARY;
        msg.buff = file;
        msg.size = size;
        #ifndef LIBDRAGON
            osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
        #else
            debug_thread_usb(&msg);
        #endif
    }
    
    
    /*==============================
        debug_screenshot
        Sends the currently displayed framebuffer through USB.
        DOES NOT PAUSE DRAWING THREAD! Using outside the drawing
        thread may lead to a screenshot with visible tearing
    ==============================*/
    
    void debug_screenshot()
    {
        usbMesg msg;
        int data[4];
        
        // These addresses were obtained from http://en64.shoutwiki.com/wiki/VI_Registers_Detailed
        void* frame = (void*)(0x80000000|(*(u32*)0xA4400004)); // Same as calling osViGetCurrentFramebuffer() in libultra
        u32 yscale = (*(u32*)0xA4400034);
        u32 w = (*(u32*)0xA4400008);
        u32 h = ((((*(u32*)0xA4400028)&0x3FF)-(((*(u32*)0xA4400028)>>16)&0x3FF))*yscale)/2048;
        u8 depth = (((*(u32*)0xA4400000)&0x03) == 0x03) ? 4 : 2;
        
        // Ensure debug mode is initialized
        if (!debug_initialized)
            return;
        
        // Create the data header to send
        data[0] = DATATYPE_SCREENSHOT;
        data[1] = depth;
        data[2] = w;
        data[3] = h;
        
        // Send the header to the USB thread
        msg.msgtype = MSG_WRITE;
        msg.datatype = DATATYPE_HEADER;
        msg.buff = data;
        msg.size = sizeof(data);
        #ifndef LIBDRAGON
            osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
        #else
            debug_thread_usb(&msg);
        #endif
        
        // Send the framebuffer to the USB thread
        msg.msgtype = MSG_WRITE;
        msg.datatype = DATATYPE_SCREENSHOT;
        msg.buff = frame;
        msg.size = depth*w*h;
        #ifndef LIBDRAGON
            osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
        #else
            debug_thread_usb(&msg);
        #endif
    }
    
    
    /*==============================
        _debug_assert
        Halts the program (assumes expression failed)
        @param The expression that was tested
        @param The file where the exception ocurred
        @param The line number where the exception ocurred
    ==============================*/
    
    void _debug_assert(const char* expression, const char* file, int line)
    {
        volatile char crash;
        
        // Set the assert data
        assert_expr = expression;
        assert_line = line;
        assert_file = file;
        
        // If on libdragon, print where the assertion failed
        #ifdef LIBDRAGON
            debug_printf("Assertion failed in file '%s', line %d.\n", assert_file, assert_line);
        #endif
    
        // Intentionally cause a TLB exception on load/instruction fetch
        crash = *(volatile char *)1;
        (void)crash;
    }
        
        
    /*==============================
        debug_64drivebutton
        Assigns a function to be executed when the 64drive button is pressed.
        @param The function pointer to execute
        @param Whether or not to execute the function only on pressing (ignore holding the button down)
    ==============================*/
    
    void debug_64drivebutton(void(*execute)(), char onpress)
    {
        debug_64dbut_func = execute;
        debug_64dbut_debounce = 0;
        debug_64dbut_hold = !onpress;
    }
    
    
    /*==============================
        debug_addcommand
        Adds a command for the USB to listen for
        @param The command name
        @param The command description
        @param The function pointer to execute
    ==============================*/
    
    void debug_addcommand(char* command, char* description, char* (*execute)())
    {
        int entry = command[0]%HASHTABLE_SIZE;
        debugCommand* slot = debug_commands_hashtable[entry];
        
        // Ensure debug mode is initialized
        if (!debug_initialized)
            return;
        
        // Ensure we haven't hit the command limit
        if (debug_commands_count == MAX_COMMANDS)
        {
            debug_printf("Max commands exceeded!\n");
            return;
        }
        
        // Look for an empty spot in the hash table
        if (slot != NULL)
        {
            while (slot->next != NULL)
                slot = slot->next;
            slot->next = &debug_commands_elements[debug_commands_count];
        }
        else
            debug_commands_hashtable[entry] = &debug_commands_elements[debug_commands_count];
            
        // Fill this spot with info about this command
        debug_commands_elements[debug_commands_count].command     = command;
        debug_commands_elements[debug_commands_count].description = description;
        debug_commands_elements[debug_commands_count].execute     = execute;
        debug_commands_count++;
    }
    
    
    /*==============================
        debug_printcommands
        Prints a list of commands to the developer's command prompt.
    ==============================*/
    
    void debug_printcommands()
    {
        int i;
        
        // Ensure debug mode is initialized
        if (!debug_initialized)
            return;
        
        // Ensure there are commands to print
        if (debug_commands_count == 0)
            return;
        
        // Print the commands
        debug_printf("Available USB commands\n----------------------\n");
        for (i=0; i<debug_commands_count; i++)
            debug_printf("%d. %s\n\t%s\n", i+1, debug_commands_elements[i].command, debug_commands_elements[i].description);
        debug_printf("\n");
    }
    
    
    /*==============================
        debug_pollcommands
        Check the USB for incoming commands
    ==============================*/
    
    void debug_pollcommands()
    {
        usbMesg msg;
    
        // Ensure debug mode is initialized
        if (!debug_initialized)
            return;
            
        // Handle 64Drive button polling
        debug_handle_64drivebutton();
        
        // Send a read message to the USB thread
        msg.msgtype = MSG_READ;
        #ifndef LIBDRAGON
            osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
        #else
            debug_thread_usb(&msg);
        #endif
    }
    
    
    /*==============================
        debug_handle_64drivebutton
        Handles the 64Drive's button logic
    ==============================*/
    
    static inline void debug_handle_64drivebutton()
    {
        static u32 held = 0;
    
        // If we own a 64Drive
        if (usb_getcart() == CART_64DRIVE && debug_64dbut_func != NULL)
        {
            u64 curtime;
            #ifndef LIBDRAGON
                curtime = osGetTime();
            #else
                curtime = timer_ticks();
            #endif
            
            // And the debounce time on the 64Drive's button has elapsed
            if (debug_64dbut_debounce < curtime)
            {
                s32 bpoll;
                #ifndef LIBDRAGON
                    osPiReadIo(0xB80002F8, &bpoll);
                #else
                    bpoll = io_read(0xB80002F8);
                #endif
                bpoll = (bpoll&0xFFFF0000)>>16;
                
                // If the 64Drive's button has been pressed, then execute the assigned function and set the debounce timer
                if (bpoll == 0 && (debug_64dbut_hold || !held))
                {
                    u64 nexttime;
                    #ifndef LIBDRAGON
                        nexttime = OS_USEC_TO_CYCLES(100000);
                    #else
                        nexttime = TIMER_TICKS(100000);
                    #endif
                    debug_64dbut_debounce = curtime + nexttime;
                    debug_64dbut_func();
                    held = 1;
                }
                else if (bpoll != 0 && held)
                    held = 0;
            }
        }
    }
    
    
    /*==============================
        debug_sizecommand
        Returns the size of the data from this part of the command
        @return The size of the data in bytes, or 0
    ==============================*/
    
    int debug_sizecommand()
    {
        // If we're out of commands to read, return 0
        if (debug_command_current == debug_command_totaltokens)
            return 0;
        
        // Otherwise, return the amount of data to read
        return debug_command_incoming_size[debug_command_current];
    }
    
    
    /*==============================
        debug_parsecommand
        Stores the next part of the incoming command into the provided buffer.
        Make sure the buffer can fit the amount of data from debug_sizecommand!
        If you pass NULL, it skips this command.
        @param The buffer to store the data in
    ==============================*/
    
    void debug_parsecommand(void* buffer)
    {
        u8 curr = debug_command_current;
        
        // Skip this command if no buffer exists
        if (buffer == NULL)
        {
            debug_command_current++;
            return;
        }
            
        // If we're out of commands to read, do nothing
        if (curr == debug_command_totaltokens)
            return;
            
        // Read from the correct offset
        usb_skip(debug_command_incoming_start[curr]);
        usb_read(buffer, debug_command_incoming_size[curr]);
        usb_rewind(debug_command_incoming_size[curr]+debug_command_incoming_start[curr]);
        debug_command_current++;
    }
    
    
    /*==============================
        debug_commands_setup
        Reads the entire incoming string and breaks it into parts for 
        debug_parsecommand and debug_sizecommand
    ==============================*/
    
    static void debug_commands_setup()
    {
        int i;
        int datasize = USBHEADER_GETSIZE(usb_poll());
        int dataleft = datasize;
        int filesize = 0;
        char filestep = 0;
        
        // Initialize the starting offsets at -1
        memset(debug_command_incoming_start, -1, COMMAND_TOKENS*sizeof(int));
        
        // Read data from USB in blocks
        while (dataleft > 0)
        {
            int readsize = BUFFER_SIZE;
            if (readsize > dataleft)
                readsize = dataleft;
            
            // Read a block from USB
            memset(debug_buffer, 0, BUFFER_SIZE);
            usb_read(debug_buffer, readsize);
            
            // Parse the block
            for (i=0; i<readsize && dataleft > 0; i++)
            {
                // If we're not reading a file
                int offset = datasize-dataleft;
                u8 tok = debug_command_totaltokens;
                
                // Decide what to do based on the current character
                switch (debug_buffer[i])
                {
                    case ' ':
                    case '\0':
                        if (filestep < 2)
                        {
                            if (debug_command_incoming_start[tok] != -1)
                            {
                                debug_command_incoming_size[tok] = offset-debug_command_incoming_start[tok];
                                debug_command_totaltokens++;
                            }
                            
                            if (debug_buffer[i] == '\0')
                                dataleft = 0;
                            break;
                        }
                    case '@':
                        filestep++;
                        if (filestep < 3)
                            break;
                    default:
                        // Decide what to do based on the file handle
                        if (filestep == 0 && debug_command_incoming_start[tok] == -1)
                        {
                            // Store the data offsets and sizes in the global command buffers 
                            debug_command_incoming_start[tok] = offset;
                        }
                        else if (filestep == 1)
                        {
                            // Get the filesize
                            filesize = filesize*10 + debug_buffer[i]-'0';
                        }
                        else if (filestep > 1)
                        {
                            // Store the file offsets and sizes in the global command buffers 
                            debug_command_incoming_start[tok] = offset;
                            debug_command_incoming_size[tok] = filesize;
                            debug_command_totaltokens++;
                            
                            // Skip a bunch of bytes
                            if ((readsize-i)-filesize < 0)
                                usb_skip(filesize-(readsize-i));
                            dataleft -= filesize;
                            i += filesize;
                            filesize = 0;
                            filestep = 0;
                        }
                        break;
                }
                dataleft--;
            }
        }
        
        // Rewind the USB fully
        usb_rewind(datasize);
    }
    
    
    /*==============================
        debug_thread_usb
        Handles the USB thread
        @param Arbitrary data that the thread can receive
    ==============================*/
    
    static void debug_thread_usb(void *arg)
    {
        char errortype = USBERROR_NONE;
        usbMesg* threadMsg;
        
        #ifndef LIBDRAGON
            // Create the message queue for the USB message
            osCreateMesgQueue(&usbMessageQ, &usbMessageBuf, 1);
        #else
            // Set the received thread message to the argument
            threadMsg = (usbMesg*)arg;
        #endif
        
        // Thread loop
        while (1)
        {
            #ifndef LIBDRAGON
                // Wait for a USB message to arrive
                osRecvMesg(&usbMessageQ, (OSMesg *)&threadMsg, OS_MESG_BLOCK);
            #endif
            
            // Ensure there's no data in the USB (which handles MSG_READ)
            while (usb_poll() != 0)
            {
                int header = usb_poll();
                debugCommand* entry;
                
                // RDB packets should be rerouted to the RDB thread
                #if USE_RDBTHREAD
                    if (USBHEADER_GETTYPE(header) == DATATYPE_RDBPACKET)
                    {
                        osSendMesg(&rdbMessageQ, (OSMesg)MSG_RDB_PACKET, OS_MESG_BLOCK);
                        continue;
                    }
                #endif
                
                // Ensure we're receiving a text command
                if (USBHEADER_GETTYPE(header) != DATATYPE_TEXT)
                {
                    errortype = USBERROR_NOTTEXT;
                    usb_purge();
                    break;
                }
                
                // Initialize the command trackers
                debug_command_totaltokens = 0;
                debug_command_current = 0;
                    
                // Break the USB command into parts
                debug_commands_setup();
                
                // Ensure we don't read past our buffer
                if (debug_sizecommand() > BUFFER_SIZE)
                {
                    errortype = USBERROR_TOOMUCH;
                    usb_purge();
                    break;
                }
                
                // Read from the USB to retrieve the command name
                debug_parsecommand(debug_buffer);
                
                // Iterate through the hashtable to see if we find the command
                entry = debug_commands_hashtable[debug_buffer[0]%HASHTABLE_SIZE];
                while (entry != NULL)
                {
                    // If we found the command
                    if (!strncmp(debug_buffer, entry->command, debug_command_incoming_size[0]))
                    {                            
                        // Execute the command function and exit the while loop
                        debug_command_error = entry->execute();
                        if (debug_command_error != NULL)
                            errortype = USBERROR_CUSTOM;
                        usb_purge();
                        break;
                    }
                    entry = entry->next;
                }
                
                // If no command was found
                if (entry == NULL)
                {
                    // Purge the USB contents and print unknown command
                    usb_purge();
                    errortype = USBERROR_UNKNOWN;
                }
            }
            
            // Spit out an error if there was one during the command parsing
            if (errortype != USBERROR_NONE)
            {
                switch (errortype)
                {
                    case USBERROR_NOTTEXT:
                        usb_write(DATATYPE_TEXT, "Error: USB data was not text\n", 29+1);
                        break;
                    case USBERROR_UNKNOWN:
                        usb_write(DATATYPE_TEXT, "Error: Unknown command\n", 23+1);
                        break;
                    case USBERROR_TOOMUCH:
                        usb_write(DATATYPE_TEXT, "Error: Command too large\n", 25+1);
                        break;
                    case USBERROR_CUSTOM:
                        usb_write(DATATYPE_TEXT, debug_command_error, strlen(debug_command_error)+1);
                        usb_write(DATATYPE_TEXT, "\n", 1+1);
                        break;
                }
                errortype = USBERROR_NONE;
            }
            
            // Handle the other USB messages
            switch (threadMsg->msgtype)
            {
                case MSG_WRITE:
                    if (usb_timedout())
                        usb_sendheartbeat();
                    usb_write(threadMsg->datatype, threadMsg->buff, threadMsg->size);
                    break;
            }
            
            // If we're in libdragon, break out of the loop as we don't need it
            #ifdef LIBDRAGON
                break;
            #endif
        }
    }
    
    #ifndef LIBDRAGON
        #if OVERWRITE_OSPRINT
        
            /*==============================
                debug_osSyncPrintf_implementation
                Overwrites osSyncPrintf calls with this one
                @param Unused
                @param The buffer with the string
                @param The amount of characters to write
                @returns The end of the buffer that was written to
            ==============================*/
            
            static void* debug_osSyncPrintf_implementation(void *unused, const char *str, size_t len)
            {
                void* ret;
                usbMesg msg;
                
                // Clear the debug buffer and copy the formatted string to it
                memset(debug_buffer, 0, len+1);
                ret =  ((char *) memcpy(debug_buffer, str, len) + len);
                
                // Send the printf to the usb thread
                msg.msgtype = MSG_WRITE;
                msg.datatype = DATATYPE_TEXT;
                msg.buff = debug_buffer;
                msg.size = len+1;
                osSendMesg(&usbMessageQ, (OSMesg)&msg, OS_MESG_BLOCK);
                
                // Return the end of the buffer
                return ret;
            }
            
        #endif 
        
        #if USE_FAULTTHREAD
            
            /*==============================
                debug_printreg
                Prints info about a register
                @param The value of the register
                @param The name of the register
                @param The registry description to use
            ==============================*/
            
            static void debug_printreg(u32 value, char *name, regDesc *desc)
            {
                char first = 1;
                debug_printf("%s\t\t0x%16x <", name, value);
                while (desc->mask != 0) 
                {
                    if ((value & desc->mask) == desc->value) 
                    {
                        (first) ? (first = 0) : ((void)debug_printf(","));
                        debug_printf("%s", desc->string);
                    }
                    desc++;
                }
                debug_printf(">\n");
            }
            
            
            /*==============================
                debug_thread_fault
                Handles the fault thread
                @param Arbitrary data that the thread can receive
            ==============================*/
            
            static void debug_thread_fault(void *arg)
            {
                OSMesg msg;
                OSThread *curr;
                
                // Create the message queue for the fault message
                osCreateMesgQueue(&faultMessageQ, &faultMessageBuf, 1);
                osSetEventMesg(OS_EVENT_FAULT, &faultMessageQ, (OSMesg)MSG_FAULT);
                
                // Thread loop
                while (1)
                {
                    // Wait for a fault message to arrive
                    osRecvMesg(&faultMessageQ, (OSMesg *)&msg, OS_MESG_BLOCK);
                    
                    // Get the faulted thread
                    curr = (OSThread *)__osGetCurrFaultedThread();
                    if (curr != NULL) 
                    {
                        __OSThreadContext* context = &curr->context;
                        
                        // Print the basic info
                        debug_printf("Fault in thread: %d\n\n", curr->id);
                        debug_printf("pc\t\t0x%16x\n", context->pc);
                        if (assert_file == NULL)
                            debug_printreg(context->cause, "cause", causeDesc);
                        else
                            debug_printf("cause\t\tAssertion failed in file '%s', line %d.\n", assert_file, assert_line);
                        debug_printreg(context->sr, "sr", srDesc);
                        debug_printf("badvaddr\t0x%16x\n\n", context->badvaddr);
                        
                        // Print the registers
                        debug_printf("at 0x%016llx v0 0x%016llx v1 0x%016llx\n", context->at, context->v0, context->v1);
                        debug_printf("a0 0x%016llx a1 0x%016llx a2 0x%016llx\n", context->a0, context->a1, context->a2);
                        debug_printf("a3 0x%016llx t0 0x%016llx t1 0x%016llx\n", context->a3, context->t0, context->t1);
                        debug_printf("t2 0x%016llx t3 0x%016llx t4 0x%016llx\n", context->t2, context->t3, context->t4);
                        debug_printf("t5 0x%016llx t6 0x%016llx t7 0x%016llx\n", context->t5, context->t6, context->t7);
                        debug_printf("s0 0x%016llx s1 0x%016llx s2 0x%016llx\n", context->s0, context->s1, context->s2);
                        debug_printf("s3 0x%016llx s4 0x%016llx s5 0x%016llx\n", context->s3, context->s4, context->s5);
                        debug_printf("s6 0x%016llx s7 0x%016llx t8 0x%016llx\n", context->s6, context->s7, context->t8);
                        debug_printf("t9 0x%016llx gp 0x%016llx sp 0x%016llx\n", context->t9, context->gp, context->sp);
                        debug_printf("s8 0x%016llx ra 0x%016llx\n\n",            context->s8, context->ra);
                        
                        // Print the floating point registers
                        debug_printreg(context->fpcsr, "fpcsr", fpcsrDesc);
                        debug_printf("\n");
                        debug_printf("d0  %.15e\td2  %.15e\n", context->fp0.d,  context->fp2.d);
                        debug_printf("d4  %.15e\td6  %.15e\n", context->fp4.d,  context->fp6.d);
                        debug_printf("d8  %.15e\td10 %.15e\n", context->fp8.d,  context->fp10.d);
                        debug_printf("d12 %.15e\td14 %.15e\n", context->fp12.d, context->fp14.d);
                        debug_printf("d16 %.15e\td18 %.15e\n", context->fp16.d, context->fp18.d);
                        debug_printf("d20 %.15e\td22 %.15e\n", context->fp20.d, context->fp22.d);
                        debug_printf("d24 %.15e\td26 %.15e\n", context->fp24.d, context->fp26.d);
                        debug_printf("d28 %.15e\td30 %.15e\n", context->fp28.d, context->fp30.d);
                    }
                }
            }
            
        #endif
        
        #if USE_RDBTHREAD
            
            /*==============================
                debug_thread_rdb
                Handles the remote debugger thread
                @param Arbitrary data that the thread can receive
            ==============================*/
            
            static void debug_thread_rdb(void *arg)
            {
                OSId mainid = (OSId)arg;
                OSThread* mainthread = &rdbThread;
                            
                // Find the main thread pointer given its ID
                while (mainthread->id != mainid)
                    mainthread = mainthread->tlnext;
            
                // Create the message queue for the rdb messages
                osCreateMesgQueue(&rdbMessageQ, &rdbMessageBuf, 1);
                
                // Initialize breakpoints
                osSetEventMesg(OS_EVENT_CPU_BREAK, &rdbMessageQ, (OSMesg)MSG_RDB_BPHIT);
                memset(debug_bpoints, 0, BPOINT_COUNT*sizeof(bPoint));
                
                // Thread loop
                while (1)
                {
                    OSMesg msg;
                    OSThread* affected = NULL;

                    // Wait for an rdb message to arrive
                    osRecvMesg(&rdbMessageQ, (OSMesg*)&msg, OS_MESG_BLOCK);

                    // Check what message we received
                    switch ((s32)msg)
                    {
                        case MSG_RDB_PACKET:
                            break; // Do nothing
                        case MSG_RDB_BPHIT:
                            affected = mainthread;
                            debug_rdbpaused = TRUE;
                            
                            // Find out which thread hit the bp exception
                            while (1) 
                            {
                                if (affected->flags & OS_FLAG_CPU_BREAK)
                                    break;
                                affected = affected->tlnext;
                            }
                            usb_purge();
                            usb_write(DATATYPE_RDBPACKET, "T05swbreak:;", 12+1);
                            usb_write(DATATYPE_TEXT, "We hit the breakpoint\n", 22+1);
                            break;
                        case MSG_RDB_PAUSE:
                            // Since USB polling should be done in main, the main thread will obviously be the one which is paused
                            affected = mainthread;
                            debug_rdbpaused = TRUE;
                            break;
                    }
                        
                    // Handle the RDB packet
                    do
                    {
                        int usbheader = usb_poll();
                        if (USBHEADER_GETTYPE(usbheader) == DATATYPE_RDBPACKET)
                        {
                            int i;
                            u8 found = FALSE;
                            
                            // Read the GDB packet from USB
                            memset(debug_buffer, 0, BUFFER_SIZE);
                            usb_read(&debug_buffer, (USBHEADER_GETSIZE(usbheader) <= BUFFER_SIZE) ? USBHEADER_GETSIZE(usbheader) : BUFFER_SIZE);

                            // Run a function based on what we received
                            for (i=0; i<(sizeof(lut_rdbpackets)/sizeof(lut_rdbpackets[0])); i++)
                            {
                                if (!strncmp(lut_rdbpackets[i].command, debug_buffer, strlen(lut_rdbpackets[i].command)))
                                {
                                    found = TRUE;
                                    lut_rdbpackets[i].func(affected);
                                    break;
                                }
                            }
                            
                            // If we didn't find a supported command, then reply back with nothing
                            if (!found)
                            {
                                char empty = '\0';
                                usb_purge();
                                usb_write(DATATYPE_RDBPACKET, &empty, 1);
                                usb_write(DATATYPE_TEXT, "Unknown\n", 8+1);
                            }
                        }
                        usb_purge();
                    }
                    while (debug_rdbpaused); // Loop forever while we are paused
                }
            }
            
            
            /*==============================
                debug_rdb_qsupported
                Responds to GDB with the supported features
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_qsupported(OSThread* t)
            {
                usb_purge();
                usb_write(DATATYPE_RDBPACKET, "PacketSize=200;swbreak+", 23+1);
            }
            
            
            /*==============================
                debug_rdb_halt
                Responds to GDB with the halt reason
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_halt(OSThread* t)
            {
                usb_purge();
                usb_write(DATATYPE_RDBPACKET, "S05", 3+1);
            }
            
            
            /*==============================
                debug_rdb_dumpregisters
                Responds to GDB with a dump of all registers
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_dumpregisters(OSThread* t)
            {
                if (t != NULL)
                {
                    int i;
                    __OSThreadContext* context = &t->context;
                    char output[(40*16) + 1];
                    
                    // Perform the humpty dumpty
                    sprintf(output+(0*16), "%016llx%016llx%016llx%016llx", 
                        0,                (u64)context->at, (u64)context->v0, (u64)context->v1
                    );
                    sprintf(output+(4*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->a0, (u64)context->a1, (u64)context->a2, (u64)context->a3
                    );
                    sprintf(output+(8*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->t0, (u64)context->t1, (u64)context->t2, (u64)context->t3
                    );
                    sprintf(output+(12*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->t4, (u64)context->t5, (u64)context->t6, (u64)context->t7
                    );
                    sprintf(output+(16*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->s0, (u64)context->s1, (u64)context->s2, (u64)context->s3
                    );
                    sprintf(output+(20*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->s4, (u64)context->s5, (u64)context->s6, (u64)context->s7
                    );
                    sprintf(output+(24*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->t8, (u64)context->t9, "xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxxxxx"
                    );
                    sprintf(output+(28*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->gp, (u64)context->sp, (u64)context->s8,   (u64)context->ra
                    );
                    sprintf(output+(32*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->sr,    (u64)context->lo, (u64)context->hi,    (u64)context->badvaddr
                    );
                    sprintf(output+(36*16), "%016llx%016llx%016llx%016llx", 
                        (u64)context->cause, (u64)context->pc, (u64)context->fpcsr, "xxxxxxxxxxxxxxxx"
                    );
                    
                    // Send the register dump
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, output, sizeof(output)/sizeof(output[0]));
                }
                else
                {
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, "E00", 3+1);
                }
            }
            
            
            /*==============================
                register_define
                Fills in our helper regType struct
                @param The affected thread, if any
            ==============================*/
            
            static regType register_define(int size, void* addr)
            {
                regType ret = {size, addr};
                return ret;
            }
            
            
            /*==============================
                debug_rdb_writeregisters
                Writes a set of registers from a GDB packet
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_writeregisters(OSThread* t)
            {
                if (t != NULL)
                {
                    int i;
                    regType registers[40];
                    __OSThreadContext* context = &t->context;
                    
                    // The incoming data probably won't fit in the buffer, so we'll go bit by bit
                    usb_rewind(BUFFER_SIZE);
                    
                    // Skip the 'G' at the start of the command
                    usb_skip(1);
                    
                    // Setup the thread context register definition
                    i=0;
                    registers[i++] = register_define(0, NULL); // Zero
                    registers[i++] = register_define(8, &context->at);
                    registers[i++] = register_define(8, &context->v0);
                    registers[i++] = register_define(8, &context->v1);
                    registers[i++] = register_define(8, &context->a0);
                    registers[i++] = register_define(8, &context->a1);
                    registers[i++] = register_define(8, &context->a2);
                    registers[i++] = register_define(8, &context->a3);
                    registers[i++] = register_define(8, &context->t0);
                    registers[i++] = register_define(8, &context->t1);
                    registers[i++] = register_define(8, &context->t2);
                    registers[i++] = register_define(8, &context->t3);
                    registers[i++] = register_define(8, &context->t4);
                    registers[i++] = register_define(8, &context->t5);
                    registers[i++] = register_define(8, &context->t6);
                    registers[i++] = register_define(8, &context->t7);
                    registers[i++] = register_define(8, &context->s0);
                    registers[i++] = register_define(8, &context->s1);
                    registers[i++] = register_define(8, &context->s2);
                    registers[i++] = register_define(8, &context->s3);
                    registers[i++] = register_define(8, &context->s4);
                    registers[i++] = register_define(8, &context->s5);
                    registers[i++] = register_define(8, &context->s6);
                    registers[i++] = register_define(8, &context->s7);
                    registers[i++] = register_define(8, &context->t8);
                    registers[i++] = register_define(8, &context->t9);
                    registers[i++] = register_define(0, NULL); // K0
                    registers[i++] = register_define(0, NULL); // K1
                    registers[i++] = register_define(8, &context->gp);
                    registers[i++] = register_define(8, &context->sp);
                    registers[i++] = register_define(8, &context->s8);
                    registers[i++] = register_define(8, &context->ra);
                    registers[i++] = register_define(4, &context->sr);
                    registers[i++] = register_define(8, &context->lo);
                    registers[i++] = register_define(8, &context->hi);
                    registers[i++] = register_define(4, &context->badvaddr);
                    registers[i++] = register_define(4, &context->cause);
                    registers[i++] = register_define(4, &context->pc);
                    registers[i++] = register_define(4, &context->fpcsr);
                    registers[i++] = register_define(0, NULL); // FIR
                    
                    // Do the writing
                    for (i=0; i<40; i++)
                    {
                        char val[8+1];
                        usb_read(val, 8);
                        val[8] = '\0';
                        if (val[0] != 'x' && registers[i].ptr != NULL)
                        {
                            if (registers[i].size == 4)
                                (*(u32*)registers[i].ptr) = strtol(val, NULL, 16);
                            else
                                (*(u64*)registers[i].ptr) = strtol(val, NULL, 16);
                        }
                    }
                    
                    // Done
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, "OK", 2+1);
                }
                else
                {
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, "E00", 3+1);
                }
            }
            
            
            /*==============================
                debug_rdb_readmemory
                Responds to GDB with a memory read
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_readmemory(OSThread* t)
            {
                int i;
                u32 addr;
                u32 size;
                char command[32];
                char* commandp = &command[0];
                strcpy(commandp, debug_buffer);
                
                // Skip the 'm' at the start of the command
                commandp++;
                
                // Extract the address value
                strtok(commandp, ",");
                addr = (u32)strtol(commandp, NULL, 16);
                
                // Extract the size value
                commandp = strtok(NULL, ",");
                size = atoi(commandp);
                
                // We need to translate the address before trying to read it
                if ((addr & 0xFF000000) == 0xA4000000 || (addr & 0xFF000000) == 0x04000000)
                    addr = (u32)OS_PHYSICAL_TO_K1(addr & 0x0FFFFFFF);
                else
                {
                    addr = (u32)osVirtualToPhysical((u32*)addr);
                    if (addr >= osGetMemSize())
                        addr = 0;
                    else
                        addr = (u32)OS_PHYSICAL_TO_K0(addr);
                }
                
                // Ensure we are reading a valid memory address
                if (addr >= 0x80000000 && addr < 0x80000000 + osGetMemSize())
                {
                    osWritebackDCache((u32*)addr, size);
                    
                    // Read the memory address, one byte at a time
                    for (i=0; i<size; i++)
                    {
                        u8 val;                    
                        val = *(((volatile u8*)addr)+i);
                        sprintf(debug_buffer+(i*2), "%02x", val);
                    }
                    
                    // Send the address dump
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, &debug_buffer, strlen(debug_buffer)+1);
                }
                else
                {
                    for (i=0; i<size; i++)
                        sprintf(debug_buffer+(i*2), "00");
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, &debug_buffer, strlen(debug_buffer)+1);
                }
            }
            
            
            /*==============================
                debug_rdb_writememory
                Writes the memory from a GDB packet
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_writememory(OSThread* t)
            {
                int i;
                u32 addr;
                u32 size;
                char* commandp = &debug_buffer[0];
                
                // Skip the 'M' at the start of the command
                commandp++;
                
                // Extract the address value
                strtok(commandp, ",");
                addr = (u32)strtol(commandp, NULL, 16);
                
                // Extract the size value
                commandp = strtok(NULL, ":");
                size = atoi(commandp);
                
                // Finally, point to the data we're actually gonna write
                commandp = strtok(NULL, "\0");
                
                // We need to translate the address before trying to write to it
                if ((addr & 0xFF000000) == 0xA4000000 || (addr & 0xFF000000) == 0x04000000)
                    addr = (u32)OS_PHYSICAL_TO_K1(addr & 0x0FFFFFFF);
                else
                {
                    addr = (u32)osVirtualToPhysical((u32*)addr);
                    if (addr >= osGetMemSize())
                        addr = 0;
                    else
                        addr = (u32)OS_PHYSICAL_TO_K0(addr);
                }
                
                // Ensure we are writing to a valid memory address
                if (addr >= 0x80000000 && addr < 0x80000000 + osGetMemSize())
                {
                    // Read the memory address, one byte at a time
                    for (i=0; i<size; i++)
                    {
                        char byte[3];
                        sprintf(byte, "%.2s", commandp+(i*2));
                        *(((volatile u8*)addr)+i) = (u8)strtol(byte, NULL, 16);
                    }
                    
                    // Done
                    osWritebackDCache((u32*)addr, size);
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, "OK", 2+1);
                }
                else
                {
                    usb_purge();
                    usb_write(DATATYPE_RDBPACKET, "E00", 3+1);
                }
            }
            
            
            /*==============================
                debug_rdb_addbreakpoint
                Enables a breakpoint
                @param The affected thread, if any
            ==============================*/
            
            void debug_rdb_addbreakpoint(OSThread* t)
            {
                int i;
                u32 addr;
                char command[32];
                char* commandp = &command[0];
                strcpy(commandp, debug_buffer);
                
                // Skip the Z0 at the start
                strtok(commandp, ",");
                
                // Extract the address value
                commandp = strtok(NULL, ",");
                addr = (u32)strtol(commandp, NULL, 16);
                
                // There's still one more byte left (the breakpoint kind) which we can ignore
                
                // We need to translate the address before trying to put the breakpoint on it
                if ((addr & 0xFF000000) == 0xA4000000 || (addr & 0xFF000000) == 0x04000000)
                    addr = (u32)OS_PHYSICAL_TO_K1(addr & 0x0FFFFFFF);
                else
                {
                    addr = (u32)osVirtualToPhysical((u32*)addr);
                    if (addr >= osGetMemSize())
                        addr = 0;
                    else
                        addr = (u32)OS_PHYSICAL_TO_K0(addr);
                }
                
                // Find an empty slot in our breakpoint array and store the breakpoint info there
                for (i=0; i<BPOINT_COUNT; i++)
                {
                    if (debug_bpoints[i].addr == (u32*)addr) // No need to re-add the bp if it already exists
                    {
                        usb_purge();
                        usb_write(DATATYPE_RDBPACKET, "OK", 2+1);
                        return;
                    }
                    if (debug_bpoints[i].addr == NULL)
                    {
                        // The first address is the one we want to breakpoint at
                        // The second address is the address of the instruction that comes after it. This will be of use later
                        debug_bpoints[i].addr = (u32*)addr;
                        debug_bpoints[i].instruction = *((u32*)addr);
                        debug_bpoints[i].next_addr = (u32*)(addr+4);
                        
                        // A breakpoint on the R4300 is any invalid instruction (It's an exception). 
                        // The first 6 bits of the opcodes are reserved for the instruction itself.
                        // So since we have a range of values, we can encode the index into the instruction itself, in the middle 20 bits
                        // Zero is reserved for temporary breakpoints (which are used when stepping through code)
                        *((u32*)addr) = MAKE_BREAKPOINT_INDEX(i+1);
                        osWritebackDCache((u32*)addr, 4);
                        osInvalICache((u32*)addr, 4);
                        
                        // Tell GDB we succeeded
                        usb_purge();
                        usb_write(DATATYPE_RDBPACKET, "OK", 2+1);
                        return;
                    }
                }
            
                // Some failure happend
                usb_purge();
                usb_write(DATATYPE_RDBPACKET, "E00", 3+1);
            }
            
            
            /*==============================
                debug_rdb_removebreakpoint
                Disables a breakpoint
                @param The affected thread, if any
            ==============================*/
            
            static void debug_rdb_removebreakpoint(OSThread* t)
            {
                usb_purge();
                usb_write(DATATYPE_TEXT, "Breakpoint Remove\n", 18+1);
                /*
                u8 bytes[9];
                u32* addr1;
                u32* addr2;
                usb_read(bytes, 9);
                
                addr1 = (u32*)((((u32)bytes[0])<<0) | (((u32)bytes[1])<<8) | (((u32)bytes[2])<<16) | (((u32)bytes[3])<<24));
                addr2 = (u32*)((((u32)bytes[4])<<0) | (((u32)bytes[5])<<8) | (((u32)bytes[6])<<16) | (((u32)bytes[7])<<24));
                if (bytes[8] == 1)
                {
                    int i;
                    
                    // Find an empty slot in our breakpoint array and store the breakpoint info there
                    for (i=0; i<BPOINT_COUNT; i++)
                    {
                        if (debug_bpoints[i].addr == addr1) // No need to re-add the bp if it already exists
                            return;
                        if (debug_bpoints[i].addr == NULL)
                        {
                            // The USB packet contains two addresses, the first address is the one we want to breakpoint at
                            // The second address is the address of the instruction that comes after it. This will be of use later
                            debug_bpoints[i].addr = addr1;
                            debug_bpoints[i].instruction = *addr1;
                            debug_bpoints[i].next_addr = addr2;
                            
                            // A breakpoint on the R4300 is any invalid instruction (It's an exception). 
                            // The first 6 bits of the opcodes are reserved for the instruction itself.
                            // So since we have a range of values, we can encode the index into the instruction itself, in the middle 20 bits
                            // Zero is reserved for temporary breakpoints (which are used when stepping through code)
	                        *addr1 = MAKE_BREAKPOINT_INDEX(i+1);
	                        osWritebackDCache(addr1, 4);
	                        osInvalICache(addr1, 4);
                            break;
                        }
                    }
                }
                else
                {
                    int index = GET_BREAKPOINT_INDEX(*addr1)-1;
                    
                    // Ensure the address has a valid breakpoint
                    if (debug_bpoints[index].addr == addr1)
                    {
                        int i;
                        
                        // Remove the breakpoint
                        *addr1 = debug_bpoints[index].instruction;
                        osWritebackDCache(addr1, 4);
                        osInvalICache(addr1, 4);
                        
                        // Move all the breakpoints in front of it back
                        for (i=index; i<BPOINT_COUNT; i++)
                        {
                            if (debug_bpoints[i].addr == NULL)
                                break;
                            if (i == BPOINT_COUNT-1)
                            {
                                debug_bpoints[i].addr = NULL;
                                debug_bpoints[i].instruction = 0;
                                debug_bpoints[i].next_addr = 0;
                            }
                            else
                            {
                                debug_bpoints[i].addr = debug_bpoints[i+1].addr;
                                debug_bpoints[i].instruction = debug_bpoints[i+1].instruction;
                                debug_bpoints[i].next_addr = debug_bpoints[i+1].next_addr;
                            }
                        }
                    }
                }
                */
            }
            
            /*==============================
                debug_rdb_continue
                Handles continue
                @param The affected thread, if any
            ==============================*/
            static void debug_rdb_continue(OSThread* t)
            {
                debug_rdbpaused = FALSE;
                usb_purge();
                usb_write(DATATYPE_RDBPACKET, "OK", 2+1);
                /*
                u32 nexti, *addr, index;
                OSThread* thread = __osGetActiveQueue();
                bPoint* point;
                
                // Find which thread faulted and get the breakpoint index
                while (thread->tlnext != NULL)
                {
                    u32 inst = *(u32*)thread->context.pc;
                    if ((inst & 0xFC00003F) == 0x0000000D)
                    {
                        index = GET_BREAKPOINT_INDEX(inst);
                        point = &debug_bpoints[index-1];
                        break;
                    }
                    thread = thread->tlnext;
                }
                
                // Set the breakpointed instruction back to what it was originally
                addr = point->addr;
                *addr = point->instruction;
                osWritebackDCache(addr, 4);
                osInvalICache(addr, 4);
                
                // Because we're gonna have to put the breakpoint back after the original instruction is run,
                // We're gonna set the address after it to a second breakpoint
                nexti = (*point->next_addr);
                (*point->next_addr) = MAKE_BREAKPOINT_INDEX(index);
                osWritebackDCache(point->next_addr, 4);
                osInvalICache(point->next_addr, 4);
                
                // Now yield the thread until that second breakpoint is hit
                osRecvMesg(&rdbMessageQ, NULL, OS_MESG_BLOCK);
                
                // If we're here, the second breakpoint was hit. Let's turn it back to what it was originally
                (*point->next_addr) = nexti;
                osWritebackDCache(point->next_addr, 4);
                osInvalICache(point->next_addr, 4);
                
                // Restore the original breakpoint
                (*point->addr) = MAKE_BREAKPOINT_INDEX(index);
                osWritebackDCache(point->addr, 4);
                osInvalICache(point->addr, 4);
                */
            }
        #endif
        
    #endif
    
#endif