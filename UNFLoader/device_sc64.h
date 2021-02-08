#ifndef __DEVICE_SC64_HEADER
#define __DEVICE_SC64_HEADER

    #include "device.h"


    /*********************************
                  Macros
    *********************************/

    #define    DEV_CMD_IDENTIFY             'I'
    #define    DEV_CMD_WRITE                'W'
    #define    DEV_CMD_READ                 'R'
    #define    DEV_CMD_DEBUG_WRITE          'D'

    #define    DEV_BANK_SDRAM               1
    #define    DEV_BANK_CART                2
    #define    DEV_BANK_EEPROM              3
    #define    DEV_BANK_FLASHRAM            4

    #define    DEV_IDENTIFIER               "S64a"

    #define    DEV_SDRAM_SIZE               (64 * 1024 * 1024)

    #define    DEV_MAX_RW_BYTES             (4 * 1024 * 1024)
    #define    DEV_BYTES_TO_LENGTH(x)       (((x) / 4) - 1)

    #define    DEV_RW_PARAM_1(offset)       ((offset) & 0x3FFFFFF)
    #define    DEV_RW_PARAM_2(bank, len)    (((bank & 0xF) << 24) | (DEV_BYTES_TO_LENGTH(len) & 0xFFFFF))
    #define    DEV_RW_PARAMS(o, b, l)       2, DEV_RW_PARAM_1(o), DEV_RW_PARAM_2(b, l)
    #define    DEV_DEBUG_WRITE_PARAM_1(len) (DEV_BYTES_TO_LENGTH(len) & 0xFFFFF)

    #define    DEV_OFFSET_SCR               (0 << 2)
    #define    DEV_OFFSET_BOOT              (1 << 2)
    #define    DEV_OFFSET_GPIO              (3 << 2)
    #define    DEV_OFFSET_DDIPL_ADDR        (7 << 2)
    #define    DEV_OFFSET_SRAM_ADDR         (8 << 2)

    #define    DEV_SCR_FLASHRAM_ENABLE      (1 << 9)
    #define    DEV_SCR_SRAM_768K_MODE       (1 << 8)
    #define    DEV_SCR_SRAM_ENABLE          (1 << 7)
    #define    DEV_SCR_EEPROM_16K_MODE      (1 << 4)
    #define    DEV_SCR_EEPROM_ENABLE        (1 << 3)
    #define    DEV_SCR_DDIPL_ENABLE         (1 << 2)

    #define    DEV_BOOT_SKIP_MENU           (1 << 15)
    #define    DEV_BOOT_CIC_SEED_OVERRIDE   (1 << 14)
    #define    DEV_BOOT_TV_TYPE_OVERRIDE    (1 << 13)
    #define    DEV_BOOT_DDIPL_OVERRIDE      (1 << 12)
    #define    DEV_BOOT_TV_TYPE(t)          (((t) & 0x3) << 10)
    #define    DEV_BOOT_CIC_SEED(s)         (((s) & 0x1FF) << 0)
    #define    DEV_BOOT(t, s)               (DEV_BOOT_CIC_SEED_OVERRIDE | DEV_BOOT_TV_TYPE_OVERRIDE | DEV_BOOT_TV_TYPE(t) | DEV_BOOT_CIC_SEED(s))

    #define    DEV_GPIO_RESET               (1 << 0)

    #define    DEV_TV_TYPE_PAL              0
    #define    DEV_TV_TYPE_NTSC             1
    #define    DEV_TV_TYPE_MPAL             2


    /*********************************
            Function Prototypes
    *********************************/

    bool device_test_sc64(ftdi_context_t* cart, int index);
    void device_open_sc64(ftdi_context_t* cart);
    void device_sendrom_sc64(ftdi_context_t* cart, FILE *file, u32 size);
    void device_senddata_sc64(ftdi_context_t* cart, int datatype, char* data, u32 size);
    void device_close_sc64(ftdi_context_t* cart);

#endif
