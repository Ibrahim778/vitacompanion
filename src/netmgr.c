#include <kernel.h>
#include <net.h>
#include <libnetctl.h>

#include "ftpvita.h"
#include "netmgr.h"
#include "common.h"
#include "cmd.h"

SceBool NetConnected = SCE_FALSE; 
SceBool IsUp         = SCE_FALSE; 

SceUID  NetThreadID  = SCE_UID_INVALID_UID;
int     NetCtlCBID   = SCE_UID_INVALID_UID;

int NetworkThread(unsigned int args, void* argp);

SceVoid StartNet()
{
    PluginUp = SCE_TRUE;
    NetThreadID = sceKernelCreateThread("vitacompanion_net_thread", NetworkThread, 0x40, SCE_KERNEL_64KiB, 0, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, NULL);
    sceKernelStartThread(NetThreadID, 0, SCE_NULL);
}

SceVoid StopNet()
{
    PluginUp = SCE_FALSE;
    sceKernelWaitThreadEnd(NetThreadID, SCE_NULL, SCE_NULL);
}

void OnNetworkConnected()
{
    char vita_ip[16];
    unsigned short int vita_port;

    sceClibPrintf("Network Connected\n");

    ftpvita_set_file_buf_size(512 * 1024);

    if (ftpvita_init(vita_ip, &vita_port) >= 0)
    {
        ftpvita_add_device("ux0:");
        ftpvita_add_device("ur0:");
        ftpvita_add_device("uma0:");
        ftpvita_add_device("imc0:");
        ftpvita_add_device("xmc0:");
        ftpvita_add_device("grw0:");
        CMDStart();
    }
}

void NetCtlCB(int eventID, void* arg)
{
    sceClibPrintf("netctl cb: %d\n", eventID);

    // TODO sceNetCtlInetGetResult

    if (eventID == SCE_NET_CTL_EVENT_TYPE_DISCONNECTED || eventID == SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED)
    {
        NetConnected = SCE_FALSE;
        if(IsUp)
        {
            ftpvita_fini();
            CMDStop();
            IsUp = SCE_FALSE;
        }
    }
    else if (eventID == SCE_NET_CTL_EVENT_TYPE_IPOBTAINED)
    { /* IP obtained */
        NetConnected = SCE_TRUE;
        if(!IsUp)
        {
            OnNetworkConnected();
            IsUp = SCE_TRUE;
        }
    }

}

int NetworkThread(unsigned int args, void* argp)
{
    int ret;

    sceClibPrintf("Net thread started!\n");

    sceKernelDelayThread(3 * 1000 * 1000);

    ret = sceNetCtlInit();
    sceClibPrintf("sceNetCtlInit: 0x%08X\n", ret);

    // If already connected to Wifi
    int state;
    sceNetCtlInetGetState(&state);
    sceClibPrintf("Net state: %d\n", state);
    sceClibPrintf("sceNetCtlInetGetState: 0x%08X\n", state);
    NetCtlCB(state, NULL); //Manually fire callback to start threads

    // FIXME: Add a mutex here, network status might change right before the callback is registered

    ret = sceNetCtlInetRegisterCallback(NetCtlCB, NULL, &NetCtlCBID);
    sceClibPrintf("sceNetCtlInetRegisterCallback: 0x%08X\n", ret);

    while (PluginUp)
    {
        sceNetCtlCheckCallback();
        sceKernelDelayThread(1000 * 1000);
    }
    NetCtlCB(SCE_NET_CTL_EVENT_TYPE_DISCONNECTED, NULL); //Manually fire callback to start threads
    
    return sceKernelExitDeleteThread(0);
}
