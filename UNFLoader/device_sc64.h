#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_sc64(CartDevice* cart);
    DeviceError device_open_sc64(CartDevice* cart);
    uint32_t    device_maxromsize_sc64();
    bool        device_shouldpadrom_sc64();
    bool        device_explicitcic_sc64(byte* bootcode);
    /*
    void        device_sendrom_sc64(CartDevice* cart, byte* rom, uint32_t size);
    bool        device_testdebug_sc64(CartDevice* cart);
    void        device_senddata_sc64(CartDevice* cart, int datatype, char* data, uint32_t size);
    */
    DeviceError device_close_sc64(CartDevice* cart);

#endif
