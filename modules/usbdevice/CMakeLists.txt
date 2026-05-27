add_executable(usbdevice
  main.c
)

target_compile_options(usbdevice PRIVATE -std=gnu11 -Wall -O3)
target_link_options(usbdevice PRIVATE -Wl,-q -nostdlib)

target_link_libraries(usbdevice
  SceUdcdForDriver_stub
  SceIofilemgrForDriver_stub
  SceSysclibForDriver_stub
  taihenForKernel_stub
)

vita_create_self(usbdevice.skprx usbdevice CONFIG exports.yml UNSAFE)
