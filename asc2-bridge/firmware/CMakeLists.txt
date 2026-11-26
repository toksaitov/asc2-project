cmake_minimum_required(VERSION 3.13)

# `boards/` has the board file of the demo board of Tiny Tapeout
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

include(pico_sdk_import.cmake)

project(bridge C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(bridge bridge.c)
pico_generate_pio_header(bridge ${CMAKE_CURRENT_LIST_DIR}/rpbus.pio)

# Core 1 runs from RAM and must not call into the flash. The compiler would
# put the table of a `switch` there, and it would turn a loop that copies
# bytes into a call of `memcpy`.
target_compile_options(bridge PRIVATE -Wall -Wextra -Werror -O2
    -fno-jump-tables -fno-tree-loop-distribute-patterns)
target_link_libraries(bridge pico_stdlib pico_multicore hardware_pwm
    hardware_pio hardware_flash hardware_vreg hardware_sync hardware_i2c)

# The firmware asks for no memory while it runs. The SDK keeps 2 KB of RAM
# for such requests, and the RAM of an RP2040 is full with the RAM of the
# chip and the image of its program.
target_compile_definitions(bridge PRIVATE PICO_HEAP_SIZE=0x100)

# The address of the chip in the multiplexer of its shuttle
if (PICO_BOARD STREQUAL "tt_demoboard_v3")
    if (NOT DEFINED TT_DESIGN)
        message(FATAL_ERROR "The demo board needs -DTT_DESIGN=N, the address of the chip on its shuttle")
    endif()
    target_compile_definitions(bridge PRIVATE TT_DESIGN=${TT_DESIGN}u)
endif()

# The console is the USB serial port. The UART of the SDK would drive GP0,
# which is the reset of the chip.
pico_enable_stdio_usb(bridge 1)
pico_enable_stdio_uart(bridge 0)

pico_add_extra_outputs(bridge)
