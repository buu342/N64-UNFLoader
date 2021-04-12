/*======================================================================*/
/*		NuSYS							*/
/*		nuboot.c						*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*									*/
/*----------------------------------------------------------------------*/    
/* Ver 1.0	97/10/9		Created by Kensaku Ohki(SLANP)		*/
/*======================================================================*/
#include <nusys.h>

static OSThread	IdleThread;		/* Idle thread */
static OSThread	MainThread;		/* main thread */



/*****************************************************************************/
/* The stack for the boot becomes reuseable when the first thread activates. */
/* Thus, it has been reused for the main thread. 			     */
/*****************************************************************************/
u64		nuMainStack[NU_MAIN_STACK_SIZE/sizeof(u64)];/* boot/main thread stack */
static u64	IdleStack[NU_IDLE_STACK_SIZE/sizeof(u64)];/* Idle thread stack */

void (*nuIdleFunc)(void);		/* Idle loop callback function */
					   
/*--------------------------------------*/
/* Static Function			    */
/*--------------------------------------*/
static void idle(void *arg);		/* idle function */

/*--------------------------------------*/
/* Extern Function			    */
/*--------------------------------------*/
extern void mainproc(void *arg);		/* game main function */


/*----------------------------------------------------------------------*/
/*	Initialize and activate NuSYS      				*/
/*	IN:	The pointer for the main function 			*/
/*	RET:	None 							*/
/*----------------------------------------------------------------------*/
void nuBoot(void)
{

    __osInitialize_common();	/* Initialize N64OS   */
    
    /* Create and execute the Idle thread  */
    osCreateThread(&IdleThread,NU_IDLE_THREAD_ID, idle, 0,
		   (IdleStack + NU_IDLE_STACK_SIZE/8), 10);
    osStartThread(&IdleThread);

}


/*----------------------------------------------------------------------*/
/*	IDLE								*/
/*	IN:	None 							*/
/*	RET:	None 							*/
/*----------------------------------------------------------------------*/
static void idle(void *arg)
{
    /* Initialize the CALLBACK function  */
    nuIdleFunc = NULL;

    nuPiInit();
    
    /* Activate the scheduler 						*/
    /* Setting of VI is NTSC/ANTIALIASING/NON-INTERLACE/16bitPixel*/
    /* Possible to change by osViSetMode. 				*/
    nuScCreateScheduler(OS_VI_NTSC_LAN1, 1);

    
    /* Setting of the VI interface 					*/
    /*    Specify OS_VI_DITHER_FILTER_ON and 			*/
    /*    use the DITHER filter (Default is OFF).		*/
    osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON
			   | OS_VI_GAMMA_OFF
			   | OS_VI_GAMMA_DITHER_OFF
			   | OS_VI_DIVOT_ON);

    /* Create th main thread for the application.  */
    osCreateThread(&MainThread, NU_MAIN_THREAD_ID, mainproc, (void*)NULL,
		   (nuMainStack + NU_MAIN_STACK_SIZE/sizeof(u64)), NU_MAIN_THREAD_PRI);
    osStartThread(&MainThread);

    /* Lower priority of the IDLE thread and give the process to the main thread  */
    osSetThreadPri(&IdleThread, NU_IDLE_THREAD_PRI);

    /* Idle loop */
    while(1){
	if((volatile)nuIdleFunc != NULL){
	    /* Execute the idle function  */
	    (*nuIdleFunc)();
	}
    }
}

