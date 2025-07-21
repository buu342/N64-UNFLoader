#ifndef __DEVICE_GOPHER64_HEADER
#define __DEVICE_GOPHER64_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_gopher64(CartDevice* cart);
    DeviceError device_open_gopher64(CartDevice* cart);
    uint32_t    device_maxromsize_gopher64();
    uint32_t    device_rompadding_gopher64(uint32_t romsize);
    bool        device_explicitcic_gopher64(byte* bootcode);
    DeviceError device_sendrom_gopher64(CartDevice* cart, byte* rom, uint32_t size);
    DeviceError device_testdebug_gopher64(CartDevice* cart);
    DeviceError device_senddata_gopher64(CartDevice* cart, USBDataType datatype, byte* data, uint32_t size);
    DeviceError device_receivedata_gopher64(CartDevice* cart, uint32_t* dataheader, byte** buff);
    DeviceError device_close_gopher64(CartDevice* cart);

#endif
