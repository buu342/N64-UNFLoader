/***************************************************************
                           device.cpp

Passes flashcart communication to more specific functions
***************************************************************/

#include "device.h"
#include "Include/ftd2xx.h"
#pragma comment(lib, "Include/FTD2XX.lib")

static char*    local_rompath  = NULL;
static bool     local_isz64    = false;
static CartType local_carttype = CART_NONE;
static CICType  local_cictype  = CIC_NONE;
static SaveType local_savetype = SAVE_NONE;


/*==============================
    device_setrom
    Sets the path of the ROM to load
    @param The path to the ROM
==============================*/

void device_setrom(char* path)
{
    local_rompath = path;
}


/*==============================
    device_setcart
    Forces a flashcart
    @param The cart to set to
==============================*/

void device_setcart(CartType cart)
{
    local_carttype = cart;
}


/*==============================
    device_setcic
    Forces a CIC
    @param The cic to set
==============================*/

void device_setcic(CICType cic)
{
    local_cictype = cic;
}


/*==============================
    device_setsave
    Forces a save type
    @param The savetype to set
==============================*/

void device_setsave(SaveType save)
{
    local_savetype = save;
}