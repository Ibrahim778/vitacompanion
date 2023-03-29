#include <kernel.h>
#include <appmgr.h>
#include <taihen.h>

#include <string.h>
#include <stdlib.h>

#include "print.h"
#include "usb.h"

extern SceInt32 VCKernelStopUSBMass();

//Required for sceAppMgrLaunchAppByPath
tai_hook_ref_t QafHookRef;
SceUID         QafHookID = SCE_UID_INVALID_UID;
SceInt32 SceQafMgrForDriver_7B14DC45_Patched()
{
    return 1;
}

tai_hook_ref_t SysrootHookRef;
SceUID         SysrootHookID = SCE_UID_INVALID_UID;
SceInt32 SceSysrootForDriver_421EFC96_Patched()
{
    return 0;
}

// So we can unload modules with the ID's directly from _vshSearchModuleByName (taihen tries to convert these ID's to Kernel IDs or something and fails)
SceUID VCKernelLoadStartKernelModule(uintptr_t path, uintptr_t sRes)
{
    unsigned state;
    ENTER_SYSCALL(state);

    char kPath[SCE_IO_MAX_PATH_LENGTH] = {0};
    sceKernelStrncpyFromUser(kPath, path, sizeof(kPath));

    SceInt32 startRes = -1;
    SceUID res = sceKernelLoadStartModule(kPath, 0, SCE_NULL, 0, SCE_NULL, &startRes);

    sceKernelCopyToUser(sRes, &startRes, sizeof(startRes));

    EXIT_SYSCALL(state);

    return res;
}

SceInt32 VCKernelStopUnloadKernelModule(SceUID modid, uintptr_t res)
{
    unsigned state;
    ENTER_SYSCALL(state);

    SceInt32 sRes = -1;
    SceInt32 ret = sceKernelStopUnloadModule(modid, 0, SCE_NULL, 0, SCE_NULL, &sRes);

    sceKernelCopyToUser(res, &sRes, sizeof(sRes));

    EXIT_SYSCALL(state);
    return ret;
}

SceUID VCKernelLaunchSelfWithArgs(const char *path, const char *argp, size_t args)
{
    unsigned state;
    char   kpath[0x400];
    char   arg[0x100];
    size_t argl = 0;

    ENTER_SYSCALL(state);

    SceAppMgrLaunchParam lParam;
    memset(&lParam, 0, sizeof(lParam));
    lParam.size = sizeof(lParam);
    
    memset(kpath, 0, sizeof(kpath));
    memset(arg, 0, sizeof(arg));
    
    argl = (args < 0x100) ? args : 0x100;
    print("argl = %lu\n", argl);
    sceKernelStrncpyFromUser(kpath, path, 0x400);
    print("path %s\n", kpath);
    if (argp)
        sceKernelCopyFromUser(arg, argp, argl);

    int ret = sceAppMgrLaunchAppByPath(kpath, (argl > 0) ? arg : 0, argl, 0, &lParam, NULL);
    print("launch %s(%s) |=>| ret: 0x%X\n", kpath, arg, ret);
    
    EXIT_SYSCALL(state);
    return ret;
}

SceInt32 module_start(SceSize args, ScePVoid argp)
{
    print("[VCKernel] Started\n");    
    
    // Setup things for sceAppMgrLaunchAppByPath
    SysrootHookID = taiHookFunctionImportForKernel(KERNEL_PID, &SysrootHookRef, "SceAppMgr", 0x2ED7F97A, 0x421EFC96, SceSysrootForDriver_421EFC96_Patched);
    print("SysrootHookID: 0x%X\n", SysrootHookID);
    QafHookID = taiHookFunctionImportForKernel(KERNEL_PID, &QafHookRef, "SceAppMgr", 0x4E29D3B6, 0x7B14DC45, SceQafMgrForDriver_7B14DC45_Patched);
    print("QafHookID: 0x%X\n", QafHookID);

    // Setup things for USB
    tai_module_info_t vstorInfo;
    vstorInfo.size = sizeof(vstorInfo);
    

    if(taiGetModuleInfoForKernel(KERNEL_PID, "SceUsbstorVStorDriver", &vstorInfo) >= 0)
    {
        int ret = module_get_export_func(KERNEL_PID, "SceUsbstorVStorDriver", TAI_ANY_LIBRARY, 0xB606F1AF, &sceUsbstorVStorStart);
        print("(0x%X) sceUsbstorVStorStart: %p\n", ret, sceUsbstorVStorStart);
        ret = module_get_export_func(KERNEL_PID, "SceUsbstorVStorDriver", TAI_ANY_LIBRARY, 0x0FD67059, &sceUsbstorVStorStop);
        print("(0x%X) sceUsbstorVStorStop: %p\n", ret, sceUsbstorVStorStop);
        
        ret = module_get_offset(KERNEL_PID, vstorInfo.modid, 0, 0x1710 | 1, &_sceUsbstorVStorStart);
        print("(0x%X) _sceUsbstorVStorStart: %p\n", ret, _sceUsbstorVStorStart);
        ret = module_get_offset(KERNEL_PID, vstorInfo.modid, 0, 0x1858 | 1, &_sceUsbstorVStorStop);
        print("(0x%X) _sceUsbstorVStorStop: %p\n", ret, _sceUsbstorVStorStop);
        ret = module_get_offset(KERNEL_PID, vstorInfo.modid, 0, 0x16b8 | 1, &_sceUsbstorVStorSetDeviceInfo);
        print("(0x%X) _sceUsbstorVStorSetDeviceInfo: %p\n", ret, _sceUsbstorVStorSetDeviceInfo);
        ret = module_get_offset(KERNEL_PID, vstorInfo.modid, 0, 0x16d8 | 1, &_sceUsbstorVStorSetImgFilePath);
        print("(0x%X) _sceUsbstorVStorSetImgFilePath: %p\n", ret, _sceUsbstorVStorSetImgFilePath);
    }
    else
    {
        print("[VCKernel] Warn: SceUsbstorVStorDriver not loaded\n");
    }

    return SCE_KERNEL_START_SUCCESS;
}

SceInt32 module_stop(SceSize args, ScePVoid argp)
{
    print("[VCKernel] Stopped");
    taiHookReleaseForKernel(SysrootHookID, SysrootHookRef);
    taiHookReleaseForKernel(QafHookID, QafHookRef);

    VCKernelStopUSBMass();
    return SCE_KERNEL_STOP_SUCCESS;
}