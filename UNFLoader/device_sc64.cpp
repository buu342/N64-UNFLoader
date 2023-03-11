/***************************************************************
                       device_sc64.cpp

Handles SC64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection
***************************************************************/

#include "device_sc64.h"
#include <string.h>


/*********************************
              Macros
*********************************/

#define CMD_VERSION_GET             'v'
#define CMD_CONFIG_SET              'C'
#define CMD_MEMORY_WRITE            'M'
#define CMD_DEBUG_WRITE             'U'

#define VERSION_V2                  0x32764353

#define CFG_ID_BOOT_MODE            5
#define CFG_ID_SAVE_TYPE            6

#define BOOT_MODE_ROM               1

#define MEMORY_ADDRESS_SDRAM        0x00000000


/*==============================
    device_test_sc64
    Checks whether the device passed as an argument is SC64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

bool device_test_sc64(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "SC64") == 0 && cart->device_info[index].ID == 0x04036014);
}