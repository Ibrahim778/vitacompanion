#ifndef _CMD_DEFINITIONS_H_
#define _CMD_DEFINITIONS_H_

#include <paf.h>

typedef void CMDExecutor(paf::vector<paf::string> args, paf::string &client_res);

typedef struct 
{
    char        *name;
    char        *description;
    char        *usage;
    size_t       minArgCount;
    CMDExecutor *executor;
} CMDDefinition;

void CMDHelp(paf::vector<paf::string> args, paf::string &client_res);
void CMDDestroy(paf::vector<paf::string> args, paf::string &client_res);
void CMDLaunch(paf::vector<paf::string> args, paf::string &client_res);
void CMDReboot(paf::vector<paf::string> args, paf::string &client_res);
void CMDScreen(paf::vector<paf::string> args, paf::string &client_res);
void CMDSelf(paf::vector<paf::string> args, paf::string &client_res);
void CMDUSB(paf::vector<paf::string> args, paf::string &client_res);
void CMDSkprx(paf::vector<paf::string> args, paf::string &client_res);
void CMDSuprx(paf::vector<paf::string> args, paf::string &client_res);
void CMDTai(paf::vector<paf::string> args, paf::string &res_msg);
void CMDVPK(paf::vector<paf::string> args, paf::string &client_res);
void CMDProm(paf::vector<paf::string> args, paf::string &res_msg);
void CMDRename(paf::vector<paf::string> args, paf::string &client_res);

const CMDDefinition *GetCMD(const char *name);

const CMDDefinition CMDDefinitions[] = 
{
    {
        .name = "help",    
        .description = "Display this help screen", 
        .usage = "Usage:\n\thelp\n\thelp <command>",
        .minArgCount = 0, 
        .executor = CMDHelp
    },
    {
        .name = "destroy", 
        .description = "Destroy all apps",
        .usage = "Usage:\n\tdestroy",       
        .minArgCount = 0, 
        .executor = CMDDestroy
    },
    {
        .name = "launch",  
        .description = "Launch an app with TitleID",
        .usage = "Usage:\n\tlaunch <titleid>\nExample:\n\tlaunch VITASHELL",         
        .minArgCount = 1, 
        .executor = CMDLaunch
    },
    {
        .name = "reboot",  
        .description = "Reboot the device",
        .usage = "Usage:\n\treboot",         
        .minArgCount = 0, 
        .executor = CMDReboot
    },
    {
        .name = "screen",  
        .description = "Turn the screen on or off",
        .usage = "Usage:\n\tscreen <on|off>\nExample:\n\tscreen on",         
        .minArgCount = 1, 
        .executor = CMDScreen
    },
    {
        .name = "self",    
        .description = "Launch a self file from path",
        .usage = "Usage:\n\tself <path>\nExample:\n\tself ux0:app/VITASHELL/eboot.bin",         
        .minArgCount = 1, 
        .executor = CMDSelf
    },
    {
        .name = "usb",
        .description = "Mount / Unmount the USB interface for file transfer",
        .usage = "Usage:\n\tusb <mount|unmount> <sd2vita|memcard|gamecard|psvsd>\nExample:\n\tusb mount sd2vita\n\tusb unmount",
        .minArgCount = 1, 
        .executor = CMDUSB
    },
    {
        .name = "skprx",   
        .description = "Load / Unload / Find a kernel plugin using VCKernel",
        .usage = "Usage:\n\tskprx load <path>\n\tskprx unload <id>\n\tskprx find <name>\nExample:\n\tskprx load ur0:tai/plugin.skprx\n\tskprx unload 400190\n\tskprx find SceAppMgr",         
        .minArgCount = 2, 
        .executor = CMDSkprx
    },
    {
        .name = "suprx",   
        .description = "Load / Unload a shell plugin",
        .usage = "Usage:\n\tsuprx load <path>\n\tsuprx unload <id>\nExample:\n\tsuprx load ur0:tai/plugin.suprx\n\tsuprx unload 400190",
        .minArgCount = 2, 
        .executor = CMDSuprx
    },
    {
        .name = "tai",   
        .description = "Load / Unload a kernel plugin using taihen",
        .usage = "Usage:\n\ttai load <path>\n\ttai unload <id>\nExample:\n\ttai load ur0:tai/plugin.skprx\n\ttai unload 400190\n\t",         
        .minArgCount = 2, 
        .executor = CMDTai
    },
    {
        .name = "vpk",     
        .description = "Install a vpk from a file",
        .usage = "Usage:\n\tvpk <path>\nExample:\n\tvpk ux0:file.vpk",         
        .minArgCount = 1, 
        .executor = CMDVPK
    },
    {
        .name = "prom", 
        .description = "Promote a folder",
        .usage = "Usage:\n\tprom <path>\nExample:\n\tprom ux0:folder",         
        .minArgCount = 1, 
        .executor = CMDProm
    },
    {
        .name = "rename",  
        .description = "Rename a file",
        .usage = "Usage:\n\trename <oldpath> <newpath>\nExample:\n\trename ux0:file.txt ux0:renamed_file.txt",         
        .minArgCount = 2, 
        .executor = CMDRename
    },
};

#endif