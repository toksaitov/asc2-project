/*
    The pins of the bridge, for the two boards that it runs on.

    The bench: a Raspberry Pi Pico (RP2040) on jumper wires to the DE10-Lite
    with the chip in its FPGA. The header comment of `bridge.c` lists them.

    The demo board of Tiny Tapeout, v3.2 and v3.3 (RP2350B): the traces of
    the board decide. The board joins GPIO17 to the input 0 of the chip and
    so on, GPIO25 to the bidirectional pin 0, and GPIO33 to the output 0
    (`asc2-kb/tt-devboard.md`); `info.yaml` says what each pin of the chip
    is. `test/pins_test.py` checks this table against `info.yaml`. The five
    pins of the display and of the button are the ones of the analog header,
    which has no pins soldered in.
*/

#ifndef BRIDGE_PINS_H
#define BRIDGE_PINS_H

#if TT_DEMOBOARD_V3

#define PIN_RST          14
#define PIN_CLK          16
#define PIN_ACK          17 // input 0 of the chip
#define PIN_IRQ          18 // input 1
#define PIN_DBG_CK       19 // input 2
#define PIN_FREE         20 // input 3, which the chip does not use
#define PIN_DBG_DI       21 // input 4
#define PIN_KBD_CLK      22 // input 5
#define PIN_KBD_DATA     23 // input 6
#define PIN_STOP         24 // input 7
#define PIN_UIO          25 // the first of the eight pins of the bus
#define PIN_REQ          33 // output 0
#define PIN_DBG_DO       34 // output 1
#define PIN_CHIP_DRIVES  35 // output 2
#define PIN_BRIDGE_DRIVES 46 // not joined to anything
#define PINS_FIRST       16 // the pins to the chip, but for the reset
#define PINS_END         41

// The multiplexer of the shuttle, which chooses one design
#define PIN_CTRL_ENA     0
#define PIN_CTRL_RST     1
#define PIN_CTRL_INC     2

#define PIN_BUTTON       41
#define PIN_OLED_SDA     42
#define PIN_OLED_SCL     43
#define PIN_OLED_RST     44
#define PIN_OLED_DC      45

// The state machines of a PIO block see 32 of the 48 pins, GPIO16 to GPIO47
#define PIO_GPIO_BASE    16

// The bus rests high: the pull-down of the first RP2350 can fail (erratum
// E9). A bus that nobody drives reads 0xff, which is no status.
#define BUS_PULL_UP      true

#define REQ_LEVEL()      ((sio_hw->gpio_hi_in >> (PIN_REQ - 32)) & 1u)

// GPIO16 to GPIO47
#define PIN_LEVELS()     ((sio_hw->gpio_in >> 16) | (sio_hw->gpio_hi_in << 16))

// The reset button of the board pulls the line of the reset low without the bridge
#define CHIP_IS_RESET()  ((sio_hw->gpio_in & (1u << PIN_RST)) == 0)

#else

#define PIN_RST          0
#define PIN_CLK          1
#define PIN_UIO          2  // the first of the eight pins of the bus
#define PIN_REQ          10
#define PIN_ACK          11
#define PIN_IRQ          12
#define PIN_STOP         13
#define PIN_BRIDGE_DRIVES 14
#define PIN_CHIP_DRIVES  15
#define PIN_KBD_CLK      16
#define PIN_KBD_DATA     17
#define PIN_DBG_CK       18
#define PIN_DBG_DI       19
#define PIN_DBG_DO       20
#define PINS_FIRST       0  // the pins to the chip
#define PINS_END         21

// The pins of the bench check of a display and a button, which no wire
// joins to the chip
#define PIN_OLED_RST     21
#define PIN_BUTTON       22
#define PIN_OLED_SDA     26
#define PIN_OLED_SCL     27
#define PIN_OLED_DC      28

#define BUS_PULL_UP      false

#define REQ_LEVEL()      ((sio_hw->gpio_in >> PIN_REQ) & 1u)
#define PIN_LEVELS()     (sio_hw->gpio_in)
#define CHIP_IS_RESET()  false

#endif

#define UIO_MASK           (0xffull << PIN_UIO)
#define ACK_MASK           (1u << PIN_ACK)
#define IRQ_MASK           (1u << PIN_IRQ)
#define STOP_MASK          (1u << PIN_STOP)
#define BRIDGE_DRIVES_MASK (1ull << PIN_BRIDGE_DRIVES)

#endif
