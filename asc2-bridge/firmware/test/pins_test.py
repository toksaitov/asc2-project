"""
Checks the pins of the demo board of Tiny Tapeout in `../pins.h`.

The chip's `info.yaml` says what each pin of the chip is (its path is the
argument; `make pins` gives the one of the clone beside this repository,
https://github.com/toksaitov/ttsky26e-almurut2), and the netlists of the two
versions of the board (`../../../asc2-kb/tt-devboard-netlists.txt`) say to
which pin of the RP2350 the board joins it. Both together give the number
that each name in `pins.h` must have. The pins that the board uses for
something else must stay free, and the pins of the state machine must be
among the 32 that its PIO block sees.
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PINS = os.path.join(HERE, "..", "pins.h")
INFO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "..", "..", "ttsky26e-almurut2", "info.yaml")
NETLISTS = os.path.join(HERE, "..", "..", "..", "asc2-kb", "tt-devboard-netlists.txt")

# The names of `info.yaml` with their names in `pins.h`. FRAME has none: the bridge does not use it.
NAMES = {"ACK": "PIN_ACK", "IRQ": "PIN_IRQ", "DBG_CK": "PIN_DBG_CK", "DBG_DI": "PIN_DBG_DI",
         "KBD_CLK": "PIN_KBD_CLK", "KBD_DATA": "PIN_KBD_DATA", "STOP": "PIN_STOP", "REQ": "PIN_REQ",
         "DBG_DO": "PIN_DBG_DO", "UIO_OE": "PIN_CHIP_DRIVES", "D0": "PIN_UIO", "FRAME": None}

# The names of the board's own nets
BOARD_NETS = {"PIN_RST": "~{project_rst}", "PIN_CTRL_ENA": "ctrl_ena", "PIN_CTRL_RST": "~{ctrl_sel_rst}",
              "PIN_CTRL_INC": "ctrl_sel_inc", "PIN_BUTTON": "ADC1", "PIN_OLED_SDA": "ADC2",
              "PIN_OLED_SCL": "ADC3", "PIN_OLED_RST": "ADC4", "PIN_OLED_DC": "ADC5"}

SM_PINS = ["PIN_UIO", "PIN_REQ", "PIN_ACK", "PIN_BRIDGE_DRIVES", "PIN_CHIP_DRIVES"]

errors = 0


def check(is_right, message):
    global errors
    if not is_right:
        errors += 1
        print("FAILED: " + message)


def read_macros():
    """The macros of `pins.h` for the demo board that are numbers."""
    out = subprocess.check_output(["cc", "-E", "-dM", "-DTT_DEMOBOARD_V3=1", PINS], text=True)
    return {m.group(1): int(m.group(2)) for m in re.finditer(r"^#define (\w+) (\d+)$", out, re.M)}


def read_boards():
    """For each version of the board, the GPIO number of each net (without the name
    of its sheet), and the GPIO pins that the board joins to nothing."""
    boards = []
    for line in open(NETLISTS):
        if line.startswith("==="):
            is_board = "Demoboard" in line
            if is_board:
                boards.append((line.strip("= \n"), {}, set()))
        elif "::" in line and boards and is_board:
            net, pads = line.split("::", 1)
            m = re.search(r"\(GPIO(\d+)", pads)
            if m:
                name = net.strip().split("/")[-1]
                check(name not in boards[-1][1], "two nets have the name " + name)
                boards[-1][1][name] = int(m.group(1))
                if len(pads.split()) == 1:
                    boards[-1][2].add(int(m.group(1)))
    return boards


def read_pinout():
    """The pins of the chip that have a name, as (net of the board, name)."""
    nets = {"ui": "ui_in", "uo": "uo_out", "uio": "uio"}
    pinout = []
    for m in re.finditer(r"^ *(ui|uo|uio)\[(\d)\]: *\"(\w+)\"", open(INFO).read(), re.M):
        pinout.append((nets[m.group(1)] + m.group(2), m.group(3)))
    return pinout


macros = read_macros()
boards = read_boards()
pinout = read_pinout()
check(len(boards) == 2, "the netlists do not have two versions of the board")
check(len(pinout) == 19, "info.yaml does not name 19 pins")

for title, gpios, loose in boards:
    wanted = {}
    for net, name in pinout:
        m = re.fullmatch(r"D(\d)", name)
        if m:
            wanted["D%s of the bus" % m.group(1)] = (macros["PIN_UIO"] + int(m.group(1)), gpios[net])
        elif NAMES[name]:
            wanted[NAMES[name]] = (macros[NAMES[name]], gpios[net])
    for name, net in BOARD_NETS.items():
        wanted[name] = (macros[name], gpios[net])
    clk_net = "RP_proj_clk" if "RP_proj_clk" in gpios else "project_clk"
    wanted["PIN_CLK"] = (macros["PIN_CLK"], gpios[clk_net])

    wanted["PIN_FREE"] = (macros["PIN_FREE"], gpios["ui_in3"])

    for name, (has, board) in wanted.items():
        check(has == board, "%s: %s is GPIO%d and the board joins it to GPIO%d" % (title, name, has, board))

    used = [has for has, _ in wanted.values()]
    check(macros["PIN_BRIDGE_DRIVES"] in loose, title + ": the board joins PIN_BRIDGE_DRIVES to something")
    used.append(macros["PIN_BRIDGE_DRIVES"])
    check(len(set(used)) == len(used), title + ": two names have the same pin")
    for net, gpio in gpios.items():
        if net.startswith("mng") or net in ("RPLED", "IREFPWM", "man_clk"):
            check(gpio not in used, "%s: GPIO%d belongs to %s of the board" % (title, gpio, net))

base = macros["PIO_GPIO_BASE"]
check(base in (0, 16), "a PIO block sees GPIO0 to GPIO31 or GPIO16 to GPIO47, so its base is 0 or 16")
check(re.search(r"^ *ui\[3\]: *\"\"", open(INFO).read(), re.M), "info.yaml gives input 3 a name, and PIN_FREE takes it for free")
for name in SM_PINS:
    check(base <= macros[name] < base + 32, name + " is not among the pins that the PIO block sees")
check(macros["PIN_REQ"] == macros["PIN_UIO"] + 8, "REQ does not follow the eight pins of the bus")

if errors:
    sys.exit(1)
print("passed: the pins of the demo board (%d pins of the chip, two versions of the board)" % len(pinout))
