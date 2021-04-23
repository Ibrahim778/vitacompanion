#include "cmd_definitions.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>
#include <psp2/vshbridge.h>
#include "Archives.h"
#include <taihen.h>


#define COUNT_OF(arr) (sizeof(arr) / sizeof(arr[0]))

int launchAppFromFileExport(uintptr_t path, uintptr_t cmd, uint32_t cmdlen);

#define printf sceClibPrintf
SceUID usbID;

int promoteApp(const char* path);
void StartSd2Vita();
void StartUsbOffCard();
void StopUsbExp();

int checkFileExist(const char *file)
{
  int fd = sceIoOpen(file, SCE_O_RDONLY, 0);
  if(fd < 0)
    return 0;
  
  sceIoClose(fd);
  return 1;
}

int checkDirExist(const char *dir)
{
  int fd = sceIoDopen(dir);
  if(fd < 0)
    return 0;
  
  sceIoDclose(fd);
  return 1;
}

void cmd_usb(char **arg_list, size_t arg_count, char *res_msg)
{
  if(!strcmp(arg_list[1], "enable"))
  {
    if(!strcmp(arg_list[2], "sd2vita"))
      StartSd2Vita();
    else if(!strcmp(arg_list[2], "official"))
      StartUsbOffCard();
    else
      strcpy(res_msg, "Error type should be either official or sd2vita!\n");
  }
  else if(!strcmp(arg_list[1], "disable"))
  {
    StopUsbExp();
  }
  else
    strcpy(res_msg, "Error should be enable or disable!\n");
}

void cmd_skprx_load(char **arg_list, size_t arg_count, char *res_msg)
{
  if(!checkFileExist(arg_list[1]))
  {
    strcpy(res_msg, "Error plugin not found!\n");
    return;
  }
  else
  {
    int res = taiLoadStartKernelModule("ur0:tai/udcd_uvc.skprx", 0, NULL, 0);
    sprintf(res_msg, "Resulted with code: %x\n", res);
  }
}

void cmd_file_launch(char **arg_list, size_t arg_count, char *res_msg)
{
  printf("launching self...\n");
  int res = launchAppFromFileExport((uintptr_t)arg_list[1], (uintptr_t)"-livearea_off", (uintptr_t)sizeof("-livearea_off"));
  
  printf("ret= 0x%X\n", res);
}

void cmd_vpk_install(char **arg_list, size_t arg_count, char *res_msg)
{
  if(checkFileExist(arg_list[1]))
  {
    Zip *vpk = ZipOpen(arg_list[1]);
    ZipExtract(vpk, NULL, "ux0:temp/pkg");
    ZipClose(vpk);
    printf("This thing exists: %d", checkDirExist("ux0:temp/pkg"));
    int ret = promoteApp("ux0:temp/pkg");
    if(ret < 0)
      sprintf(res_msg, "Install failed with error code: %x\n", ret);
    else strcpy(res_msg, "Success!\n");
  }
  else
  {
    strcpy(res_msg, "Error VPK file not found!\n");
  }
}

void cmd_ext_vpk_install(char **arg, size_t length, char *res_msg)
{
  if(checkDirExist(arg[1]))
  {
    int ret = promoteApp(arg[1]);
    if(ret < 0)
      sprintf(res_msg, "Install failed with error code: %x\n", ret);
    else strcpy(res_msg, "Success!\n");
  }
  else
  {
    sprintf(res_msg, "Error directory %s does not exist!\n",  arg[1]);
  }
}

const cmd_definition cmd_definitions[] = 
{
    {.name = "destroy", .arg_count = 0, .executor = &cmd_destroy},
    {.name = "launch", .arg_count = 1, .executor = &cmd_launch},
    {.name = "reboot", .arg_count = 0, .executor = &cmd_reboot},
    {.name = "screen", .arg_count = 1, .executor = &cmd_screen},
    {.name = "file", .arg_count = 1, .executor = &cmd_file_launch},
    {.name = "usb", .arg_count = 2, .executor = &cmd_usb},
    {.name = "skprx", .arg_count = 1, .executor = &cmd_skprx_load},
    {.name = "vpk", .arg_count = 1, .executor = &cmd_vpk_install},
    {.name = "ext_vpk", .arg_count = 1, .executor = &cmd_ext_vpk_install}
};

const cmd_definition *cmd_get_definition(char *cmd_name) 
{
  for (unsigned int i = 0; i < COUNT_OF(cmd_definitions); i++) 
  {
    if (!strcmp(cmd_name, cmd_definitions[i].name)) 
    {
      return &(cmd_definitions[i]);
    }
  }

  return NULL;
}

void cmd_destroy(char **arg_list, size_t arg_count, char *res_msg) {
  sceAppMgrDestroyOtherApp();
  strcpy(res_msg, "Apps destroyed.\n");
}

void cmd_launch(char **arg_list, size_t arg_count, char *res_msg) {
  char uri[32];

  snprintf(uri, 32, "psgm:play?titleid=%s", arg_list[1]);

  if (sceAppMgrLaunchAppByUri(0x20000, uri) < 0) {
    strcpy(res_msg, "Error: cannot launch the app. Is the TITLEID correct?\n");
  } else {
    strcpy(res_msg, "Launched.\n");
  }
}

void cmd_reboot(char **arg_list, size_t arg_count, char *res_msg) {
  scePowerRequestColdReset();
  strcpy(res_msg, "Rebooting...\n");
}

void cmd_screen(char **arg_list, size_t arg_count, char *res_msg) {
  char *state = arg_list[1];

  if (!strcmp(state, "on")) {
    scePowerRequestDisplayOn();
    strcpy(res_msg, "Turning display on...\n");
  } else if (!strcmp(state, "off")) {
    scePowerRequestDisplayOff();
    strcpy(res_msg, "Turning display off...\n");
  } else {
    strcpy(res_msg, "Error: param should be 'on' or 'off'\n");
  }
}
