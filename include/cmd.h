#ifndef _CMD_H_
#define _CMD_H_

#include <kernel.h>

SCE_CDECL_BEGIN

#define CMD_PORT 1338
#define ARG_MAX (20)

SceVoid CMDStart();
SceVoid CMDStop();

SCE_CDECL_END

#endif