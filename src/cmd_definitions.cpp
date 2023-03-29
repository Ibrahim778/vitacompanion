#include <kernel.h>
#include <paf.h>
#include <appmgr.h>
#include <power.h>
#include <libsysmodule.h>
#include <common_gui_dialog.h>

#include "cmd_definitions.h"
#include "print.h"
#include "taihen.h"
#include "dialog.h"
#include "zip.h"
#include "promote.h"
#include "usb.h"

using namespace paf;

extern "C" {
    SceUID   _vshKernelSearchModuleByName(const char *name, SceUInt64 *buff);
    SceUID   VCKernelLaunchSelfWithArgs(const char *path, const char *argp, size_t args);
    SceUID   VCKernelFindKernelModule(const char *uName);
    SceUID   VCKernelLoadStartKernelModule(const char *path, int *res);
    SceInt32 VCKernelStopUnloadKernelModule(SceUID modid, int *res);
    SceBool  VCKernelGetUSBStatus();
    SceInt32 VCKernelStopUSBMass();
};

SceBool g_InstallJobRunning = SCE_FALSE;

const CMDDefinition *GetCMD(const char *name)
{
    for(int i = 0; i < sizeof(CMDDefinitions) / sizeof(CMDDefinitions[0]); i++)
        if(sce_paf_strcmp(name, CMDDefinitions[i].name) == 0)
            return &CMDDefinitions[i];
    return SCE_NULL;
}

void CMDHelp(vector<string> args, string &res_msg)
{
    if(args.size() == 1)
    {
        unsigned int longestOut = 0;
        for(int i = 0; i < (sizeof(CMDDefinitions) / sizeof(CMDDefinitions[0])); i++)
        {
            int outLen = sce_paf_strlen(CMDDefinitions[i].name);
            if(outLen > longestOut)
                longestOut = outLen;
        }

        for(int i = 0; i < (sizeof(CMDDefinitions) / sizeof(CMDDefinitions[0])); i++)
        {
            paf::string buff;
            common::string_util::setf(buff, "%-*s\t\t%s\n", longestOut, CMDDefinitions[i].name, CMDDefinitions[i].description);
            res_msg += buff;
        }
        return;
    }
    
    const CMDDefinition *cmdInfo = GetCMD(args[1].c_str());
    if(cmdInfo == SCE_NULL)
    {
        common::string_util::setf(res_msg, "[Error] Command %s not found!\n", args[1].c_str());
        return;
    }

    common::string_util::setf(res_msg, "%s\t\t%s\n%s\n", cmdInfo->name, cmdInfo->description, cmdInfo->usage);
}

void CMDDestroy(vector<string> args, string &res_msg)
{
    sceAppMgrDestroyOtherApp();
    res_msg = "Apps Destroyed.\n";
}

void CMDLaunch(vector<string> args, string &res_msg)
{
    if(VCKernelGetUSBStatus())
    {
        res_msg = "[Abort] USB mounted\n";
        return;
    }
    
    if(sce_paf_strstr(args[1].c_str(), "NPXS") != SCE_NULL)
    {
        sceAppMgrLaunchAppByName2ForShell(args[1].c_str(), SCE_NULL, SCE_NULL);
        res_msg = "Launched.\n";
        return;
    }

    string uri;
    common::string_util::setf(uri, "psgm:play?titleid=%s", args[1]);
    
    SceInt32 ret = sceAppMgrLaunchAppByUri(0x20000, uri.c_str());
    if(ret != SCE_OK)
    {
        common::string_util::setf(res_msg, "[Error] Unable to launch app 0x%X (Is the TitleID correct?)\n", ret);
        return;
    }

    res_msg = "Launched.\n";
}

void CMDReboot(vector<string> args, string &res_msg)
{
    scePowerRequestColdReset();
    res_msg = "Rebooting...\n";
}

void CMDScreen(vector<string> args, string &res_msg)
{
    if(args[1] == "on")
    {
        scePowerRequestDisplayOn();
        res_msg = "Turning display on...\n";
    }
    else if(args[1] == "off")
    {
        scePowerRequestDisplayOff();
        res_msg = "Turning display off...\n";
    }
    else
    {
        res_msg = "[Error] Argument 1 should be \"on\" or \"off\"\n";
    }
}

void CMDSelf(vector<string> args, string &res_msg)
{
    SceUID ret = VCKernelLaunchSelfWithArgs(args[1].c_str(), SCE_NULL, 0);
    common::string_util::setf(res_msg, "Result Code: 0x%X\n", ret);
}

