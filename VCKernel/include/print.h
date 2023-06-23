#ifndef _PRINT_H_
#define _PRINT_H_

#ifdef _DEBUG
    #define print(...) sceDebugPrintf(__VA_ARGS__)
#else
    #define print(...) {(void)SCE_NULL;}
#endif

#endif