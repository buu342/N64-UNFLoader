/*******************************************************************
                              stacks.c
                               
Initializes all the stacks that will be used by the ROM.
*******************************************************************/

#include <ultra64.h>
#include "osconfig.h"

u64 bootStack[BOOTSTACKSIZE/sizeof(u64)];
u64 idleThreadStack[IDLESTACKSIZE/sizeof(u64)];
u64 mainThreadStack[MAINSTACKSIZE/sizeof(u64)];