void CMDUSB(vector<string> args, string &res_msg)
{
    if(g_InstallJobRunning)
    {
        res_msg = "[Abort] An install job is running\n";
        return;
    }

    SceUInt64 buff = 0;
    if(_vshKernelSearchModuleByName("VitaShellUsbDevice", &buff) > 0)
    {
        res_msg = "[Abort] VitaShell USB is already running\n";
        return;
    }

    if(sce_paf_strncmp(args[1].c_str(), "mount", 5) == 0)
    {
        if(VCKernelGetUSBStatus())
        {
            res_msg = "[Abort] USB is already mounted\n";
            return;
        }
        const USBDevice *dev = GetUSBDeviceFromID(args[2].c_str());
        if(!dev)
        {
            common::string_util::setf(res_msg, "[Error] %s is not a valid ID!\n", args[2].c_str());
            return;
        }
        
        int ret = MountUSBDevice(dev);
        if(ret < 0)
            common::string_util::setf(res_msg, "[Error] Failed to mount %s -> 0x%X\n", dev->name, ret);
        else
            common::string_util::setf(res_msg, "%s Mounted Successfully\n", dev->name);
    }
    else if (sce_paf_strncmp(args[1].c_str(), "unmount", 7) == 0)
    {
        if(!VCKernelGetUSBStatus())
        {
            res_msg = "[Abort] USB is not mounted\n";
            return;
        }

        int ret = UnmountUSBDevice();
        if(ret < 0)
            common::string_util::setf(res_msg, "[Error] Failed to unmount 0x%X\n", ret);
        else res_msg = "Unmounted successfully\n";
    }
}

void CMDTai(vector<string> args, string &res_msg)
{
    if(args[1] == "load")
    {
        if(!LocalFile::Exists(args[2].c_str()))
        {
            res_msg = "[Error] File not found.\n";
            return;
        }
        SceInt32 res = taiLoadStartKernelModule(args[2].c_str(), 0, SCE_NULL, 0);
        common::string_util::setf(res_msg, "Started successfully. UID: 0x%X\n", res);
    }
    else if(args[1] == "unload")
    {
        SceUID moduleID = (SceUID)sce_paf_strtol(args[2].c_str(), SCE_NULL, 16);
        SceInt32 moduleStop = SCE_OK;
        SceInt32 res = taiStopUnloadKernelModule(moduleID, 0, SCE_NULL, 0, SCE_NULL, &moduleStop);
        if(res != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] taiStopUnloadKernelModule(0x%X) -> 0x%X\n", moduleID, res);
            return;
        }

        if(moduleStop != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] module_stop -> 0x%X taiStopUnloadKernelModule(0x%X) -> 0x%X\n", moduleStop, moduleID, res);
            return;
        }

        res_msg = "Module stopped successfully.\n";
    }
    else res_msg = "[Error] Argument 1 should be \"load\", or \"unload\"";
}

void CMDSkprx(vector<string> args, string &res_msg)
{
    if(args[1] == "load")
    {
        if(!LocalFile::Exists(args[2].c_str()))
        {
            res_msg = "[Error] File not found.\n";
            return;
        }
        SceInt32 moduleStart = SCE_OK;
        SceInt32 res = VCKernelLoadStartKernelModule(args[2].c_str(), &moduleStart);
        if(res != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] VCKernelLoadStartKernelModule(%s) -> 0x%X\n", args[2].c_str(), res);
            return;
        }

        if(moduleStart != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] module_start -> 0x%X VCKernelStopUnloadKernelModule(%s) -> 0x%X\n", moduleStart, args[2].c_str(), res);
            return;
        }
        
        common::string_util::setf(res_msg, "Started successfully. UID: 0x%X module_start: 0x%X\n", res, moduleStart);
    }
    else if(args[1] == "unload")
    {
        SceUID moduleID = (SceUID)sce_paf_strtol(args[2].c_str(), SCE_NULL, 16);
        SceInt32 moduleStop = SCE_OK;
        SceInt32 res = VCKernelStopUnloadKernelModule(moduleID, &moduleStop);
        if(res != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] VCKernelStopUnloadKernelModule(0x%X) -> 0x%X\n", moduleID, res);
            return;
        }

        if(moduleStop != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] module_stop -> 0x%X VCKernelStopUnloadKernelModule(0x%X) -> 0x%X\n", moduleStop, moduleID, res);
            return;
        }

        res_msg = "Module stopped successfully.\n";
    }
    else if(args[1] == "find")
    {
        SceUInt64 unk = 0;
        SceUID id = _vshKernelSearchModuleByName(args[2].c_str(), &unk);
        if(id < 0)
        {
            common::string_util::setf(res_msg, "[Error] Could not find module with name \"%s\" -> 0x%X\n", args[2].c_str(), id);
            return;
        }
        common::string_util::setf(res_msg, "Module \"%s\" found with ID: 0x%X\n", args[2].c_str(), id);
    }
    else res_msg = "[Error] Argument 1 should be \"load\",\"find\" or \"unload\"";
}

