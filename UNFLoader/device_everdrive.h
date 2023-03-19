#ifndef __DEVICE_EVERDRIVE_HEADER
#define __DEVICE_EVERDRIVE_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_everdrive(CartDevice* cart);
    DeviceError device_open_everdrive(CartDevice* cart);
    uint32_t    device_maxromsize_everdrive();
    bool        device_shouldpadrom_everdrive();
    bool        device_explicitcic_everdrive(byte* bootcode);
    /*
    void        device_sendrom_everdrive(CartDevice* cart, byte* rom, uint32_t size);
    bool        device_testdebug_everdrive(CartDevice* cart);
    void        device_senddata_everdrive(CartDevice* cart, int datatype, char *data, uint32_t size);
    */
    DeviceError device_close_everdrive(CartDevice* cart);

#endif