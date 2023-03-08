#include "main.h"
#include "helper.h"
#include <sys/time.h>

uint64_t time_miliseconds()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}