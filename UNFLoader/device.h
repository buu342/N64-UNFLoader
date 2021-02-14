#ifndef __DEVICE_HEADER
#define __DEVICE_HEADER


    /*********************************
                  Macros
    *********************************/

    #define CART_NONE      0
    #define CART_64DRIVE1  1
    #define CART_64DRIVE2  2
    #define CART_EVERDRIVE 3
    #define CART_SC64      4


    /*********************************
                 Typedefs
    *********************************/

    typedef struct {
        DWORD        devices;
        int          device_index;
        FT_STATUS    status;
        FT_DEVICE_LIST_INFO_NODE *dev_info;
        FT_HANDLE    handle;
        DWORD        synchronous; // For 64Drive
        DWORD        bytes_written;
        DWORD        bytes_read;
        DWORD        carttype;
        DWORD        cictype;
    } ftdi_context_t;
    #ifdef LINUX
        typedef int errno_t;
    #endif


    /*********************************
            Function Prototypes
    *********************************/

    void  device_find(int automode);
    void  device_set_64drive1(ftdi_context_t* cart, int index);
    void  device_set_64drive2(ftdi_context_t* cart, int index);
    void  device_set_everdrive(ftdi_context_t* cart, int index);
    void  device_set_sc64(ftdi_context_t* cart, int index);
    void  device_open();
    void  device_sendrom(char* rompath);
    void  device_senddata(int datatype, char* data, u32 size);
    bool  device_isopen();
    DWORD device_getcarttype();
    void  device_close();

#endif