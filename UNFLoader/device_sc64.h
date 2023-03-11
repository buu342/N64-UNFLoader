#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"
    #include <stdbool.h>


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_sc64(FTDIDevice* cart, uint32_t index);
    /*
    void device_open_sc64(FTDIDevice* cart);
    void device_sendrom_sc64(FTDIDevice* cart, FILE *file, uint32_t size);
    bool device_testdebug_sc64(FTDIDevice* cart);
    void device_senddata_sc64(FTDIDevice* cart, int datatype, char* data, uint32_t size);
    void device_close_sc64(FTDIDevice* cart);
    */

#endif
