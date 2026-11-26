/*
    The board file of the Pico SDK for the demo board of Tiny Tapeout, v3.2
    and v3.3 (`asc2-kb/tt-devboard.md`). `cmake -DPICO_BOARD=tt_demoboard_v3`
    chooses it. The assembler reads this file too.
*/

#ifndef BOARDS_TT_DEMOBOARD_V3_H
#define BOARDS_TT_DEMOBOARD_V3_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

#define TT_DEMOBOARD_V3 1

// The RP2350B, with 48 pins. The pins of the chip's outputs are above GPIO31.
#define PICO_RP2350A 0

// No source says which stepping the board has, so the first one counts too
pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

// The W25Q32JV of the board, 4 MB
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (4 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

#endif
