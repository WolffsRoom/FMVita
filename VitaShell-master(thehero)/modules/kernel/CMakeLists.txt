cmake_minimum_required(VERSION 3.10)

# VitaSDK toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(kernel C)

# Include VitaSDK
include("${VITASDK}/share/vita.cmake" REQUIRED)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# Kernel executable
add_executable(kernel
  main.c
)

# Link libraries / stubs
target_link_libraries(kernel
  SceIofilemgrForDriver_stub
  SceSysclibForDriver_stub
  SceSysmemForDriver_stub
  SceModulemgrForDriver_stub
  SceThreadmgrForDriver_stub
  SceProcessmgrForDriver_stub
  SceNpDrmForDriver_stub
  taihenForKernel_stub
  taihenModuleUtils_stub
)

# Create Vita artifact
vita_create_self(kernel.skprx kernel CONFIG exports.yml UNSAFE)

# Generate stubs for VitaShell
vita_create_stubs(vitashell_kernel_stubs kernel exports.yml KERNEL)
