#include <kernel.h>
#include <paf.h>
#include <quickmenureborn/qm_reborn.h>

#include "netmgr.h"
#include "usb.h"

#define prefix "VC_"

#define separator_id prefix "separator"
#define plane_id prefix "plane"
#define checkbox_id prefix "checkbox"
#define text_id prefix "text"
#define usb_id prefix "usb_button"

#define TEXT "CMD/FTP Servers"

extern "C" {
    SceUID   _vshKernelSearchModuleByName(const char *name, SceUInt64 *buff);
};

extern bool PluginUp;

inline void MakeWidgetWithProperties(const char *refID, const char *parentRefID, QMRWidgetType type, float posX, float posY, float sizeX, float sizeY, float colR, float colG, float colB, float colA, const char *label)
{
    QuickMenuRebornRegisterWidget(refID, parentRefID, type);
    QuickMenuRebornSetWidgetPosition(refID, posX, posY, 0, 0);
    QuickMenuRebornSetWidgetColor(refID, colR, colG, colB, colA);
    QuickMenuRebornSetWidgetSize(refID, sizeX, sizeY, 0, 0);
    if(label != NULL)
        QuickMenuRebornSetWidgetLabel(refID, label);
}

void OnCheckboxPressed(const char *id, int hash, int evtID, void *pUserData)
{
    PluginUp = QuickMenuRebornGetCheckboxValue(checkbox_id);
    
    if(!PluginUp)
        StopNet();
    else
        StartNet();
}

void USBButtonPressed(const char *id, int hash, int evtID, void *pUserData)
{
    QuickMenuRebornCloseMenu();
    MountUSBDeviceFromUser();
}

void DisplayWidgets()
{
    QuickMenuRebornSeparator(separator_id, SCE_SEPARATOR_HEIGHT);
    
    MakeWidgetWithProperties(plane_id, NULL, plane, 0, 0, SCE_PLANE_WIDTH, 50, 0,0,0,0, NULL);
    MakeWidgetWithProperties(text_id, plane_id, text, -275, 0, 215, 40, 1,1,1,1, TEXT);

    MakeWidgetWithProperties(checkbox_id, plane_id, check_box, 350.5, 0, 46, 46, 1,1,1,1, NULL);
    QuickMenuRebornAssignDefaultCheckBoxRecall(checkbox_id);
    QuickMenuRebornAssignDefaultCheckBoxSave(checkbox_id);
    QuickMenuRebornRegisterEventHanlder(checkbox_id, QMR_BUTTON_RELEASE_ID, OnCheckboxPressed, NULL);

    SceUInt64 buff = 0;
    if(_vshKernelSearchModuleByName("VCKernel", &buff) < 0)
        return;
    
    QuickMenuRebornSetWidgetPosition(checkbox_id, -67, 0, 0, 0);
    
    MakeWidgetWithProperties(usb_id, plane_id, button, 208.75, 0, 415, 47, 1,1,1,1, "USB");
    QuickMenuRebornRegisterEventHanlder(usb_id, QMR_BUTTON_RELEASE_ID, USBButtonPressed, SCE_NULL);
}