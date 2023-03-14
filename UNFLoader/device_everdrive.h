#ifndef __DEVICE_EVERDRIVE_HEADER
#define __DEVICE_EVERDRIVE_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_everdrive(FTDIDevice* cart, uint32_t index);
    DeviceError device_open_everdrive(FTDIDevice* cart);
    uint32_t    device_maxromsize_everdrive();
    bool        device_shouldpadrom_everdrive();
    bool        device_explicitcic_everdrive(byte* bootcode);
    /*
    void        device_sendrom_everdrive(FTDIDevice* cart, byte* rom, uint32_t size);
    bool        device_testdebug_everdrive(FTDIDevice* cart);
    void        device_senddata_everdrive(FTDIDevice* cart, int datatype, char *data, uint32_t size);
    */
    DeviceError device_close_everdrive(FTDIDevice* cart);

#endif