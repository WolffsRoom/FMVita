add_executable(patch
  main.c
)

target_compile_options(patch PRIVATE -std=gnu11 -Wall -O3)
target_link_options(patch PRIVATE -Wl,-q -nostdlib)

target_link_libraries(patch
  taihenForKernel_stub
)

vita_create_self(patch.skprx patch CONFIG exports.yml UNSAFE)
