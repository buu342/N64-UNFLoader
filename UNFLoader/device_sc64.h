#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"


    /*********************************
                  Macros
    *********************************/

    #define    DEV_CMD_CONFIG               'C'
    #define    DEV_CMD_WRITE                'W'
    #define    DEV_CMD_DEBUG_WRITE          'D'

    #define    DEV_CONFIG_SAVE_TYPE         4
    #define    DEV_CONFIG_CIC_SEED          5
    #define    DEV_CONFIG_TV_TYPE           6
    #define    DEV_CONFIG_SKIP_BOOTLOADER   9


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_sc64(ftdi_context_t* cart, int index);
    void device_open_sc64(ftdi_context_t* cart);
    void device_sendrom_sc64(ftdi_context_t* cart, FILE *file, u32 size);
    bool device_testdebug_sc64(ftdi_context_t* cart);
    void device_senddata_sc64(ftdi_context_t* cart, int datatype, char* data, u32 size);
    void device_close_sc64(ftdi_context_t* cart);

#endif
