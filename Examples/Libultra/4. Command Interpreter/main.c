/***************************************************************
                            main.c
                               
Program entrypoint.
***************************************************************/

#include <nusys.h>
#include <stdlib.h> // For atoi()
#include "config.h"
#include "texture.h"
#include "stages.h"
#include "debug.h"


/*********************************
        Function Prototypes
*********************************/

static void callback_prenmi();
static void callback_vsync(int tasksleft);

// Command functions
char* command_ping();
char* command_rotate();
char* command_color();
char* command_texture();
void  command_button();


/*==============================
    mainproc
    Initializes the game
==============================*/

void mainproc(void)
{
    // Start by selecting the proper television
    if (TV_TYPE == PAL)
    {
        osViSetMode(&osViModeTable[OS_VI_FPAL_LAN1]);
        osViSetYScale(0.833);
    }
    else if (TV_TYPE == MPAL)
        osViSetMode(&osViModeTable[OS_VI_MPAL_LAN1]);

    // Initialize and activate the graphics thread and Graphics Task Manager.
    nuGfxInit();
    
    // Initialize the debug library
    debug_initialize();
    debug_addcommand("ping", "Pong!", command_ping);
    debug_addcommand("rotate", "Toggles square rotation", command_rotate);
    debug_addcommand("color R G B", "Changes the background color", command_color);
    debug_addcommand("texture @file@", "Changes the square texture", command_texture);
    debug_64drivebutton(command_button, TRUE);
    debug_printcommands();
        
    // Initialize stage 0
    stage00_init();
        
    // Set callback functions for reset and graphics
    nuGfxFuncSet((NUGfxFunc)callback_vsync);
    
    // Turn on the screen and loop forever to keep the idle thread busy
    nuGfxDisplayOn();
    while(1)
        ;
}


/*==============================
    callback_vsync
    Code that runs on on the graphics
    thread
    @param The number of tasks left to execute
==============================*/

static void callback_vsync(int tasksleft)
{
    // Update the stage, then draw it when the RDP is ready
    stage00_update();
    if (tasksleft < 1)
        stage00_draw();
}


/*==============================
    callback_prenmi
    Code that runs when the reset button
    is pressed. Required to prevent crashes
==============================*/

static void callback_prenmi()
{
    nuGfxDisplayOff();
    osViSetYScale(1);
}


/*==============================
    command_ping
    Reply with pong!
==============================*/

char* command_ping()
{
    return "Pong!";
}


/*==============================
    command_rotate
    Toggles the square rotation
==============================*/

char* command_rotate()
{
    global_rotate = !global_rotate;
    return NULL;
}


/*==============================
    command_color
    Changes the background color
==============================*/

char* command_color()
{
    char colr[4];
    char colg[4];
    char colb[4];
    char size;
    
    // Get the red value
    size = debug_sizecommand();
    if (size == 0 || size > 3)
        return "Bad argument RED";
    debug_parsecommand(colr);
    colr[size] = '\0';
    
    // Get the green value
    size = debug_sizecommand();
    if (size == 0 || size > 3)
        return "Bad argument GREEN";
    debug_parsecommand(colg);
    colg[size] = '\0';
    
    // Get the blue value
    size = debug_sizecommand();
    if (size == 0 || size > 3)
        return "Bad argument BLUE";
    debug_parsecommand(colb);
    colb[size] = '\0';
    
    // Set the colors
    global_red = atoi(colr);
    global_green = atoi(colg);
    global_blue = atoi(colb);
    return NULL;
}


/*==============================
    command_color
    Changes the square texture
==============================*/

char* command_texture()
{
    int size;
    
    // Get the size of the incoming file
    size = debug_sizecommand();
    if (size == 0)
        return "No file provided";
    if (size > 4096)
        return "Texture larger than TMEM";
    
    // Replace the texture with the incoming data
    debug_parsecommand(global_texture);
    return NULL;
}


/*==============================
    command_button
    Prints when you pressed the 64Drive button
==============================*/

void command_button()
{
    debug_printf("You have pressed the 64Drive's button.\n");
}