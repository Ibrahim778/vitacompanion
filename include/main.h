#ifndef _main_H_
#define _main_H_

#include <kernel.h>

extern "C" {
    int module_start(SceSize args, ScePVoid argp);
    int module_stop(SceSize args, ScePVoid argp);
};

#endif