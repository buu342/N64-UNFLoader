#ifndef __DEVICE_EVERDRIVE7_HEADER
#define __DEVICE_EVERDRIVE7_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_everdrive7(ftdi_context_t* cart, int index);
    void device_open_everdrive7(ftdi_context_t* cart);
    void device_sendrom_everdrive7(ftdi_context_t* cart, FILE *file, u32 size);
    void device_close_everdrive7(ftdi_context_t* cart);

#endif