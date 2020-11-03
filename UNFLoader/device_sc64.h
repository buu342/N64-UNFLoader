#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"


    /*********************************
                  Macros
    *********************************/

    #define    DEV_CMD_IDENTIFY             'I'
    #define    DEV_CMD_READ                 'R'
    #define    DEV_CMD_WRITE                'W'
    #define    DEV_CMD_DEBUG_MODE           'D'
    #define    DEV_CMD_DEBUG_WRITE          'F'

    #define    DEV_BANK_ROM                 1

    #define    DEV_IDENTIFIER               "S64a"

    #define    DEV_MAX_RW_BYTES             (4 * 1024 * 1024)
    #define    DEV_BYTES_TO_LENGTH(x)       (((x) / 4) - 1)

    #define    DEV_RW_PARAM_1(offset)       ((offset) & 0x0FFFFFFF)
    #define    DEV_RW_PARAM_2(bank, len)    (((bank & 0xFF) << 24) | (DEV_BYTES_TO_LENGTH(len) & 0xFFFFF))


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_sc64(ftdi_context_t* cart, int index);
    void device_open_sc64(ftdi_context_t* cart);
    void device_sendrom_sc64(ftdi_context_t* cart, FILE *file, u32 size);
    void device_senddata_sc64(ftdi_context_t* cart, int datatype, char* data, u32 size);
    void device_close_sc64(ftdi_context_t* cart);

#endif
