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
project(user C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)

# Flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# Include kernel headers
include_directories(
  ../kernel
)

# Executable
add_executable(user
  main.c
)

# Dependencies
add_dependencies(user vitashell_kernel_stubs)

# Link libraries
target_link_libraries(user
  ${CMAKE_CURRENT_BINARY_DIR}/../kernel/libVitaShellKernel2_stub.a
  SceLibKernel_stub
  SceIofilemgr_stub
)

# Create user module self
vita_create_self(user.suprx user CONFIG exports.yml UNSAFE)

# Create user stubs
vita_create_stubs(vitashell_user_stubs user exports.yml)
