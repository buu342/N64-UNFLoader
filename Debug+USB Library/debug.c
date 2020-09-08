/***************************************************************
                            debug.c
                               
A basic debug library that makes use of the USB library for N64
flashcarts. 
https://github.com/buu342/N64-UNFLoader
***************************************************************/

#include <ultra64.h> 
#include <stdarg.h> // Located in ultra\GCC\MIPSE\INCLUDE
#include <PR/os_internal.h> // Needed for Crash's Linux toolchain
#include <string.h> // Needed for Crash's Linux toolchain
#include "debug.h"


#if DEBUG_MODE
    
    /*********************************
           Standard Definitions
    *********************************/
    
    #define MSG_FAULT	0x10
    
    
    /*********************************
                 Structs
    *********************************/

    typedef struct {
        u32 mask;
        u32 value;
        char *string;
    } regDesc;
    
    
    /*********************************
            Function Prototypes
    *********************************/
    
    #if USE_FAULTTHREAD
        static void debug_thread_fault(void *arg);
    #endif

    #if OVERWRITE_OSPRINT
        static void* debug_osSyncPrintf_implementation(void *str, const char *buf, size_t n);
    #endif
    
    
    /*********************************
                 Globals
    *********************************/

    // Function pointers
    #if OVERWRITE_OSPRINT
        extern void* __printfunc;
    #endif

    // Debug globals
    static char debug_initialized = 0;

    // Assertion globals
    static int assert_line = 0;
    static const char* assert_file = NULL;
    static const char* assert_expr = NULL;

    // Fault thread globals
    #if USE_FAULTTHREAD
        static OSMesgQueue faultMessageQ;
        static OSMesg      faultMessageBuf;
        static OSThread    faultThread;
        static u64         faultThreadStack[FAULT_THREAD_STACK/sizeof(u64)];
    #endif

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
        usb_initialize();
    
        // Overwrite osSyncPrintf
        #if OVERWRITE_OSPRINT
            __printfunc = (void*)debug_osSyncPrintf_implementation;
        #endif
        
        // Initialize the fault thread
        #if USE_FAULTTHREAD
            osCreateThread(&faultThread, FAULT_THREAD_ID, debug_thread_fault, 0, 
                            (faultThreadStack+FAULT_THREAD_STACK/sizeof(u64)), 
                            FAULT_THREAD_PRI);
            osStartThread(&faultThread);
        #endif
        
        // Mark the debug mode as initialized
        debug_initialized = 1;
    }
    
    
    /*==============================
        debug_printf
        Prints a formatted message to the developer's command prompt.
        Supports up to 256 characters.
        @param A string to print
        @param variadic arguments to print as well
    ==============================*/

    void debug_printf(const char* message, ...)
    {
        va_list args;
        int i, j=0, delimcount;
        int size = strlen(message);
        char buff[256] = {0};
        char isdelim = FALSE;
        char delim[8] = {0};
        
        // Get the variadic arguments
        va_start(args, message);

        // Build the string
        for (i=0; i<size; i++)
        {      
            // Decide if we're dealing with %% or something else
            if (message[i] == '%')
            {
                // Handle printing %%
                if ((i != 0 && message[i-1] == '%') || (i+1 != size && message[i+1] == '%'))
                {
                    buff[j++] = message[i];
                    isdelim = FALSE;
                    continue;
                }
                
                // Handle delimiter start
                if (!isdelim)
                {
                    memset(delim, 0, sizeof(delim)/sizeof(delim[0]));
                    isdelim = TRUE;
                    delim[0] = '%';
                    delimcount = 1;
                    continue;
                }
            }
          
            // If we're dealing with something else
            if (isdelim)
            {
                char numbuf[25] = {0};
                char* vastr = NULL;

                // Pick what to attach based on the character after the percentage symbol
                delim[delimcount++] = message[i];
                switch(message[i])
                {
                    case 'c':
                        buff[j++] = (char)va_arg(args, int);
                        isdelim = FALSE;
                        break;
                    case 's':
                        vastr = (char*)va_arg(args, char*);
                        break;
                    case 'u':
                        sprintf(numbuf, delim, (unsigned int)va_arg(args, unsigned int));
                        break;
                    case 'f':
                    case 'e':
                        sprintf(numbuf, delim, (double)va_arg(args, double));
                        break;
                    case 'x':
                    case 'i':
                    case 'd':
                        sprintf(numbuf, delim, (int)va_arg(args, int));
                        break;
                    case 'p':
                        sprintf(numbuf, delim, (void*)va_arg(args, void*));
                        break;
                    case 'l':
                    case '.':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        continue;
                    default:
                        buff[j++] = '?';
                        isdelim = FALSE;
                        continue;
                }
                            
                // Append the data to the end of our string
                if (vastr != NULL)
                {
                    int vastrlen = strlen(vastr);
                    strcat (buff, vastr);
                    j += vastrlen;
                    isdelim = FALSE;
                }
                else if (numbuf[0] != '\0')
                {
                    int vastrlen = strlen(numbuf);
                    strcat (buff, numbuf);
                    j += vastrlen;
                    isdelim = FALSE;
                }
            }
            else
                buff[j++] = message[i];
        }
        va_end(args); 
        
        // Get the new size
        size = strlen(buff)+1;
            
        // Call the USB write function for the specific flashcart
        usb_write(DATATYPE_TEXT, buff, size);
    }
    
    
    /*==============================
        debug_screenshot
        Sends the currently displayed framebuffer through USB.
        Does not pause the drawing thread!
        @param The size of each pixel of the framebuffer in bytes
               Typically 4 if 32-bit or 2 if 16-bit
        @param The width of the framebuffer
        @param The height of the framebuffer
    ==============================*/
    
    void debug_screenshot(int size, int w, int h)
    {
        void* frame = osViGetCurrentFramebuffer();
        int data[4];
        data[0] = DATATYPE_SCREENSHOT;
        data[1] = size;
        data[2] = w;
        data[3] = h;
        
        usb_write(DATATYPE_HEADER, data, sizeof(data));
        usb_write(DATATYPE_SCREENSHOT, frame, size*w*h);
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
        // Set the assert data
        assert_expr = expression;
        assert_line = line;
        assert_file = file;
        
        // Intentionally cause a null pointer exception
        *((char*)(NULL)) = 0;
    }

    
    #if OVERWRITE_OSPRINT
        /*==============================
            debug_osSyncPrintf_implementation
            Overwrites osSyncPrintf calls with this one
        ==============================*/
    
        static void* debug_osSyncPrintf_implementation(void *str, const char *buf, size_t n)
        {
            debug_printf(buf);
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

            debug_printf("%s\t\t0x%08x ", name, value);
            debug_printf("<");
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
            @param 
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
                    debug_printf("pc\t\t0x%08x\n", context->pc);
                    if (assert_file == NULL)
                        debug_printreg(context->cause, "cause", causeDesc);
                    else
                        debug_printf("cause\t\tAssertion failed in file '%s', line %d.\n", assert_file, assert_line);
                    debug_printreg(context->sr, "sr", srDesc);
                    debug_printf("badvaddr\t0x%08x\n\n", context->badvaddr);
                    
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
#endif