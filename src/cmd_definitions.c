#include "cmd_definitions.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>
#include "usb.h"

#define COUNT_OF(arr) (sizeof(arr) / sizeof(arr[0]))

const cmd_definition cmd_definitions[] = {
    {.name = "destroy", .arg_count = 0, .executor = &cmd_destroy},
    {.name = "launch", .arg_count = 1, .executor = &cmd_launch},
    {.name = "reboot", .arg_count = 0, .executor = &cmd_reboot},
    {.name = "screen", .arg_count = 1, .executor = &cmd_screen},
    {.name = "usb", .arg_count = 2, .executor = &cmd_usb}};

const cmd_definition *cmd_get_definition(char *cmd_name) {
  for (unsigned int i = 0; i < COUNT_OF(cmd_definitions); i++) {
    if (!strcmp(cmd_name, cmd_definitions[i].name)) {
      return &(cmd_definitions[i]);
    }
  }

  return NULL;
}


void cmd_usb(char **arg_list, size_t arg_count, char *res_msg)
{
  char *type = arg_list[1];
  char *state = arg_list[2];
  if(strcmp(state, "enable"))
  {
    initUsb();
    if(strcmp(type, "sd2vita"))
    {
      memoryConfig = 2;
      initUsb();
    }
    else if(strcmp(type, "OFFICIAL"))
    {
      memoryConfig = 1;
      initUsb();
    }
    else
    {
      strcpy(res_msg, "Error invalid memory config given! It should be either OFFICIAL or sd2vita");
    }
    
  }
  else if(strcmp(state, "disable"))
  {
    int res = stopUsb();
    if(res < 0)
    {
      snprintf(res_msg, "Error stopping usb: %x", res);
    }
    else
    {
      strcpy(res_msg, "Success!");
    }
  }
  else
  {
    strcpy(res_msg, "Error argument should be either enable or disable !");
  }
  
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
