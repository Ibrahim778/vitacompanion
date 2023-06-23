cmake_minimum_required(VERSION 3.12)
set(CMAKE_TOOLCHAIN_FILE $ENV{SCE_PSP2_SDK_DIR}/host_tools/build/cmake/psp2-snc-toolchain.cmake)

project(vitacompanion)
include(VitaDevelopmentSuite)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

add_subdirectory(VCKernel)  

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
enable_language(EMD)

add_library(${PROJECT_NAME} MODULE

    include/main.h
    include/ftpvita.h
    include/netmgr.h
    include/common.h
    include/cmd.h
    include/print.h
    include/cmd_definitions.h
    include/zip.h
    include/sha1.h
    include/promote.h
    include/head_bin.h
    include/dialog.h
    include/usb.h
    include/widgets.h
    include/compressed_file.h

    src/netmgr.cpp
    src/main.cpp
    src/ftpvita.c
    src/cmd.cpp
    src/cmd_definitions.cpp
    src/zip.cpp
    src/compressed_file.cpp
    src/promote.c
    src/sha1.c
    src/dialog.cpp
    src/usb.cpp
    src/widgets.cpp
    src/libc2paf.c

    # minizip
    src/minizip/unzip.c
    src/minizip/ioapi.c
    src/minizip/zip.c

    ${PROJECT_NAME}.emd
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -D_SCE_TARGET_OS_PSP2=1 -Xdiag=0 -Xquit=2 -D__SNC__
)

target_link_options(${PROJECT_NAME} PRIVATE
  --prx-no-runtime-support
)

target_link_directories(${PROJECT_NAME} PRIVATE
  ${VDSUITE_LIBRARY_DIRECTORIES}
  ${CMAKE_BINARY_DIR}/VCKernel/stubs
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${VDSUITE_USER_INCLUDE_DIRECTORIES}
  ${VDSUITE_STANDARD_INCLUDE_DIRECTORIES}
  $ENV{SCE_PSP2_SDK_DIR}/target/include
  $ENV{SCE_PSP2_SDK_DIR}/target/include_common
)

target_link_libraries(${PROJECT_NAME}
    SceLibKernel_stub
    SceKernel_stub
    SceThreadmgr_stub
    SceNet_stub_weak
    SceNetCtl_stub_weak
    SceRtcUser_stub
    SceAppMgrUser_stub
    SceRegMgr_stub
    SceDisplay_stub
	ScePafStdc_stub_weak
    ScePafThread_stub_weak
    ScePafTopLevel_stub_weak
    ScePafWidget_stub_weak
    ScePafCommon_stub_weak
    ScePafGraphics_stub_weak
    ScePafMisc_stub_weak
    ScePafResource_stub_weak
    SceAppMgr_stub
    ScePower_stub
    SceVshBridge_stub
    taihenUnsafe_stub
    VCKernel_stub_weak
    SceCommonGuiDialog_stub_weak
    ScePromoterUtil_stub_weak
    SceSysmodule_stub
    taihen_stub
    taihenUnsafe_stub
    SceMtpIf_stub
    QuickMenuReborn_stub_weak
    z
)

VDSuiteSignElf(${PROJECT_NAME}.suprx ${PROJECT_NAME})
