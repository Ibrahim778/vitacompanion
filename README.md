# LoaderCompanion

LoaderCompanion is just vitacompanion but with some more commands speficically designed for use with Vita-FTPI-Core to make homebrew development way easier. 99% of the credits go to @devNoName120

# Build

Same as vitacompanion.

# Install

Run VitaShell on your PS Vita, press SELECT to activate the FTP server and copy `loaderCompanion.suprx` to `ur0:/tai`. Finally, add the following line to `ur0:/tai/config.txt`:

```
*main
ur0:tai/loaderCompanion.suprx
```

after this install the Kernel Module for this from here (coming soon)

## Commands

### USB

Start / Stop USB

usage usb <enable / disable> <storage type (either sd2vita or official)>

### VPK
Install a VPK

usage vpk < path >

### VPK Extracted
Install and already extracted VPK

usage ext_vpk < path >

### File (Needs kernel plugin)
Load a self from a file.

usage file < path >

# Acknowledgements

Thanks to @devNoName120 for vitacompanion, thanks to @SKGleba @The Princess of Sleeping and @GrapheneCT for helping me in a few things (jk everything). Thanks to Teakhanirons for EmergencyMount which is what I used for my usb code.
