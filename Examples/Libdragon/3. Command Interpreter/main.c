/***************************************************************
                             main.c
                               
Handles the boot process of the ROM.
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libdragon.h>
#include "debug.h"


/*********************************
             Globals
*********************************/

int  global_red   = 255; 
int  global_green = 255; 
int  global_blue  = 255; 
char global_move  = 0;

sprite_t* spr_texture;
char*     usb_buffer = NULL;


/*********************************
        Function Prototypes
*********************************/

char* command_ping();
char* command_move();
char* command_color();
char* command_texture();
void  command_button();


/*==============================
    main
    Initializes the game
==============================*/

int main(void)
{
    int x = 64;

    // Initialize libdragon
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    timer_init();

    // Initialize the debug library
    debug_initialize();
    debug_addcommand("ping", "Pong!", command_ping);
    debug_addcommand("move", "Toggles square movement", command_move);
    debug_addcommand("color R G B", "Changes the background color", command_color);
    debug_addcommand("texture @file@", "Changes the square texture", command_texture);
    debug_64drivebutton(command_button, true);
    debug_printcommands();

    // Load the default sprite from ROM
    spr_texture = sprite_load("rom:/texture.sprite");

    // Main loop
    while(1)
    {
        surface_t *disp = display_get();

        // Check if anything was in the USB (unnecessary if auto-polling is enabled)
        #if !AUTOPOLL_ENABLED
            debug_pollcommands();
        #endif

        // Handle square movement
        if (global_move)
            x = (x+4)%(320-32);

        // Draw the background and the sprite
        rdpq_attach(disp, NULL);
        rdpq_clear(RGBA16(global_red, global_green, global_blue, 255));
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_alphacompare(1);
        rdpq_sprite_blit(spr_texture, x, 64, NULL);

        // Render the display
        rdpq_detach_show();
    }
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
    command_move
    Toggles the square movement
==============================*/

char* command_move()
{
    global_move = !global_move;
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
    int size;
    
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
    command_texture
    Changes the square texture
==============================*/

char* command_texture()
{
    int size;
    
    // Get the size of the incoming file
    size = debug_sizecommand();
    if (size == 0)
        return "No file provided";

    // Free the old sprite
    if (usb_buffer != NULL)
        free(usb_buffer);
    sprite_free(spr_texture);

    // Allocate memory for the buffer
    usb_buffer = (char*)malloc(size);
    
    // Replace the texture with the incoming data
    debug_parsecommand(usb_buffer);
    spr_texture = sprite_load_buf(usb_buffer, size);
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