#include "device_usb.h"
#include "Include/ftd2xx.h"

#ifdef _WIN64
    #pragma comment(lib, "Include/FTD2XX_x64.lib")
#else
    #pragma comment(lib, "Include/FTD2XX.lib")
#endif 
