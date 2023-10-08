/***************************************************************
                           stage00.c
                               
Handles the first level of the game.
***************************************************************/

#include <nusys.h>
#include "config.h"
#include "helper.h"
#include "texture.h"
#include "debug.h"


/*********************************
        Function Prototypes
*********************************/

void draw_square();


/*********************************
             Globals
*********************************/

// Matricies and vectors
static Mtx projection, modeling;
static u16 normal;
static u16 rotation;

// Square vertex list
static Vtx  square_verts[] = {
    {-10, -10, 0, 0, 0 <<6, 32<<6, 255, 255, 255, 255},
    { 10, -10, 0, 0, 32<<6, 32<<6, 255, 255, 255, 255},
    { 10,  10, 0, 0, 32<<6, 0 <<6, 255, 255, 255, 255},
    {-10,  10, 0, 0, 0 <<6, 0 <<6, 255, 255, 255, 255},
};

// Globals
u8 global_rotate;
u8 global_red; 
u8 global_green; 
u8 global_blue; 


/*==============================
    stage00_init
    Initialize the stage
==============================*/

void stage00_init(void)
{   
    global_rotate = FALSE;
    global_red = 0;
    global_green = 0;
    global_blue = 0;
    
    rotation = 0;
}


/*==============================
    stage00_update
    Update stage variables every frame
==============================*/

void stage00_update(void)
{
    // Poll for incoming USB data (unnecessary if we have auto-polling enabled)
    #if !AUTOPOLL_ENABLED
        debug_pollcommands();
    #endif
    
    // Rotate the square if rotation is enabled
    if (global_rotate)
        rotation = (rotation+1)%360;
}


/*==============================
    stage00_draw
    Draw the stage
==============================*/

void stage00_draw(void)
{
    // Assign our glist pointer to our glist array for ease of access
    glistp = glist;

    // Initialize the RCP and framebuffer
    rcp_init();
    fb_clear(global_red, global_green, global_blue);
    
    // Setup the projection matrix and then draw the square
    guPerspective(&projection, &normal, 45.0, (float)SCREEN_WD / (float)SCREEN_HT, 10.0, 100.0, 1.0);
    draw_square();
    
    // Syncronize the RCP and CPU and specify that our display list has ended
    gDPFullSync(glistp++);
    gSPEndDisplayList(glistp++);
    
    // Ensure the chache lines are valid
    osWritebackDCache(&projection, sizeof(projection));
    osWritebackDCache(&modeling, sizeof(modeling));
    
    // Ensure we haven't gone over the display list size and start the graphics task
    debug_assert((glistp-glist) < GLIST_LENGTH);
    nuGfxTaskStart(glist, (s32)(glistp - glist) * sizeof(Gfx), NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
}


/*==============================
    draw_square
    Draws a textured square
==============================*/

void draw_square()
{
    float fmat1[4][4], fmat2[4][4];
    
    gDPPipeSync(glistp++);

    // Apply some transformations to our square
    guRotateF(fmat1, rotation, 0.0, 1.0, 0.0);
    guTranslateF(fmat2, 0.0, 0.0, -48.0);
    guMtxCatF(fmat1, fmat2, fmat1);
    guMtxF2L(fmat1, &modeling);

    // Apply the projection and modeling matricies
    gSPMatrix(glistp++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(glistp++, &modeling, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPPerspNormalize(glistp++, &normal);

    // Setup the RDP and RSP to render a transulcent texture
    gDPSetCycleType(glistp++, G_CYC_1CYCLE);
    gSPTexture(glistp++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetRenderMode(glistp++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(glistp++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    
    // Setup the texture paramaters and load the texture to TMEM
    gDPSetTexturePersp(glistp++, G_TP_PERSP);
    gDPSetTextureFilter(glistp++, G_TF_POINT);
    gDPSetTextureConvert(glistp++, G_TC_FILT);
    gDPSetTextureLOD(glistp++, G_TL_TILE);
    gDPSetTextureDetail(glistp++, G_TD_CLAMP);
    gDPSetTextureLUT(glistp++, G_TT_NONE);
    gDPLoadTextureBlock(glistp++, global_texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);  /* シフト（ここではシフトせず） */

    // Draw the square
    gSPVertex(glistp++, square_verts, 4, 0);
    gSP2Triangles(glistp++, 0, 1, 2, 0, 0, 2, 3, 0);
}