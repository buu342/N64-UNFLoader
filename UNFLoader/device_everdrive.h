#ifndef __DEVICE_EVERDRIVE_HEADER
#define __DEVICE_EVERDRIVE_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_test_everdrive(CartDevice* cart);
    DeviceError device_open_everdrive(CartDevice* cart);
    DeviceError device_sendrom_everdrive(CartDevice* cart, byte* rom, uint32_t size);
    uint32_t    device_maxromsize_everdrive();
    uint32_t    device_rompadding_everdrive(uint32_t romsize);
    bool        device_explicitcic_everdrive(byte* bootcode);
    DeviceError device_testdebug_everdrive(CartDevice* cart);
    DeviceError device_senddata_everdrive(CartDevice* cart, USBDataType datatype, byte* data, uint32_t size);
    DeviceError device_receivedata_everdrive(CartDevice* cart, uint32_t* dataheader, byte** buff);
    DeviceError device_close_everdrive(CartDevice* cart);

#endif