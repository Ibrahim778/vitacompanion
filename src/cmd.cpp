#include <kernel.h>
#include <paf.h>
#include <net.h>

#include "cmd.h"
#include "cmd_definitions.h"
#include "common.h"
#include "print.h"

SceUID      CMDThreadID = SCE_UID_INVALID_UID;
SceNetId    CMDSocketID = SCE_UID_INVALID_UID;

void CMDHandle(char *cmd, size_t size, paf::string &res_msg)
{
    print("Reached CMDHandle!\n");
    paf::vector <paf::string>args;
    
    char *prevBegin = cmd;
    for(char *c = cmd; *c != '\0'; c++) // Split with ' '
    {
        if(*c == ' ' || *c == '\n')
        {
            args.push_back(paf::string(prevBegin, c - prevBegin));
            prevBegin = c + 1;
        }
    }

    // Now allow for arguments encapsulated with "
    int quoted = -1;
    for(int i = 0; i < args.size(); i++)
    {
        if(args[i].empty())
            continue;
        
        if(args[i].c_str()[0] == '"' && quoted == -1) // Begins with '"'
            quoted = i;
        
        if(args[i].c_str()[args[i].size() - 1] == '"' && quoted != -1) // Ends with '"'
        {
            paf::string newArg = "";
            
            int added = 0;
            for(int x = quoted; x <= i; x++) // Loop through all quoted strings
            {
                newArg += " ";
                newArg += args[x];
                added = added + 1;
            }
            
            paf::string finalArg = paf::string(&newArg.c_str()[2], newArg.length() - 3); // Remove '"'
            args.erase(args.cbegin() + quoted, args.cbegin() + i + 1);
            args.insert(args.cbegin() + quoted, finalArg);
            
            quoted = -1;
            i -= (added - 1);
        }
    }
    print("Handled cmd\n");
    for(auto& a : args)
    {
        print("%s\n", a.c_str());
    }
    const CMDDefinition *def = GetCMD(args[0].c_str());
    print("Got def: %p\n", def);
    if(!def)
    {
        res_msg = paf::common::FormatString("[Error] Command %s not found!\n", args[0].c_str());
        return;
    }
    print("%s %p\n", def->name, def->executor);
    if(args.size() - 1 < def->minArgCount)
    {
        res_msg = paf::common::FormatString("[Error] Command %s requires at least %d arguments!\n", def->name, def->minArgCount);
        return;
    }

    def->executor(args, res_msg);
}

int CMDThread(SceSize args, ScePVoid argp)
{
    print("[Start] CMDThread\n");
    SceNetSockaddrIn loaderAddr;

    CMDSocketID = sceNetSocket("vitacompanion_cmd_sock", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);

    loaderAddr.sin_family = SCE_NET_AF_INET;
#ifndef __INTELLISENSE__ //Some windows headers ruin stuff here
    loaderAddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
#endif
    loaderAddr.sin_port = sceNetHtons(CMD_PORT);

    int bindR = sceNetBind(CMDSocketID, &loaderAddr, sizeof(loaderAddr));

    int listenR = sceNetListen(CMDSocketID, 128);
    
    while (PluginUp && NetConnected)
    {
        SceNetSockaddrIn clientaddr;
        int client_sockfd;
        unsigned int addrlen = sizeof(clientaddr);

        client_sockfd = sceNetAccept(CMDSocketID, &clientaddr, &addrlen);
        if (client_sockfd >= 0)
        {
            char cmd[100] = { 0 };
            sceNetSend(client_sockfd, "> ", 2, 0);
            int size = sceNetRecv(client_sockfd, cmd, sizeof(cmd), 0);

            paf::string res_msg;
            print("Recieved: %s\n", cmd);
            if (size >= 0)
                CMDHandle(cmd, size, res_msg);
                
            sceNetSend(client_sockfd, res_msg.c_str(), res_msg.length(), 0);
            sceNetSocketClose(client_sockfd);
        }
        else
        {
            print("client_socketfd: 0x%X\n", client_sockfd);
            break;
        }
    }

    print("[Stop] CMDThread\n");
    return sceKernelExitDeleteThread(0);
}

void CMDStart()
{
    int ret = SCE_OK;
    ret = sceKernelCreateThread("vitacompanion_cmd_thread", CMDThread, 0x40, SCE_KERNEL_128KiB, 0, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_NULL);
    if(ret == SCE_UID_INVALID_UID)
    {
        print("[Error] sceKernelCreateThread(\"vitacompanion_cmd_thread\") -> 0x%X\n", CMDThreadID);
        return;
    }
    CMDThreadID = ret;
    ret = sceKernelStartThread(CMDThreadID, 0, SCE_NULL);
    if(ret != SCE_OK)
    {
        print("[Error] sceKernelStartThread(CMDThreadID) -> 0x%X\n", ret);
        return;
    }
}

void CMDStop()
{
    sceNetSocketClose(CMDSocketID);
    sceKernelWaitThreadEnd(CMDThreadID, SCE_NULL, SCE_NULL);
}