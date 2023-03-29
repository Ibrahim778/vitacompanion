#include <kernel.h>
#include <taihen.h>
#include <udcd.h>

#include "usb.h"
#include "print.h"

// Functions for USB Mass storage (obtained in module_start)
int (*sceUsbstorVStorStart)(unsigned int type) = SCE_NULL;
int (*sceUsbstorVStorStop)(void) = SCE_NULL;
int (*_sceUsbstorVStorStart)(unsigned int type) = SCE_NULL;
int (*_sceUsbstorVStorStop)(void) = SCE_NULL;
int (*_sceUsbstorVStorSetImgFilePath)(const char *path) = SCE_NULL;
int (*_sceUsbstorVStorSetDeviceInfo)(const char *name, const char *version) = SCE_NULL;

SceBool usbActivated = SCE_FALSE;

SceUID hooks[3] = {SCE_UID_INVALID_UID};

tai_hook_ref_t sceIoOpenRef;
tai_hook_ref_t sceIoReadRef;

char device[0x20] = {0};

SceInt32 g_USBResult = SCE_OK;

int first = 0;
SceUID sceIoOpenPatched(const char *file, int flags, SceMode mode) 
{
    first = 1;

    SceUID fd = TAI_CONTINUE(SceUID, sceIoOpenRef, file, flags, mode);

    if (fd == 0x800F090D)
        return TAI_CONTINUE(SceUID, sceIoOpenRef, file, flags & ~SCE_O_WRONLY, mode);

    return fd;
}

int sceIoReadPatched(SceUID fd, void *data, SceSize size) 
{
    int res = TAI_CONTINUE(int, sceIoReadRef, fd, data, size);

    if (first) 
    {
        first = 0;

        // Manipulate boot sector to support exFAT
        if (memcmp((int)data + 0x3, "EXFAT", 5) == 0) 
        {
            // Sector size
            *(uint16_t *)((int)data + 0xB) = 1 << *(uint8_t *)((int)data + 0x6C);

            // Volume size
            *(uint32_t *)((int)data + 0x20) = *(uint32_t *)((int)data + 0x48);
        }
    }

    return res;
}

SceBool VCKernelGetUSBStatus()
{
    return usbActivated;
}

SceInt32 PatchUSB()
{
    tai_module_info_t vstorInfo;
    vstorInfo.size = sizeof(vstorInfo);

    if(taiGetModuleInfoForKernel(KERNEL_PID, "SceUsbstorVStorDriver", &vstorInfo) < 0)
        return -1;

    // Remove image path limitation
  	char zero[0x6E];
 	memset(zero, 0, 0x6E); // can probably use DmacMemset here but I'll leave it as is since 0x6E bytes is small enough
  	hooks[0] = taiInjectDataForKernel(KERNEL_PID, vstorInfo.modid, 0, 0x1738, zero, 0x6E);
    if(hooks[0] < 0)
        return hooks[0];

    // Add patches to support exFAT
  	hooks[1] = taiHookFunctionImportForKernel(KERNEL_PID, &sceIoOpenRef, "SceUsbstorVStorDriver", 0x40FD29C7, 0x75192972, sceIoOpenPatched);
    if(hooks[1] < 0)
        return hooks[1];

    hooks[2] = taiHookFunctionImportForKernel(KERNEL_PID, &sceIoReadRef, "SceUsbstorVStorDriver", 0x40FD29C7, 0xE17EFC03, sceIoReadPatched);
  	if(hooks[2] < 0)
        return hooks[2];

    sceDebugPrintf("exfat and image path limit patches = %x %x %x\n", hooks[0], hooks[1], hooks[2]);
    
    return SCE_OK;
}

SceInt32 ReleaseUSBPatches()
{
    if(hooks[2] > 0)
        taiHookReleaseForKernel(hooks[2], sceIoReadRef);
    
    if(hooks[1] > 0)
        taiHookReleaseForKernel(hooks[1], sceIoOpenRef);
    
    if(hooks[0] > 0)
        taiInjectReleaseForKernel(hooks[0]);
    print("Released USB Patches!\n");
    return SCE_OK; // This should never fail lol
}

SceInt32 USBStartThread(SceSize args, void *argp)
{
    PatchUSB();
    sceUdcdStopCurrentInternal(2);

    SceInt32 ret = SCE_OK;

    ret = _sceUsbstorVStorSetDeviceInfo("\"PS Vita\" MC", "1.00");
    if(ret < 0)
    {
        print("sdi 0x%X\n", ret);
        goto EXIT;
    }
    
    ret = _sceUsbstorVStorSetImgFilePath(device);
    if(ret < 0)
    {
        print("sifp 0x%X\n", ret);
        goto EXIT;
    }

    sceUsbstorVStorStart(0); // This is just in case we have PSMLogUSB so it can trigger the hooks
    ret = _sceUsbstorVStorStart(0); // TYPE_FAT
    if(ret < 0)
    {
        print("st 0x%X\n", ret);
        goto EXIT;
    }

    usbActivated = SCE_TRUE;

EXIT:
    if(ret < 0)
        ReleaseUSBPatches();
    g_USBResult = ret;
    return sceKernelExitDeleteThread(0);
}

SceInt32 VCKernelStartUSBMass(uintptr_t pDevice)
{
    if(usbActivated)
        return -2;

    int ret = SCE_OK;

    unsigned state;
    ENTER_SYSCALL(state);


    sceKernelStrncpyFromUser(device, pDevice, sizeof(device));

    SceUID thrID = ret = sceKernelCreateThread("VCKernelUsbStartThread", USBStartThread, 64, 0x10000, 0, 0, 0); // For some fucking reason we need a god damn thread to make the vstor function work or we get an error 
    if(thrID < 0)
        goto EXIT;
    

    ret = sceKernelStartThread(thrID, 0, SCE_NULL);
    if(ret < 0)
        goto EXIT;

    ret = sceKernelWaitThreadEnd(thrID, SCE_NULL, SCE_NULL);
    if(ret < 0)
        goto EXIT;

    ret = g_USBResult;
EXIT:
    EXIT_SYSCALL(state);
    return ret;
}

SceInt32 VCKernelStopUSBMass()
{
    if(!usbActivated)
        return -2;

    unsigned state;
    ENTER_SYSCALL(state);

    sceUsbstorVStorStop();
    int ret = _sceUsbstorVStorStop();
    if(ret < 0 && ret != 0x80244115)
        goto EXIT;

    ret = ReleaseUSBPatches();
    if(ret < 0) 
        goto EXIT;

    usbActivated = SCE_FALSE;
    ret = sceIoSync(device, 0);

EXIT:
    EXIT_SYSCALL(state);
    return ret;
}