void CMDSuprx(vector<string> args, string &res_msg)
{
    if(args[1] == "load")
    {
        if(!LocalFile::Exists(args[2].c_str()))
        {
            res_msg = "[Error] File not found.\n";
            return;
        }
        SceInt32 loadRes = SCE_OK;
        SceInt32 res = sceKernelLoadStartModule(args[2].c_str(), 0, SCE_NULL, 0, SCE_NULL, &loadRes);
        common::string_util::setf(res_msg, "Started successfully. UID: 0x%X module_start: 0x%X\n", res, loadRes);
    }
    else if(args[1] == "unload")
    {
        SceUID moduleID = (SceUID)sce_paf_strtol(args[2].c_str(), SCE_NULL, 10);
        SceInt32 moduleStop = SCE_OK;
        SceInt32 res = sceKernelStopUnloadModule(moduleID, 0, SCE_NULL, 0, SCE_NULL, &moduleStop);
        if(res != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] sceKernelStopUnload(0x%X) -> 0x%X\n", moduleID, res);
            return;
        }

        if(moduleStop != SCE_OK)
        {
            common::string_util::setf(res_msg, "[Error] module_stop -> 0x%X sceKernelStopUnload(0x%X) -> 0x%X\n", moduleStop, moduleID, res);
            return;
        }

        res_msg = "Module stopped successfully.\n";
    }
    else res_msg = "[Error] Argument 1 should be \"load\" or \"unload\"";
}

void ExtractCB(SceUInt current, SceUInt total)
{
    SceFloat32 progPercent = ((SceFloat32)current / (SceFloat32)total) * 80.0f;
    ui::ProgressBar *prog = (ui::ProgressBar *)sce::CommonGuiDialog::Dialog::GetWidget(dialog::Current(), sce::CommonGuiDialog::WidgetType::WidgetType_Progressbar);
    if(prog)
        prog->SetProgress(progPercent, 0, 0);
}

void CMDVPK(vector<string> args, string &res_msg)
{
    if(g_InstallJobRunning)
    {
        res_msg = "[Abort] An install / promote job is already running\n";
        return;
    }

    if(VCKernelGetUSBStatus())
    {
        res_msg = "[Abort] USB is mounted\n";
        return;
    }

    Plugin *topmenu_plugin = Plugin::Find("topmenu_plugin");
    if(!paf::LocalFile::Exists(args[1].c_str()))
    {
        res_msg = "[Error] File not found\n";
        return;
    }

    g_InstallJobRunning = SCE_TRUE;
    dialog::OpenProgress(topmenu_plugin, L"Installing App", L"Extracting");
    paf::Dir::RemoveRecursive(PROM_DIR);

    Zipfile zFile = Zipfile(args[1]);
    SceInt32 result = zFile.Unzip(PROM_DIR, ExtractCB);

    if(result < 0)
    {
        dialog::Close();
        dialog::OpenError(topmenu_plugin, result, L"Error extracting archive");
        common::string_util::setf(res_msg, "[Error] Could not extract archive 0x%X\n", result);
        g_InstallJobRunning = SCE_FALSE;
        return;
    }
    ui::Text *txt = (ui::Text *)sce::CommonGuiDialog::Dialog::GetWidget(dialog::Current(), sce::CommonGuiDialog::WidgetType::WidgetType_TextMessage1);
    txt->SetLabel(&paf::wstring(L"Promoting"));
    //Promote and set progress, then close
    paf::Dir::RemoveRecursive("ux0:temp/new");
    paf::Dir::RemoveRecursive("ux0:appmeta/new");
    paf::Dir::RemoveRecursive("ux0:temp/promote");
    paf::Dir::RemoveRecursive("ux0:temp/game");

    paf::Dir::RemoveRecursive("ur0:temp/new");
    paf::Dir::RemoveRecursive("ur0:appmeta/new");
    paf::Dir::RemoveRecursive("ur0:temp/promote");
    paf::Dir::RemoveRecursive("ur0:temp/game");

    result = promoteApp(PROM_DIR);
    if(result < 0)
    {
        dialog::Close();
        dialog::OpenError(topmenu_plugin, result, L"Error promoting folder");
        common::string_util::setf(res_msg, "[Error] Could not promote folder 0x%X\n", result);
        g_InstallJobRunning = SCE_FALSE;
        return;
    }

    paf::Dir::RemoveRecursive(PROM_DIR);

    paf::Dir::RemoveRecursive("ux0:temp/new");
    paf::Dir::RemoveRecursive("ux0:appmeta/new");
    paf::Dir::RemoveRecursive("ux0:temp/promote");
    paf::Dir::RemoveRecursive("ux0:temp/game");

    paf::Dir::RemoveRecursive("ur0:temp/new");
    paf::Dir::RemoveRecursive("ur0:appmeta/new");
    paf::Dir::RemoveRecursive("ur0:temp/promote");
    paf::Dir::RemoveRecursive("ur0:temp/game");

    ui::ProgressBar *prog = (ui::ProgressBar *)sce::CommonGuiDialog::Dialog::GetWidget(dialog::Current(), sce::CommonGuiDialog::WidgetType::WidgetType_Progressbar);
    prog->SetProgress(100.0f, 0, 0);
    dialog::Close();
    g_InstallJobRunning = SCE_FALSE;
    res_msg = "App installed successfully!\n";
}

