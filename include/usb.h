#ifndef _USB_H_
#define _USB_H_

#include <kernel.h>

typedef struct USBDevice 
{
    const char *first;
    const char *second;
    const char *name;
    const char *id;
} USBDevice; 

const USBDevice usbDevices[] = {
    {
        .first = "sdstor0:xmc-lp-ign-userext",
        .second = "sdstor0:int-lp-ign-userext",
        .name = "Memory Card",
        .id = "memcard",
    },
    {
        .first = "sdstor0:gcd-lp-ign-gamero",
        .second = SCE_NULL,
        .name = "Game Card",
        .id = "gamecard",
    },
    {
        .first = "sdstor0:gcd-lp-ign-entire",
        .second = SCE_NULL,
        .name = "sd2vita",
        .id = "sd2vita",
    },
    {
        .first = "sdstor0:uma-pp-act-a",
        .second = "sdstor0:uma-lp-act-entire",
        .name = "psvsd",
        .id = "psvsd",
    }
};

SceVoid MountUSBDeviceFromUser();
const USBDevice *GetUSBDeviceFromID(const char *id);

SceUID MountUSBDevice(const USBDevice *path);
SceUID UnmountUSBDevice();

#endif