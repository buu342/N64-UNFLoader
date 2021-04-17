/***************************************************************
                             main.c
                               
Handles the boot process of the ROM.
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libdragon.h>
#include "texture.h"
#include "debug.h"


/*********************************
             Globals
*********************************/

int global_red = 255; 
int global_green = 255; 
int global_blue = 255; 
char global_move = 0;
sprite_t* spr_texture;


/*********************************
        Function Prototypes
*********************************/

char* command_ping();
char* command_move();
char* command_color();
char* command_texture();


/*==============================
    main
    Initializes the game
==============================*/

int main(void)
{
    int x = 64;

    // Initialize libdragon
    init_interrupts();
    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdp_init();

    // Initialize the debug library
    debug_initialize();
    debug_addcommand("ping", "Pong!", command_ping);
    debug_addcommand("move", "Toggles square movement", command_move);
    debug_addcommand("color R G B", "Changes the background color", command_color);
    debug_addcommand("texture @file@", "Changes the square texture", command_texture);
    debug_printcommands();

    // Initialize our sprite
    spr_texture = (sprite_t*) malloc(sizeof(sprite_t)+4096);
    spr_texture->width = 32;
    spr_texture->height = 32;
    spr_texture->bitdepth = 4;
    spr_texture->format = 0;
    spr_texture->hslices = 1;
    spr_texture->vslices = 1;
    for (int i=0; i<4096; i++)
        spr_texture->data[i] = global_texture[i];

    // Main loop
    while(1)
    {
        static display_context_t disp = 0;

        // Grab an available framebuffer
        while(!(disp = display_lock()));

        // Check if anything was in the USB
        debug_pollcommands();

        // Handle square movement
        if (global_move)
            x = (x+4)%(320-32);

        // Draw the background and the sprite
        graphics_fill_screen(disp, (((uint32_t)global_red)<<24)|(((uint32_t)global_green)<<16)|(((uint32_t)global_blue)<<8)|0x000000FF);
        graphics_set_color(0x0, 0xFFFFFFFF);
        graphics_draw_sprite_trans(disp, x, 64, spr_texture);

        // Render the display
        display_show(disp);
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
    if (size > 4096)
        return "Texture larger than TMEM";
    
    // Replace the texture with the incoming data
    debug_parsecommand(global_texture);
    for (int i=0; i<4096; i++)
        spr_texture->data[i] = global_texture[i];
    return NULL;
}