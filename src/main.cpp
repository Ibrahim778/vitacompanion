#include <kernel.h>
#include <paf.h>
#include <libsysmodule.h>
#include <quickmenureborn/qm_reborn.h>

#include "main.h"
#include "ftpvita.h"
#include "netmgr.h"
#include "print.h"
#include "widgets.h"

bool PluginUp = false;

int module_start(SceSize args, ScePVoid argp)
{
    PluginUp = (QuickMenuRebornGetCheckboxValue("VC_checkbox") == QMR_CONFIG_MGR_ERROR_NOT_EXIST ? false : QuickMenuRebornGetCheckboxValue("VC_checkbox"));
    if(PluginUp)
        StartNet();

    DisplayWidgets();
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, ScePVoid argp)
{
    PluginUp = false;
    StopNet();
    return SCE_KERNEL_STOP_SUCCESS;
}