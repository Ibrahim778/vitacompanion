#ifndef PROMOTE_HPP
#define PROMOTE_HPP
#include <kernel.h>

SCE_CDECL_BEGIN

#define PROM_DIR "ux0:data/vc_prom/"

int promoteApp(const char* path);

SCE_CDECL_END

#endif