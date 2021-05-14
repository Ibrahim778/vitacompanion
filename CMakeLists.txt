cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()


project(loaderCompanion)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -fno-pic -fno-PIC -std=gnu99 -Wno-implicit-function-declaration -Wno-misleading-indentation")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-psabi -Wno-unused-variable -Wwrite-strings -fpermissive")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
        src/Archives.c
        src/Archives.h
        src/main.c
        src/main.h
        src/log.c
        src/log.h
        src/cmd.c
        src/cmd.h
        src/net.c
        src/net.h
        src/sha1.c
        src/sha1.h
        src/parser.c
        src/promote.c
        src/parser.h
        src/cmd_definitions.c
        src/cmd_definitions.h
)

target_link_libraries(${PROJECT_NAME}
  taipool
  taihen_stub
  ftpvita
  k
  gcc
  SceLibKernel_stub_weak
  SceNet_stub_weak
  SceNetCtl_stub_weak
  SceSysmodule_stub_weak
  SceKernelThreadMgr_stub_weak
  LoaderCompanionKernel_stub_weak
  SceIofilemgr_stub_weak
  SceRtc_stub_weak
  SceAppMgr_stub_weak
  ScePower_stub_weak
  ScePaf_stub_weak
  ScePromoterUtil_stub_weak
  SceShellSvc_stub_weak
  SceIncomingDialog_stub_weak
  z
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}
  UNSAFE
  CONFIG ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.yml
)
