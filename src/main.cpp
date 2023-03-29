#include <kernel.h>
#include <taihen.h>
#include <libsysmodule.h>
#include <quickmenureborn/qm_reborn.h>

#include "main.h"
#include "ftpvita.h"
#include "netmgr.h"
#include "print.h"
#include "widgets.h"

extern "C" {
    int zipInitPsp2(void);
}

SceBool PluginUp = SCE_FALSE;

SceUID sceSysmoduleLoadModuleInternalID = SCE_UID_INVALID_UID;
tai_hook_ref_t sceSysmoduleLoadModuleInternalRef;
int sceSysmoduleLoadModuleInternalWithArgPatched(SceUInt32 id, SceSize args, void *argp, void *unk) 
{
    int res = TAI_NEXT(sceSysmoduleLoadModuleInternalWithArgPatched, sceSysmoduleLoadModuleInternalRef, id, args, argp, unk);

    if (res >= 0 && id == SCE_SYSMODULE_INTERNAL_PAF) 
    {
        zipInitPsp2(); // Requires ScePaf
    }

    return res;
}

int module_start(SceSize args, ScePVoid argp)
{
    tai_module_info_t info;
    info.size = sizeof(info);
    if(taiGetModuleInfo("ScePaf", &info) >= 0)
    {
        zipInitPsp2(); // Requires ScePaf
    }
    else
    {
        sceSysmoduleLoadModuleInternalID = taiHookFunctionImport(&sceSysmoduleLoadModuleInternalRef, (const char *)TAI_MAIN_MODULE, 0x03FCF19D, 0xC3C26339, sceSysmoduleLoadModuleInternalWithArgPatched);
    }

    PluginUp = (QuickMenuRebornGetCheckboxValue("VC_checkbox") == QMR_CONFIG_MGR_ERROR_NOT_EXIST ? SCE_FALSE : QuickMenuRebornGetCheckboxValue("VC_checkbox"));
    if(PluginUp)
        StartNet();

    DisplayWidgets();
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, ScePVoid argp)
{
    PluginUp = SCE_FALSE;
    StopNet();
    return SCE_KERNEL_STOP_SUCCESS;
}