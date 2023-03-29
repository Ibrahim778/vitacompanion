#ifndef _PRINT_H_
#define _PRINT_H_

#include <kernel.h>

#ifdef _DEBUG
    #define print(...) sceClibPrintf(__VA_ARGS__)
#else
    #define print(x) {(void)SCE_NULL;}
#endif

#endif