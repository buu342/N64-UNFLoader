#ifndef __DEVICE_EVERDRIVE3_HEADER
#define __DEVICE_EVERDRIVE3_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_everdrive3(ftdi_context_t* cart, int index);
    void device_open_everdrive3(ftdi_context_t* cart);
    void device_sendrom_everdrive3(ftdi_context_t* cart, FILE *file, u32 size);
    void device_close_everdrive3(ftdi_context_t* cart);

#endif