void CMDProm(vector<string> args, string &res_msg)
{
    if(g_InstallJobRunning)
    {
        res_msg = "[Abort] An install / promote job is already running\n";
        return;
    }

    if(VCKernelGetUSBStatus())
    {
        res_msg = "[Abort] USB is mounted\n";
        return;
    }

    Plugin *topmenu_plugin = Plugin::Find("topmenu_plugin");
    if(!paf::Dir::IsExist(args[1].c_str()))
    {
        res_msg = "[Error] Folder not found\n";
        return;
    }

    g_InstallJobRunning = SCE_TRUE;
    dialog::OpenPleaseWait(topmenu_plugin, SCE_NULL, L"Promoting Folder...");
    
    paf::Dir::RemoveRecursive("ux0:temp/new");
    paf::Dir::RemoveRecursive("ux0:appmeta/new");
    paf::Dir::RemoveRecursive("ux0:temp/promote");
    paf::Dir::RemoveRecursive("ux0:temp/game");

    paf::Dir::RemoveRecursive("ur0:temp/new");
    paf::Dir::RemoveRecursive("ur0:appmeta/new");
    paf::Dir::RemoveRecursive("ur0:temp/promote");
    paf::Dir::RemoveRecursive("ur0:temp/game");

    SceInt32 result = promoteApp(args[1].c_str());
    if(result < 0)
    {
        dialog::Close();
        dialog::OpenError(topmenu_plugin, result, L"Error promoting folder");
        common::string_util::setf(res_msg, "[Error] Could not promote folder 0x%X\n", result);
        g_InstallJobRunning = SCE_FALSE;
        return;
    }

    paf::Dir::RemoveRecursive(args[1].c_str());

    paf::Dir::RemoveRecursive("ux0:temp/new");
    paf::Dir::RemoveRecursive("ux0:appmeta/new");
    paf::Dir::RemoveRecursive("ux0:temp/promote");
    paf::Dir::RemoveRecursive("ux0:temp/game");

    paf::Dir::RemoveRecursive("ur0:temp/new");
    paf::Dir::RemoveRecursive("ur0:appmeta/new");
    paf::Dir::RemoveRecursive("ur0:temp/promote");
    paf::Dir::RemoveRecursive("ur0:temp/game");

    dialog::Close();
    g_InstallJobRunning = SCE_FALSE;
    res_msg = "App installed successfully!\n";
}

void CMDRename(vector<string> args, string &res_msg)
{
    if(!LocalFile::Exists(args[1].c_str()))
    {
        res_msg = "[Error] File doesn't exist.\n";
        return;
    }
    SceInt32 res = sceIoRename(args[1].c_str(), args[2].c_str());
    if(res != SCE_OK)
    {
        common::string_util::setf(res_msg, "[Error] Couldn't rename 0x%X.\n", res);
        return;
    }
    res_msg = "Done.\n";
}