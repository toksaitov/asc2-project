cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(bench_mem C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(bench_mem mem.c)
pico_generate_pio_header(bench_mem ${CMAKE_CURRENT_LIST_DIR}/rpbus.pio)
# No loop-to-memcpy rewriting and no jump tables (they land in flash):
# core 1 must never touch flash.
target_compile_options(bench_mem PRIVATE -Wall -Wextra -Werror -O2
    -fno-tree-loop-distribute-patterns -fno-jump-tables -fno-strict-aliasing)
target_link_libraries(bench_mem pico_stdlib pico_multicore hardware_pwm
    hardware_pio hardware_flash hardware_vreg hardware_sync)

# USB serial only. The default UART stdio would drive GP0 (UART0 TX),
# which is the RST_N wire to the DE10.
pico_enable_stdio_usb(bench_mem 1)
pico_enable_stdio_uart(bench_mem 0)

pico_add_extra_outputs(bench_mem)
