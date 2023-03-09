/***************************************************************
                           device.cpp

Passes flashcart communication to more specific functions
***************************************************************/

#include "Include/ftd2xx.h"

char* local_rompath = NULL;


/*==============================
    device_setrom
    Sets the path of the ROM to load
    @param The path to the ROM
==============================*/

void device_setrom(char* path)
{
    local_rompath = path;
}