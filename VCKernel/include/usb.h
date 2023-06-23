#ifndef _USB_H_
#define _USB_H_

// Functions for USB Mass storage
extern int (*sceUsbstorVStorStart)(unsigned int type);
extern int (*sceUsbstorVStorStop)(void);
extern int (*_sceUsbstorVStorStart)(unsigned int type);
extern int (*_sceUsbstorVStorStop)(void);
extern int (*_sceUsbstorVStorSetImgFilePath)(const char *path);
extern int (*_sceUsbstorVStorSetDeviceInfo)(const char *name, const char *version);

int PatchUSB();
int ReleaseUSBPatches();

#endif
 