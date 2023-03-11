/***************************************************************
                       device_64drive.cpp

Handles 64Drive HW1 and HW2 USB communication. A lot of the code
here is courtesy of MarshallH's 64Drive USB tool:
http://64drive.retroactive.be/support.php
***************************************************************/

#include "device_64drive.h"
#include <string.h>


/*==============================
    device_test_64drive1
    Checks whether the device passed as an argument is 64Drive HW1
    @param A pointer to the cart context
    @param The index of the cart
    @returns true if the cart is a 64Drive HW1, or false otherwise
==============================*/

bool device_test_64drive1(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "64drive USB device A") == 0 && cart->device_info[index].ID == 0x4036010);
}


/*==============================
    device_test_64drive2
    Checks whether the device passed as an argument is 64Drive HW2
    @param A pointer to the cart context
    @param The index of the cart
    @returns true if the cart is a 64Drive HW2, or false otherwise
==============================*/

bool device_test_64drive2(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "64drive USB device") == 0 && cart->device_info[index].ID == 0x4036014);
}