#ifndef __DEVICE_HEADER
#define __DEVICE_HEADER

    #include "Include/ftd2xx.h"
    #include <stdint.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <stdbool.h>


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
        CIC_5101 = 7,
        CIC_8303 = 8
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
        DEVICEERR_RESETPORTFAIL,
        DEVICEERR_TIMEOUTSETFAIL,
        DEVICEERR_PURGEFAIL,
        DEVICEERR_READFAIL,
        DEVICEERR_WRITEFAIL,
        DEVICEERR_WRITEZERO,
        DEVICEERR_CLOSEFAIL,
        DEVICEERR_BITMODEFAIL_RESET,
        DEVICEERR_BITMODEFAIL_SYNCFIFO,
        DEVICEERR_SETDTRFAIL,
        DEVICEERR_CLEARDTRFAIL,
        DEVICEERR_GETMODEMSTATUSFAIL,
        DEVICEERR_TXREPLYMISMATCH,
        DEVICEERR_READCOMPSIGFAIL,
        DEVICEERR_NOCOMPSIG,
        DEVICEERR_READPACKSIZEFAIL,
        DEVICEERR_BADPACKSIZE,
        DEVICEERR_MALLOCFAIL,
        DEVICEERR_UPLOADCANCELLED,
        DEVICEERR_TIMEOUT,
        DEVICEERR_64D_BADCMP,
        DEVICEERR_64D_8303USB,
        DEVICEERR_SC64_CTRLRESETFAIL,
        DEVICEERR_SC64_CTRLRELEASEFAIL,
        DEVICEERR_SC64_FIRMWARECHECKFAIL,
        DEVICEERR_SC64_FIRMWAREUNKNOWN,
    } DeviceError;


    /*********************************
                 Typedefs
    *********************************/

    typedef struct {
        CartType                  carttype;
        CICType                   cictype;
        SaveType                  savetype;
        DWORD                     device_count;
        uint32_t                  device_index;
        FT_DEVICE_LIST_INFO_NODE* device_info;
        FT_STATUS                 status;
        FT_HANDLE                 handle;
        bool                      synchronous; // For 64Drive
        DWORD                     bytes_written;
        DWORD                     bytes_read;
    } FTDIDevice;


    /*********************************
            Function Prototypes
    *********************************/

    // Main device functions
    void        device_initialize();
    DeviceError device_find();
    DeviceError device_open();
    uint32_t    device_getmaxromsize();
    bool        device_shouldpadrom();
    bool        device_explicitcic();
    bool        device_isopen();
    bool        device_testdebug();
    DeviceError device_sendrom(FILE* rom, uint32_t filesize);
    DeviceError device_close();

    // Device configuration
	bool     device_setrom(char* path);
    void     device_setcart(CartType cart);
    void     device_setcic(CICType cic);
    void     device_setsave(SaveType save);
    char*    device_getrom();
    CartType device_getcart();
    CICType  device_getcic();

    // Upload related
    void  device_cancelupload();
    bool  device_uploadcancelled();
    void  device_setuploadprogress(float progress);
    float device_getuploadprogress();
    
    // Helper functions
    #define  SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) // From https://graphics.stanford.edu/~seander/bithacks.html#SwappingValuesXOR
    uint32_t swap_endian(uint32_t val);
    uint32_t calc_padsize(uint32_t size);
    uint32_t romhash(uint8_t *buff, uint32_t len);
    CICType  cic_from_hash(uint32_t hash);

#endif