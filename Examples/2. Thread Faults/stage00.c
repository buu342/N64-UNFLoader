/***************************************************************
                           stage00.c
                               
Handles the first level of the game.
***************************************************************/

#include <nusys.h>
#include "config.h"
#include "debug.h"

// Sprites
#include "spr_arena.h"
#include "spr_chuck.h"

#define DIRECTION_RIGHT 1
#define DIRECTION_LEFT  -1

typedef struct {
    const void* sprite;
    int xoffset;
    int yoffset;
    int width;
    int height;
    int masks;
    int duration;
} frame;

typedef struct {
    const frame* frames[10];
    int numframes;
} animation;

typedef struct {
    int x;
    int y;
    s8 direction;
    int state;
    int frame;
    int subframe;
    OSTime frametime;
    animation* anim;
} chuck;

//{
// Chuck wrist frames
frame frm_chuck_wrist_01 = {spr_chuck_wrist1, -1, -1, 32, 43, 5, 3};
frame frm_chuck_wrist_02 = {spr_chuck_wrist2, -1, -1, 32, 43, 5, 5};
frame frm_chuck_wrist_03 = {spr_chuck_wrist3, -2, -1, 32, 43, 5, 3};
frame frm_chuck_wrist_04 = {spr_chuck_wrist4, -3, -1, 32, 43, 5, 5};

// Chuck idle frames
frame frm_chuck_idle_01 = {spr_chuck_idle1, 0,  0, 32, 42, 5, 1};
frame frm_chuck_idle_02 = {spr_chuck_idle2, 0, -1, 32, 43, 5, 1};
frame frm_chuck_idle_03 = {spr_chuck_idle3, 0, -2, 32, 45, 5, 2};
frame frm_chuck_idle_04 = {spr_chuck_idle4, 0, -1, 32, 43, 5, 1};
frame frm_chuck_idle_05 = {spr_chuck_idle5, 0,  0, 32, 42, 5, 1};
frame frm_chuck_idle_06 = {spr_chuck_idle6, 0,  0, 32, 42, 5, 1};

// Chuck kick frames
frame frm_chuck_kick_01 = {spr_chuck_kick1, 16,  0, 64, 41, 6, 1};
frame frm_chuck_kick_02 = {spr_chuck_kick2, -8,  1, 32, 40, 5, 1};
frame frm_chuck_kick_03 = {spr_chuck_kick3, -10, 2, 64, 38, 6, 1};
frame frm_chuck_kick_04 = {spr_chuck_kick4, 1,   0, 64, 42, 6, 1};
frame frm_chuck_kick_05 = {spr_chuck_kick5, 4,  -2, 64, 45, 6, 1};

// Chuck wrist animation
animation anm_chuck_wrist = {
    {
        &frm_chuck_wrist_01, 
        &frm_chuck_wrist_02,
        &frm_chuck_wrist_03, 
        &frm_chuck_wrist_04
    }, 4
};

// Chuck idle animation
animation anm_chuck_idle = {
    {
        &frm_chuck_idle_01, 
        &frm_chuck_idle_02, 
        &frm_chuck_idle_03, 
        &frm_chuck_idle_04, 
        &frm_chuck_idle_05, 
        &frm_chuck_idle_06
    }, 6
};

// Chuck kick animation
animation anm_chuck_kick = {
    {
        &frm_chuck_kick_01, 
        &frm_chuck_kick_02, 
        &frm_chuck_kick_03, 
        &frm_chuck_kick_04,
        &frm_chuck_kick_05
    }, 6
};

static chuck obj_chuck1;
static chuck obj_chuck2;
//}
/*==============================
    stage00_init
    Initialize the stage
==============================*/

void stage00_init(void)
{   
    const frame* spr;
    
    obj_chuck1.x = 111;
    obj_chuck1.y = 191;
    obj_chuck1.direction = DIRECTION_RIGHT;
    obj_chuck1.state = 0;
    obj_chuck1.frame = 0;
    obj_chuck1.subframe = 0;
    obj_chuck1.anim = &anm_chuck_wrist;
    obj_chuck1.frametime = osGetTime() + OS_USEC_TO_CYCLES(100000);
    
    obj_chuck2.x = 208;
    obj_chuck2.y = 191;
    obj_chuck2.direction = DIRECTION_LEFT;
    obj_chuck2.state = 0;
    obj_chuck2.frame = 0;
    obj_chuck2.subframe = 0;
    obj_chuck2.anim = &anm_chuck_wrist;
    obj_chuck2.frametime = osGetTime() + OS_USEC_TO_CYCLES(100000);
    
    spr = obj_chuck1.anim->frames[obj_chuck1.frame];
}


/*==============================
    stage00_update
    Update variables and objects
==============================*/

