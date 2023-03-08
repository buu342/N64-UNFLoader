#include "main.h"
#include "helper.h"
#ifdef LINUX
    #include <sys/time.h>
#endif


/*==============================
    time_miliseconds
    Retrieves the current system
    time in miliseconds
    @return The time in miliseconds
==============================*/

uint64_t time_miliseconds()
{
    #ifndef LINUX
        SYSTEMTIME st;
        GetSystemTime(&st);
        return (uint64_t)st.wMilliseconds;
    #else
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
    #endif
}