#include <vitasdk.h>
#include "usb.h"
#include <taihen.h>
#include <common/debugScreen.h>

memoryConfig = -1;
char *path;
SceUID id;

#define printf psvDebugScreenPrintf


int checkFileExist(const char *file) {
  SceUID fd = sceIoOpen(file, SCE_O_RDONLY, 0);
  if (fd < 0)
    return 0;

  sceIoClose(fd);
  return 1;
}

int vshIoMount(int id, const char *path, int permission, int a4, int a5, int a6) {
  uint32_t buf[3];
  
  buf[0] = a4;
  buf[1] = a5;
  buf[2] = a6;

  return _vshIoMount(id, path, permission, buf);
}

void remount(int id) {
  vshIoUmount(id, 0, 0, 0);
  vshIoUmount(id, 1, 0, 0);
  vshIoMount(id, NULL, 0, 0, 0, 0);
}

SceUID startUsb(const char *usbDevicePath, const char *imgFilePath, int type) {
  SceUID modid = -1;
  int res;

  // Destroy other apps
  sceAppMgrDestroyOtherApp();

  // Load and start usbdevice module
  res = taiLoadStartKernelModule(usbDevicePath, 0, NULL, 0);
  if (res < 0)
    goto ERROR_LOAD_MODULE;

  modid = res;

  // Stop MTP driver
  res = sceMtpIfStopDriver(1);
  if (res < 0 && res != 0x8054360C)
    goto ERROR_STOP_DRIVER;

  // Set device information
  res = sceUsbstorVStorSetDeviceInfo("\"PS Vita\" MC", "1.00");
  if (res < 0)
    goto ERROR_USBSTOR_VSTOR;

  // Set image file path
  res = sceUsbstorVStorSetImgFilePath(imgFilePath);
  if (res < 0)
    goto ERROR_USBSTOR_VSTOR;

  // Start USB storage
  res = sceUsbstorVStorStart(type);
  if (res < 0)
    goto ERROR_USBSTOR_VSTOR;

  return modid;

ERROR_USBSTOR_VSTOR:
  sceMtpIfStartDriver(1);

ERROR_STOP_DRIVER:
  taiStopUnloadKernelModule(modid, 0, NULL, 0, NULL, NULL);

ERROR_LOAD_MODULE:
  return res;
}

int stopUsb() {

  int res;

  // Stop USB storage
  res = sceUsbstorVStorStop();
  if (res < 0)
    return res;

  // Start MTP driver
  res = sceMtpIfStartDriver(1);
  if (res < 0)
    return res;

  // Stop and unload usbdevice module
  res = taiStopUnloadKernelModule(id, 0, NULL, 0, NULL, NULL);
  if (res < 0)
    return res;

  // Remount Memory Card
  remount(0x800);

  // Remount imc0:
  if (checkFileExist("imc0:"))
    remount(0xD00);

  // Remount xmc0:
  if (checkFileExist("xmc0:"))
    remount(0xE00);

  // Remount uma0:
  if (checkFileExist("uma0:"))
    remount(0xF00);

  return 0;
}

int setPath()
{
  if(memoryConfig == -1)
  {
    return -1;
  }
  if(memoryConfig == 1)
  {
    if (checkFileExist("sdstor0:xmc-lp-ign-userext"))
      path = "sdstor0:xmc-lp-ign-userext";
    else if (checkFileExist("sdstor0:int-lp-ign-userext"))
      path = "sdstor0:int-lp-ign-userext";
    else
      printf("Memory card not found!\n");
  }
  if(memoryConfig == 2)
  {
    if (checkFileExist("sdstor0:gcd-lp-ign-entire"))
    {
      printf("Setting path to sd2vita\n");
      path = "sdstor0:gcd-lp-ign-entire";
    }
    else 
    { 
      printf("SD2VITA not found\n"); 
    }
  }
  return 0;
}

void initUsb()
{
  if(checkFileExist("ux0:data/unityLoader/sd2vita"))
  {
    memoryConfig = 2;
  }
  else if(checkFileExist("ux0:data/unityLoader/OFFICIAL"))
  {
    memoryConfig = 1;
  }

  setPath();

  SceUID usbID = startUsb("ux0:VitaShell/module/usbdevice.skprx", path, SCE_USBSTOR_VSTOR_TYPE_FAT);
  
  id =  usbID;
}