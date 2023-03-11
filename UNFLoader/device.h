#ifndef __DEVICE_HEADER
#define __DEVICE_HEADER

    #include "Include/ftd2xx.h"
    #include <stdint.h>


    /*********************************
               Enumerations
    *********************************/

    typedef enum {
        CART_NONE      = 0,
        CART_64DRIVE1  = 1,
        CART_64DRIVE2  = 2,
        CART_EVERDRIVE = 3,
        CART_SC64      = 4
    } CartType;

    typedef enum {
        CIC_NONE = -1,
        CIC_6101 = 0,
        CIC_6102 = 1,
        CIC_7101 = 2,
        CIC_7102 = 3,
        CIC_X103 = 4,
        CIC_X105 = 5,
        CIC_X106 = 6,
        CIC_5101 = 7
    } CICType;

    typedef enum {
        SAVE_NONE         = 0,
        SAVE_EEPROM4K     = 1,
        SAVE_EEPROM16K    = 2,
        SAVE_SRAM256      = 3,
        SAVE_FLASHRAM     = 4,
        SAVE_SRAM768      = 5,
        SAVE_FLASHRAMPKMN = 6,
    } SaveType;

    typedef enum {
        DEVICEERR_OK = 0,
        DEVICEERR_NOTCART,
        DEVICEERR_USBBUSY,
        DEVICEERR_NODEVICES,
        DEVICEERR_CARTFINDFAIL,
        DEVICEERR_CANTOPEN,
        DEVICEERR_RESETFAIL,
        DEVICEERR_TIMEOUTSETFAIL,
        DEVICEERR_PURGEFAIL,
        DEVICEERR_READFAIL,
        DEVICEERR_WRITEFAIL,
        DEVICEERR_CLOSEFAIL,
    } DeviceError;


    /*********************************
                 Typedefs
    *********************************/

    typedef struct {
        CartType                  carttype;
        CICType                   cictype;
        CICType                   savetype;
        uint32_t                  device_count;
        uint32_t                  device_index;
        FT_DEVICE_LIST_INFO_NODE* device_info;
        FT_STATUS                 status;
        FT_HANDLE                 handle;
        uint32_t                  synchronous; // For 64Drive
        uint32_t                  bytes_written;
        uint32_t                  bytes_read;
    } FTDIDevice;


    /*********************************
            Function Prototypes
    *********************************/

    DeviceError device_find();

	void device_setrom(char* path);
    void device_setcart(CartType cart);
    void device_setcic(CICType cic);
    void device_setsave(SaveType save);
    char* device_getrom();

#endif