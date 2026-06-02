cmake_minimum_required(VERSION 3.10)

# VitaSDK defines
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

# Project start
project(usbdevice C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)

# Flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# Executable
add_executable(usbdevice
  main.c
)

# Link libraries
target_link_libraries(usbdevice
  SceUdcdForDriver_stub
  SceIofilemgrForDriver_stub
  SceSysclibForDriver_stub
  taihenForKernel_stub
)

# Create kernel self
vita_create_self(usbdevice.skprx usbdevice CONFIG exports.yml UNSAFE)

