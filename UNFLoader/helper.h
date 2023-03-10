#ifndef __HELPER_HEADER
#define __HELPER_HEADER

    #include "device.h"

    void        terminate(const char* reason, ...);
    uint64_t    time_miliseconds();
    CICType     cic_strtotype(char* cicstring);
    const char* cic_typetostr(CICType cicenum);
    CartType    cart_strtotype(char* cartstring);
    const char* cart_typetostr(CartType cartenum);
    SaveType    save_strtotype(char* savestring);
    const char* save_typetostr(SaveType saveenum);

#endif