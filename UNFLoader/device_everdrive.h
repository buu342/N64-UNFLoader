#ifndef __DEVICE_EVERDRIVE_HEADER
#define __DEVICE_EVERDRIVE_HEADER

    #include "device.h"
    #include <stdbool.h>


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_everdrive(FTDIDevice* cart, uint32_t index);
    DeviceError device_open_everdrive(FTDIDevice* cart);
    /*
    void        device_sendrom_everdrive(FTDIDevice* cart, FILE *file, uint32_t size);
    bool        device_testdebug_everdrive(FTDIDevice* cart);
    void        device_senddata_everdrive(FTDIDevice* cart, int datatype, char *data, uint32_t size);
    */
    DeviceError device_close_everdrive(FTDIDevice* cart);

#endif