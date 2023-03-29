# VitaCompanion

Vitacompanion is a user module which makes developing homebrews for the PS Vita device easier. It does two things:
- Open a FTP server on port 1337
- Listen to commands on port 1338

This fork contains some UX and other internal improvements

# Build

Use VDSuite

# Install

Run VitaShell on your PS Vita, press SELECT to activate the FTP server and copy `vitacompanion.suprx` to `ur0:/tai`. Finally, add the following line to `ur0:/tai/config.txt`:

```
*main
ur0:tai/vitacompanion.suprx
```

# Usage

## FTP server

You can upload stuff to your vita by running:
```
curl --ftp-method nocwd -T somefile.zip ftp://IP_TO_VITA:1337/ux0:/somedir/
```
Or you can use your regular FTP client.

## Command server

Send a command by opening a TCP connection to the port 1338 of your Vita.

For example, you can reboot your vita by running:
```
echo reboot | nc IP_TO_PSVITA 1338
```

Note that you need to append a newline character to the command that you send. `echo` already adds one, which is why it works here.

### Available commands

Use the help command, you can also see include/command_definitions.h  
See src/command_definitions.cpp for sauce or to add your own commands

### Kernel Plugin

This fork of vitacompanion also provides an accompanying kernel plugin which is required for the following commands to function  
 - usb
 - self
 - skprx (tai command will work without it)  

# Integration in IDE's
 
 ## VSCode
 
 https://github.com/imcquee/vitacompanion-VSCODE
 
*Note:* Integration for commands specific to this fork is not present.

# Acknowledgements 

Thanks to [@devnoname120](https://github.com/devnoname120) and all the others behind the original vitacompanion.  
Thanks to [@GrapheneCt](https://github.com/GrapheneCt) for ScePaf & SceCommonGuiDialog reversing.  
Thanks to [@theOfficialFlow](https://github.com/theOfficialFlow) and [@Teakhanirons](https://github.com/teakhanirons) for VitaShell and EmergencyMount which make the base for my USB code