add_executable(user
  main.c
)

target_compile_options(user PRIVATE -std=gnu11 -Wall -O3)
target_link_options(user PRIVATE -Wl,-q -nostdlib)
target_include_directories(user PRIVATE ../kernel)

add_dependencies(user vitashell_kernel_stubs)

target_link_libraries(user
  ${CMAKE_CURRENT_BINARY_DIR}/../kernel/libVitaShellKernel2_stub.a
  SceLibKernel_stub
  SceIofilemgr_stub
)

vita_create_self(user.suprx user CONFIG exports.yml UNSAFE)

vita_create_stubs(vitashell_user_stubs user exports.yml)
