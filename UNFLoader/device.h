#ifndef __DEVICE_HEADER
#define __DEVICE_HEADER

    #include <stdint.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <stdbool.h>


    /*********************************
                  Macros
    *********************************/

    #define USBPROTOCOL_LATEST PROTOCOL_VERSION2


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
        DATATYPE_TEXT       = 0x01,
        DATATYPE_RAWBINARY  = 0x02,
        DATATYPE_HEADER     = 0x03,
        DATATYPE_SCREENSHOT = 0x04,
        DATATYPE_HEARTBEAT  = 0x05,
        DATATYPE_RDBPACKET  = 0x06
    } USBDataType;

    typedef enum {
        PROTOCOL_VERSION1   = 0x00, 
        PROTOCOL_VERSION2   = 0x02,
    } ProtocolVer;

    typedef enum {
        DEVICEERR_OK = 0,
        DEVICEERR_NOTCART,
        DEVICEERR_USBBUSY,
        DEVICEERR_NODEVICES,
        DEVICEERR_CARTFINDFAIL,
        DEVICEERR_CANTOPEN,
        DEVICEERR_FILEREADFAIL,
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
        DEVICEERR_POLLFAIL,
        DEVICEERR_64D_BADCMP,
        DEVICEERR_64D_8303USB,
        DEVICEERR_64D_CANTDEBUG,
        DEVICEERR_64D_BADDMA,
        DEVICEERR_64D_DATATOOBIG,
        DEVICEERR_SC64_CMDFAIL,
        DEVICEERR_SC64_COMMFAIL,
        DEVICEERR_SC64_CTRLRELEASEFAIL,
        DEVICEERR_SC64_CTRLRESETFAIL,
        DEVICEERR_SC64_FIRMWARECHECKFAIL,
        DEVICEERR_SC64_FIRMWAREUNSUPPORTED,
    } DeviceError;


    /*********************************
                 Typedefs
    *********************************/

    typedef uint8_t byte;

    typedef struct {
        CartType    carttype;
        CICType     cictype;
        SaveType    savetype;
        ProtocolVer protocol;
        void*       structure;
    } CartDevice;


    /*********************************
            Function Prototypes
    *********************************/

    // Main device functions
    void        device_initialize();
    DeviceError device_find();
    DeviceError device_open();
    uint32_t    device_getmaxromsize();
    uint32_t    device_rompadding(uint32_t romsize);
    bool        device_explicitcic();
    bool        device_isopen();
    DeviceError device_testdebug();
    DeviceError device_sendrom(FILE* rom, uint32_t filesize);
    DeviceError device_senddata(USBDataType datatype, byte* data, uint32_t size);
    DeviceError device_receivedata(uint32_t* dataheader, byte** buff);
    DeviceError device_close();

    // Device configuration
	bool     device_setrom(char* path);
    void     device_setcart(CartType cart);
    void     device_setcic(CICType cic);
    void     device_setsave(SaveType save);
    char*    device_getrom();
    CartType device_getcart();
    CICType  device_getcic();
    SaveType device_getsave();

    // Upload related
    void  device_cancelupload();
    bool  device_uploadcancelled();
    void  device_setuploadprogress(float progress);
    float device_getuploadprogress();

    // Protocol version handling
    void        device_setprotocol(ProtocolVer version);
    ProtocolVer device_getprotocol();
    
    // Helper functions
    #define  SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) // From https://graphics.stanford.edu/~seander/bithacks.html#SwappingValuesXOR
    #define  ALIGN(s, align) (((uint32_t)(s) + ((align)-1)) & ~((align)-1))
    uint32_t swap_endian(uint32_t val);
    uint32_t calc_padsize(uint32_t size);
    uint32_t romhash(byte* buff, uint32_t len);
    CICType  cic_from_hash(uint32_t hash);

#endif