void stage00_update(void)
{
    int i;
    chuck* chucks[2] = {&obj_chuck1, &obj_chuck2};
    
    // Update chucks
    for (i=0; i<(sizeof(chucks)/sizeof(chucks[0])); i++)
    {        
        // Do logic every 0.1 seconds
        if (chucks[i]->frametime < osGetTime())
        {
            // Restart the timer
            chucks[i]->frametime = osGetTime() + OS_USEC_TO_CYCLES(100000);
            
            // Advance the sprite subframes
            chucks[i]->subframe++;
            /**debug_printf("Advanced to subframe %d in %d\n", chucks[i]->subframe, i);*/
            
            // Advance the sprite frames
            if (chucks[i]->subframe == chucks[i]->anim->frames[chucks[i]->frame]->duration)
            {
                chucks[i]->frame++;
                chucks[i]->subframe = 0;
                /**debug_printf("Advanced to frame %d in %d\n", chucks[i]->frame, i);*/
                
                // Advance the animation
                if (chucks[i]->frame == chucks[i]->anim->numframes)
                {
                    if (chucks[i]->anim == &anm_chuck_wrist)
                        chucks[i]->anim = &anm_chuck_idle;
                    else if (chucks[i]->anim == &anm_chuck_idle && obj_chuck1.state == 2)
                        chucks[i]->anim = &anm_chuck_kick;
                    else
                        obj_chuck1.state++;
                    chucks[i]->frame = 0;
                    /**debug_printf("Advanced to next sprite in %d\n", i);*/
                }
            }
            
            // Move chuck
            if (chucks[i]->anim == &anm_chuck_idle)
                if (chucks[i]->frame == 1 || chucks[i]->frame == 2 || chucks[i]->frame == 3)
                    chucks[i]->x = chucks[i]->x+chucks[i]->direction*3;
        }
    }
}


/*==============================
    stage00_draw
    Draw the stage
==============================*/

void stage00_draw(void)
{
    int i, j;
    chuck* chucks[2] = {&obj_chuck1, &obj_chuck2};

    // Assign our glist pointer to our glist array for ease of access
    glistp = glist;

    // Initialize the RCP and clear the framebuffer + zbuffer
    rcp_init(glistp);
    gDPSetCycleType(glistp++, G_CYC_FILL);
    gDPSetDepthImage(glistp++, nuGfxZBuffer);
    gDPSetColorImage(glistp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, nuGfxZBuffer);
    gDPSetFillColor(glistp++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));
    gDPFillRectangle(glistp++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);
    gDPPipeSync(glistp++);
    gDPSetColorImage(glistp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, osVirtualToPhysical(nuGfxCfb_ptr));
    gDPSetFillColor(glistp++, (GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1)));
    gDPFillRectangle(glistp++, 0, 0, SCREEN_WD-1, SCREEN_HT-1);
    gDPPipeSync(glistp++);
    
    // Draw the arena
    gDPSetCycleType(glistp++, G_CYC_1CYCLE);
    gDPSetRenderMode(glistp++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF);
    gDPSetDepthSource(glistp++, G_ZS_PRIM);
    gDPSetTexturePersp(glistp++, G_TP_NONE);
    gDPSetCombineMode(glistp++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    gDPSetPrimColor(glistp++,0,0,255,255,255,255);
    gDPSetTextureLUT(glistp++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(glistp++, spr_arena_tlut);
    for(i=0; i<240; i++)
    {
        gDPLoadMultiTile(glistp++, spr_arena, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 320, 240, 0, i, 320-1, i, 0, G_TX_WRAP, G_TX_WRAP, 0, 0, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(glistp++, 0 << 2, i << 2, 320 << 2, i+1 << 2,  G_TX_RENDERTILE,  0 << 5, i << 5,  1 << 10, 1 << 10);
    }
    gDPPipeSync(glistp++);
    
    // Draw Chucks
    gDPLoadTLUT_pal256(glistp++, spr_chuck_tlut);
    for (i=0; i<(sizeof(chucks)/sizeof(chucks[0])); i++)
    {
        const frame* spr = chucks[i]->anim->frames[chucks[i]->frame];
        for(j=0; j<spr->height; j++)
        {
            if (chucks[i]->direction == DIRECTION_RIGHT)
            {
                gDPLoadMultiTile(glistp++, spr->sprite, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, spr->width, spr->height, 0, j, spr->width-1, j, 0, G_TX_WRAP, G_TX_WRAP, 0, 0, G_TX_NOLOD, G_TX_NOLOD);
                gSPTextureRectangle(glistp++, (chucks[i]->x+spr->xoffset-spr->width/2) << 2, (chucks[i]->y+spr->yoffset-spr->height/2+j) << 2, (chucks[i]->x+spr->xoffset+spr->width/2) << 2, (chucks[i]->y+spr->yoffset-spr->height/2+j+1) << 2,  G_TX_RENDERTILE, 0 << 5, j << 5,  1 << 10, 1 << 10);
            }
            else
            {
                gDPLoadMultiTile(glistp++, spr->sprite, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, spr->width, spr->height, 0, j, spr->width-1, j, 0, G_TX_MIRROR, G_TX_WRAP, spr->masks, 0, G_TX_NOLOD, G_TX_NOLOD);
                gSPTextureRectangle(glistp++, (chucks[i]->x-spr->xoffset-spr->width/2) << 2, (chucks[i]->y+spr->yoffset-spr->height/2+j) << 2, (chucks[i]->x-spr->xoffset+spr->width/2) << 2, (chucks[i]->y+spr->yoffset-spr->height/2+j+1) << 2,  G_TX_RENDERTILE,  spr->width << 5, j << 5,  1 << 10, 1 << 10);
            }
        }
    }
    
    // Syncronize the RCP and CPU and specify that our display list has ended
    gDPFullSync(glistp++);
    gSPEndDisplayList(glistp++);
    
    // Ensure we haven't gone over the display list size and start the graphics task
    debug_assert((glistp-glist) < GLIST_LENGTH);
    nuGfxTaskStart(glist, (s32)(glistp - glist) * sizeof(Gfx), NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
}