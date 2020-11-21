#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"


    /*********************************
                  Macros
    *********************************/

    #define    DEV_CMD_IDENTIFY             'I'
    #define    DEV_CMD_WRITE                'W'

    #define    DEV_BANK_ROM                 1
    #define    DEV_BANK_CART                2
    #define    DEV_BANK_EEPROM              3

    #define    DEV_IDENTIFIER               "S64a"

    #define    DEV_MAX_RW_BYTES             (4 * 1024 * 1024)
    #define    DEV_BYTES_TO_LENGTH(x)       (((x) / 4) - 1)

    #define    DEV_RW_PARAM_1(offset)       ((offset) & 0x3FFFFFF)
    #define    DEV_RW_PARAM_2(bank, len)    (((bank & 0xF) << 24) | (DEV_BYTES_TO_LENGTH(len) & 0xFFFFF))
    #define    DEV_RW_PARAMS(o, b, l)       2, DEV_RW_PARAM_1(o), DEV_RW_PARAM_2(b, l)

    #define    DEV_OFFSET_SCR               0
    #define    DEV_OFFSET_CIC_TV_TYPE       4

    #define    DEV_SAVE_TYPE_EEPROM_4K      (1 << 3)
    #define    DEV_SAVE_TYPE_EEPROM_16K     ((1 << 4) | DEV_SAVE_TYPE_EEPROM_4K)

    #define    DEV_CIC_UNKNOWN              0
    #define    DEV_CIC_5101                 1
    #define    DEV_CIC_6101_7102            2
    #define    DEV_CIC_6102_7101            3
    #define    DEV_CIC_X103                 4
    #define    DEV_CIC_X105                 5
    #define    DEV_CIC_X106                 6
    #define    DEV_CIC_8303                 7

    #define    DEV_TV_TYPE_PAL              0
    #define    DEV_TV_TYPE_NTSC             1
    #define    DEV_TV_TYPE_MPAL             2
    #define    DEV_TV_TYPE_UNKNOWN          3

    #define    DEV_CIC(cic)                 (cic & 0xF)
    #define    DEV_TV_TYPE(tv)              ((tv & 0x3) << 4)
    #define    DEV_CIC_TV_TYPE(cic, tv)     (DEV_CIC(cic) | DEV_TV_TYPE(tv))


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_sc64(ftdi_context_t* cart, int index);
    void device_open_sc64(ftdi_context_t* cart);
    void device_sendrom_sc64(ftdi_context_t* cart, FILE *file, u32 size);
    void device_senddata_sc64(ftdi_context_t* cart, int datatype, char* data, u32 size);
    void device_close_sc64(ftdi_context_t* cart);

#endif
