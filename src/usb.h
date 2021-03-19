#include <vitasdk.h>

int checkFileExist(const char *file);
int vshIoMount(int id, const char *path, int permission, int a4, int a5, int a6);

SceUID startUsb(const char *usbDevicePath, const char *imgFilePath, int type);
int stopUsb();

int setPath();
int memoryConfig;
void initUsb();
