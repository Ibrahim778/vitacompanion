#include <kernel.h>
#include <common_dialog.h>
#include <taihen.h>
#include <appmgr.h>
#include <usbstorvstor.h>
#include <mtpif.h>
#include <vshbridge.h>

#include "dialog.h"
#include "usb.h"
#include "print.h"

using namespace paf;

extern "C" {
    int VCKernelStartUSBMass(const char *device);
    int VCKernelStopUSBMass();
}

int checkFileExist(const char *file)
{
    SceUID fd = sceIoOpen(file, SCE_O_RDONLY, 0);
    if (fd < 0)
        return 0;

    sceIoClose(fd);
    return 1;
}

void remountDevice(int id) 
{
    vshIoUmount(id, 0, 0, 0); // Ask nicely
    vshIoUmount(id, 1, 0, 0); // Force
    vshIoMount(id, NULL, 0, 0, 0, 0); //Remount
}

const USBDevice *GetUSBDeviceFromID(const char *id)
{
    for(unsigned short i = 0; i < sizeof(usbDevices) / sizeof(usbDevices[0]); i++)
        if(sce_paf_strcmp(usbDevices[i].id, id) == 0)
            return &usbDevices[i];
    
    return SCE_NULL;
}

int UnmountUSBDevice()
{
    int res = SCE_OK;
    // Stop USB storage
    res = VCKernelStopUSBMass();
    if (res < 0)
        return res;

    // Start MTP driver
    res = sceMtpIfStartDriver(1);
    if (res < 0)
        return res;

    // Remount Memory Card
    remountDevice(0x800);

    // Remount imc0:
    if (paf::LocalFile::Exists("imc0:"))
        remountDevice(0xD00);

    // Remount xmc0:
    if (paf::LocalFile::Exists("xmc0:"))
        remountDevice(0xE00);

    // Remount uma0:
    if (paf::LocalFile::Exists("uma0:"))
        remountDevice(0xF00);
    dialog::Close();
    return 0;
}

void USBDialogCB(dialog::ButtonCode button, void *pUserData)
{
    UnmountUSBDevice();
}

SceUID MountUSBDevice(const USBDevice *pInfo)
{
    int res = SCE_OK;
    const char *dev = SCE_NULL;

    if(checkFileExist(pInfo->first))
        dev = pInfo->first;
    else if(checkFileExist(pInfo->second))
        dev = pInfo->second;
    else 
        return SCE_UID_INVALID_UID;

    print("Device: %s (%s)\n", dev, checkFileExist(dev) ? "Exists" : "Doesn't exist");
    
    sceAppMgrDestroyOtherApp();

    res = sceMtpIfStopDriver(1);
    if(res < 0 && res != 0x8054360C /* Driver not laoded */)
        return res;

    res = VCKernelStartUSBMass(dev);
    dialog::OpenOk(Plugin::Find("topmenu_plugin"), SCE_NULL, L"USB device started\nPress OK to terminate", USBDialogCB);
    return res;
}

void ListButtonCB(int eventID, ui::Handler *handler, ui::Event *evt, void *pUserData)
{
    auto widget = (ui::Widget *)handler;
    const USBDevice *pDevice = (USBDevice *)widget->GetName().GetIDHash();

    dialog::Close();
    int mRes = MountUSBDevice(pDevice);
    print("0x%X %s\n", mRes, pDevice->name);
}

void MountUSBDeviceFromUser()
{
    Plugin *topmenu_plugin = Plugin::Find("topmenu_plugin");
    ui::ScrollView *sv = (ui::ScrollView *)dialog::OpenScrollView(topmenu_plugin, L"Select Device");
    if(!sv)
        return;
    
    auto tex = topmenu_plugin->GetTexture("_common_texture_list_70px");
    
    ui::Box *box = (ui::Box *)sv->FindChild(0x19BC95D);
    
    for (unsigned int i = 0; i < sizeof(usbDevices) / sizeof(usbDevices[0]); i++)
    {
        if (checkFileExist(usbDevices[i].first) || checkFileExist(usbDevices[i].second)) // Apparently paf::LocalFile::Exist fails to find these paths?
        {

            ui::Button *button = (ui::Button *)topmenu_plugin->CreateWidget(box, "button", (SceUInt32)&usbDevices[i], "_common_default_style_button");

            button->SetSize(760, 70, 0, 0);
            button->SetTexture(tex);
            button->SetAdjust(1,1,0);
            
            wstring txt16;
            common::Utf8ToUtf16(usbDevices[i].name, &txt16);
            button->SetString(txt16);

            button->AddEventCallback(ui::Button::CB_BTN_DECIDE, ListButtonCB);
        }
    }
}