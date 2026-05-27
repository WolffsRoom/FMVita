add_executable(kernel
  main.c
)

target_compile_options(kernel PRIVATE -std=gnu11 -Wall -O3)
target_link_options(kernel PRIVATE -Wl,-q -nostdlib)

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

vita_create_self(kernel.skprx kernel CONFIG exports.yml UNSAFE)

vita_create_stubs(vitashell_kernel_stubs kernel exports.yml KERNEL)
