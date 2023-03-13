#ifndef __HELPER_HEADER
#define __HELPER_HEADER

    #include "device.h"


    /*********************************
            Function Prototypes
    *********************************/

    // Useful
    void     terminate(const char* reason, ...);
    void     progressbar_draw(const char* text, short color, float percent);
    uint64_t time_miliseconds();
    time_t   file_lastmodtime(const char* path);
    void     handle_deviceerror(DeviceError err);
    
    // Program configuration
    CICType     cic_strtotype(const char* cicstring);
    const char* cic_typetostr(CICType cicenum);
    CartType    cart_strtotype(const char* cartstring);
    const char* cart_typetostr(CartType cartenum);
    SaveType    save_strtotype(const char* savestring);
    const char* save_typetostr(SaveType saveenum);

#endif