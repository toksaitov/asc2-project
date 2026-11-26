#!/usr/bin/env python3
"""Bench Test 4: which RP memories the chip side can use, how fast, how reliably.

Needs the DE10 running bench/de10-lite (instances "T4A", "T4B", "T4C") and the
Pico running bench/firmware, both on bus protocol v1. The DE10 runs the chip's
bus and reset RTL behind pin delays that stand in for the Tiny Tapeout
pads; the Pico serves memory at 150 MHz with the handshake in PIO, as the
devkit's RP2350 would. Memory map: RAM, a 16 KB ROM (served from a RAM
copy on the Pico), devices, a disk in flash sectors.

Phases (any order, several per call):
  prep    fill the Pico's ROM copy with h(o, FSEED) and check it (always
          runs first)
  manual  single transactions against a model of the memory map: RAM,
          kernel page, flash window, every device register, disk
          registers, every fault; at 1 MHz and at the top clock
  matrix  chip clock x pin delay, the full mix of every memory and device
          with interrupts, stop on the first error
  speed   one memory type at a time at the top clock with TT-like
          delays: transactions per second, time per round trip
  long25  10 minutes at 25 MHz with TT-like delays (the Test 3 hang);
          on a hang, keeps the DE10 state and the Pico trace
  soak    the full mix for --hours at the top clock with TT-like delays
  faults  the full mix at the top clock while the Pico breaks one
          transaction in every 500 on purpose, each way it can (drop,
          wrong SEQ, stall, lost answer, late answer); every fault must
          be recovered with no error; then a dead Pico (every transaction
          dropped) must end in LINK_ERROR, not a hang, and the bus must
          work again after it
  torture the chip clock taken from the Pico's CLK jumper wire instead of
          the DE10's PLL (the noisy wire of the 25 MHz hangs), 25 MHz;
          a measurement: how many link faults the retry absorbs; a hang
          fails the run
  tear    the torn clock edge the torture test found, on demand: the DE10
          throws the bus unit's state into IDLE in the middle of one
          transaction in every 512, in each of four states in turn; every
          tear must be one bad-state fault and one retry, with no error

At the end, and on any error, the chip is held in reset and every Pico pin
goes back to an input.

Usage: python3 -I memtest.py prep manual matrix speed long25
       python3 -I memtest.py soak --hours 4 [--div 3]
"""

import argparse
import json
import math
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_link import BenchError, De10, Pico  # noqa: E402

SYS_HZ = 150_000_000
FSEED = 0x5EED0F1A
MANUAL_DIV = 150                       # 1 MHz
MATRIX_DIVS = [150, 15, 6, 5, 4, 3]    # 1, 10, 25, 30, 37.5, 50 MHz
MATRIX_DELAYS = [(0, 0), (2, 2), (4, 4)]
TT_DELAY = (2, 2)                      # 10-15 ns each way
DIV_25 = 6

OK, BUS_ERROR, PROTECTION, MISALIGNED, BAD_NEXT = 0, 1, 2, 3, 4
ST_NAME = {0: "OK", 1: "BUS_ERROR", 2: "PROTECTION", 3: "MISALIGNED", 4: "BAD_NEXT"}

RAM_SIZE = 0x30000
KERNEL_BASE = 0x2F000
FLASH_BASE = 0x800000   # the ROM (the DE10 still calls its kinds "flash")
FLASH_WIN = 0x4000      # 16 KB
DEV_BASE = 0xFF0000
DEV_ID = 0x324D4C41

STRESS_WORDS = 0xA000    # 160 KB, below the disk buffers at 0x028000
ALL_USER_WORDS = 0xBC00  # 188 KB, every RAM byte below the kernel page
FLASH_WORDS = FLASH_WIN // 4
MASK_ALL = 0xFFF

# T4A source fields: (lsb, width)
S = {
    "man_go": (0, 1), "stress": (1, 1), "stop_err": (2, 1), "clear": (3, 1),
    "mask": (4, 12), "dly_in": (16, 4), "dly_out": (20, 4), "hdr": (24, 8),
    "addr": (32, 24), "wdata": (56, 32), "seed": (88, 32), "fseed": (120, 32),
    "ram_words": (152, 16), "flash_words": (168, 18), "log2": (186, 4),
    "tear_on": (190, 1), "tear_sel": (191, 2), "tear_log2": (193, 4),
}

# T4A probe fields: (lsb, width)
P = {
    "man_done": (0, 1), "man_status": (1, 8), "man_rdata": (9, 32),
    "stress_active": (41, 1), "bus_idle": (42, 1), "tx": (43, 32),
    "err": (75, 32), "rt": (107, 40), "cycles": (147, 48),
    "e_valid": (195, 1), "e_kind": (196, 4), "e_status": (200, 8),
    "e_addr": (208, 24), "e_exp": (232, 32), "e_got": (264, 32),
    "e_hdr": (296, 8), "viol": (304, 16), "rst_count": (320, 16),
    "rst_short": (336, 32), "ack_glitch": (368, 32), "bus_dbg": (400, 16),
    "seq": (416, 12), "txmax": (428, 32),
}

KINDS = ["ram_rd", "ram_wr", "ram_fetch", "ram_next", "flash_rd", "flash_fetch",
         "timer", "scratch", "console", "disk", "fault", "fill", "irq_ack",
         "sectors", "ovl_events", "ovl_long"]
E_KIND = {1: "RAM", 2: "FLASH", 3: "TIMER", 4: "SCRATCH", 5: "CONSOLE",
          6: "DISK data", 7: "DISK", 8: "FAULT", 10: "IRQ"}
BUS_STATE = ["IDLE", "SEND_TOGGLE", "SEND_WAIT", "HANDOFF", "RECV_WAIT", "RECV_TOGGLE", "QUIET", "BAD7"]
OP_NAME = ["RAM_RD", "RAM_WR", "RAM_FETCH", "RAM_NEXT", "FLASH_RD", "FLASH_FETCH",
           "TIMER", "SCRATCH", "CONSOLE", "DISK", "FAULT", "FILL"]
T_NAME = ["IDLE", "PLAN", "ISSUE", "WAIT", "EVAL", "?5", "?6", "?7"]


def field(value, lsb_width):
    lsb, width = lsb_width
    return (value >> lsb) & ((1 << width) - 1)


def with_field(value, lsb_width, x):
    lsb, width = lsb_width
    mask = ((1 << width) - 1) << lsb
    return (value & ~mask) | ((x << lsb) & mask)


def h(a, s):
    """The expected-data function shared with the DE10 and the Pico."""
    x = ((a & 0xFFFFFF) ^ s) & 0xFFFFFFFF
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= x >> 17
    x ^= (x << 5) & 0xFFFFFFFF
    return x


def con_byte(j):
    return h(j, 0xC0DE0000) & 0xFF


def decode_bus_dbg(d):
    return (f"{BUS_STATE[(d >> 13) & 7]} req={(d >> 12) & 1} oe={(d >> 11) & 1} "
            f"ack_seen={(d >> 10) & 1} ack_sync={(d >> 9) & 1} got_status={(d >> 8) & 1} "
            f"tx_i={(d >> 4) & 15} seq={(d >> 3) & 1} rx_left={d & 7}")


def t_rp_us(hz):
    """The Pico's mid-transaction timeout: about 4096 chip clocks (the
    chip's own is 65535, and it then stays quiet 65535 more)."""
    return max(20, math.ceil(4096 * 1e6 / hz))


def decode_seq(s):
    return (f"t={T_NAME[(s >> 9) & 7]} op={OP_NAME[(s >> 5) & 15] if ((s >> 5) & 15) < 12 else (s >> 5) & 15} "
            f"disk={(s >> 1) & 15} irq={s & 1}")


class Model:
    """The RP side as bench/firmware serves it, for single transactions."""

    def __init__(self, fseed):
        self.fseed = fseed
        self.ram = bytearray(RAM_SIZE)
        self.scratch = 0
        self.tx = 0
        self.faults = 0
        self.next = [None, None]   # [data, fetch]
        self.con_deliv = 0
        self.con_echo = 0
        self.disk = {"sec": 0, "buf": 0, "cmd": 0, "err": 0, "done": 0}

    def dev(self, off, wr, wdata):
        """Returns (status, rdata, checked)."""
        d = self.disk
        if off == 0x00:
            return (BUS_ERROR, 0, True) if wr else (OK, DEV_ID, True)
        if off == 0x04:
            if wr:
                self.scratch = wdata
                return OK, 0, True
            return OK, self.scratch, True
        if off == 0x08:
            return (BUS_ERROR, 0, True) if wr else (OK, self.tx, True)
        if off == 0x0C:
            return (BUS_ERROR, 0, True) if wr else (OK, 0, False)
        if off == 0x10:
            # Single transactions are milliseconds apart: the input FIFO
            # always holds bytes.
            return (BUS_ERROR, 0, True) if wr else (OK, 3, True)
        if off == 0x14:
            if wr:
                self.con_echo += 1
                return OK, 0, True
            b = con_byte(self.con_deliv)
            self.con_deliv += 1
            return OK, b, True
        if off == 0x18:
            return OK, 0, True      # interrupts are off in this phase
        if off in (0x20, 0x24):
            key = "sec" if off == 0x20 else "buf"
            if wr:
                d[key] = wdata
                return OK, 0, True
            return OK, d[key], True
        if off == 0x28:
            if wr:
                d["cmd"] = wdata
                if (d["sec"] >= 64 or d["buf"] & 3 or d["buf"] > RAM_SIZE - 4096
                        or wdata not in (1, 2)):
                    d["err"] = 1
                else:
                    d["done"] += 1  # finished long before the next transaction
                return OK, 0, True
            return OK, d["cmd"], True
        if off == 0x2C:
            if wr:
                return BUS_ERROR, 0, True
            return OK, (d["done"] << 16) | (d["err"] << 1), True
        return BUS_ERROR, 0, True

    def access(self, hdr, addr, wdata):
        wr = (hdr >> 7) & 1
        nx = (hdr >> 6) & 1
        size = (1, 2, 4, 0)[(hdr >> 4) & 3]
        kind = (hdr >> 3) & 1
        kernel = (hdr >> 2) & 1
        st, rd, checked = OK, 0, True
        if nx:
            if self.next[kind] is None:
                st = BAD_NEXT
            else:
                addr = self.next[kind]
        if st == OK and size == 0:
            st = BUS_ERROR
        if st == OK and addr % size:
            st = MISALIGNED
        if st == OK:
            if addr + size <= RAM_SIZE:
                if not kernel and addr + size > KERNEL_BASE:
                    st = PROTECTION
                elif wr:
                    self.ram[addr:addr + size] = (wdata & ((1 << (8 * size)) - 1)).to_bytes(size, "little")
                else:
                    rd = int.from_bytes(self.ram[addr:addr + size], "little")
            elif FLASH_BASE <= addr and addr + size <= FLASH_BASE + FLASH_WIN:
                if wr:
                    st = BUS_ERROR
                else:
                    o = addr - FLASH_BASE
                    rd = (h(o & ~3, self.fseed) >> (8 * (o & 3))) & ((1 << (8 * size)) - 1)
            elif DEV_BASE <= addr < DEV_BASE + 0x30:
                if size != 4:
                    st = BUS_ERROR
                elif kind:
                    st = PROTECTION
                else:
                    st, rd, checked = self.dev(addr - DEV_BASE, wr, wdata)
            else:
                st = BUS_ERROR
        if st == OK:
            self.next[kind] = addr + size
        else:
            self.next[kind] = None
            self.faults += 1
        self.tx += 1
        return st, (rd if st == OK and not wr else 0), checked


# (name, hdr, addr, wdata). hdr bits: 7 write, 6 next, 5:4 size
# (0=1, 1=2, 2=4, 3=reserved), 3 fetch, 2 kernel. The model gives the
# expected answer.
MANUAL = [
    ("data NEXT right after reset",          0x64, 0x000000, 0),
    ("fetch NEXT right after reset",         0x6C, 0x000000, 0),
    ("read ID",                              0x24, 0xFF0000, 0),
    ("read TXCOUNT",                         0x24, 0xFF0008, 0),
    ("write SCRATCH",                        0xA4, 0xFF0004, 0xDEADBEEF),
    ("read SCRATCH back",                    0x24, 0xFF0004, 0),
    ("write read-only ID",                   0xA4, 0xFF0000, 0x12345678),
    ("byte read of a device register",       0x04, 0xFF0004, 0),
    ("fetch from a device register",         0x2C, 0xFF0000, 0),
    ("read TIMER",                           0x24, 0xFF000C, 0),
    ("read TIMER again (must not go back)",  0x24, 0xFF000C, 0),
    ("write read-only TIMER",                0xA4, 0xFF000C, 1),
    ("read CON_STAT",                        0x24, 0xFF0010, 0),
    ("read CON_DATA (input byte 0)",         0x24, 0xFF0014, 0),
    ("read CON_DATA (input byte 1)",         0x24, 0xFF0014, 0),
    ("write CON_DATA (echo byte 0)",         0xA4, 0xFF0014, con_byte(0)),
    ("write CON_DATA (echo byte 1)",         0xA4, 0xFF0014, con_byte(1)),
    ("read IRQ_ACK (no interrupt)",          0x24, 0xFF0018, 0),
    ("write IRQ_ACK",                        0xA4, 0xFF0018, 1),
    ("read unassigned register 0xFF001C",    0x24, 0xFF001C, 0),
    ("read past the registers 0xFF0030",     0x24, 0xFF0030, 0),
    ("write DISK_SEC 5",                     0xA4, 0xFF0020, 5),
    ("write DISK_BUF 0x029000",              0xA4, 0xFF0024, 0x029000),
    ("read DISK_SEC back",                   0x24, 0xFF0020, 0),
    ("read DISK_BUF back",                   0x24, 0xFF0024, 0),
    ("read DISK_STAT (idle)",                0x24, 0xFF002C, 0),
    ("write DISK_CMD 1 (read sector 5)",     0xA4, 0xFF0028, 1),
    ("read DISK_STAT (1 done)",              0x24, 0xFF002C, 0),
    ("read DISK_CMD back",                   0x24, 0xFF0028, 0),
    ("write DISK_CMD 3 (invalid)",           0xA4, 0xFF0028, 3),
    ("read DISK_STAT (error)",               0x24, 0xFF002C, 0),
    ("write read-only DISK_STAT",            0xA4, 0xFF002C, 0),
    ("word write 0x100",                     0xA4, 0x000100, 0x11223344),
    ("byte write 0x101",                     0x84, 0x000101, 0xAA),
    ("halfword write 0x102",                 0x94, 0x000102, 0xBBCC),
    ("word read 0x100",                      0x24, 0x000100, 0),
    ("byte read 0x103",                      0x04, 0x000103, 0),
    ("halfword read 0x100",                  0x14, 0x000100, 0),
    ("misaligned word read 0x102",           0x24, 0x000102, 0),
    ("data NEXT after a data fault",         0x64, 0x000000, 0),
    ("misaligned halfword read 0x101",       0x14, 0x000101, 0),
    ("word write 0x020000 (above 128 KB)",   0xA4, 0x020000, 0x5A5A1234),
    ("word read 0x020000",                   0x24, 0x020000, 0),
    ("user read below kernel page 0x02EFFC", 0x20, 0x02EFFC, 0),
    ("user read of kernel page 0x02F000",    0x20, 0x02F000, 0),
    ("user write to kernel page",            0xA0, 0x02F000, 0xCAFEF00D),
    ("kernel write kernel page 0x02F000",    0xA4, 0x02F000, 0xCAFEF00D),
    ("kernel read kernel page 0x02F000",     0x24, 0x02F000, 0),
    ("kernel write last RAM word 0x02FFFC",  0xA4, 0x02FFFC, 0x0BADC0DE),
    ("kernel read last RAM word 0x02FFFC",   0x24, 0x02FFFC, 0),
    ("read just past RAM 0x030000",          0x24, 0x030000, 0),
    ("unmapped read 0x400000",               0x24, 0x400000, 0),
    ("ROM word 0x800000",                  0x24, 0x800000, 0),
    ("ROM word 0x800004",                  0x24, 0x800004, 0),
    ("ROM halfword 0x800002",              0x14, 0x800002, 0),
    ("ROM byte 0x800003",                  0x04, 0x800003, 0),
    ("ROM misaligned word 0x800002",       0x24, 0x800002, 0),
    ("ROM last word 0x803FFC",               0x24, 0x803FFC, 0),
    ("just past the ROM 0x804000",           0x24, 0x804000, 0),
    ("write to ROM 0x800000",              0xA4, 0x800000, 1),
    ("user read of ROM 0x800010",          0x20, 0x800010, 0),
    ("fetch ROM 0x800100",                 0x2C, 0x800100, 0),
    ("fetch NEXT (0x800104)",                0x6C, 0x000000, 0),
    ("fetch NEXT (0x800108)",                0x6C, 0x000000, 0),
    ("data read ROM 0x802340",               0x24, 0x802340, 0),
    ("data NEXT (0x802344)",                 0x64, 0x000000, 0),
    ("fetch NEXT keeps its pointer (0x80010C)", 0x6C, 0x000000, 0),
    ("fetch RAM 0x000100 (user)",            0x28, 0x000100, 0),
    ("reserved size read",                   0x34, 0x000100, 0),
    ("reserved size write",                  0xB4, 0x000100, 0),
    ("read TXCOUNT again",                   0x24, 0xFF0008, 0),
]

# Speed runs, one memory type at a time:
# (name, op mask, RAM words, ROM words, log2 run length, interrupts, unused)
# 25 MHz diagnosis variants: (name, divider, (dly_in, dly_out), CLK drive,
# IRQs[, Pico data drive])
DIAG = [
    ("25 MHz, CLK 4 mA slow (as before)", 6, (2, 2), "4 0", True),
    ("25 MHz, CLK 12 mA fast", 6, (2, 2), "12 1", True),
    ("30 MHz, CLK 4 mA slow", 5, (2, 2), "4 0", True),
    ("50 MHz, CLK 4 mA slow", 3, (2, 2), "4 0", True),
    ("25 MHz, delays 4/4, CLK 4 mA slow", 6, (4, 4), "4 0", True),
    ("25 MHz, delays 4/4, CLK 12 mA fast", 6, (4, 4), "12 1", True),
    ("25 MHz, no interrupts", 6, (2, 2), "4 0", False),
    ("30 MHz, no interrupts", 5, (2, 2), "4 0", False),
    ("37.5 MHz, CLK 4 mA slow", 4, (2, 2), "4 0", True),
    ("10 MHz, CLK 4 mA slow", 15, (2, 2), "4 0", True),
    ("25 MHz, delays 4/0 (inputs only)", 6, (4, 0), "4 0", True),
    ("25 MHz, delays 0/4 (outputs only)", 6, (0, 4), "4 0", True),
    ("25 MHz, delays 0/0", 6, (0, 0), "4 0", True),
    ("25 MHz, Pico data 2 mA slow", 6, (2, 2), "4 0", True, "2 0"),
    ("25 MHz, delays 0/2", 6, (0, 2), "4 0", True),
    ("25 MHz, delays 2/0", 6, (2, 0), "4 0", True),
    ("30 MHz, delays 4/0", 5, (4, 0), "4 0", True),
    ("37.5 MHz, delays 4/0", 4, (4, 0), "4 0", True),
    ("50 MHz, delays 4/0", 3, (4, 0), "4 0", True),
    ("10 MHz, delays 4/0", 15, (4, 0), "4 0", True),
    ("25 MHz, delays 4/0", 6, (4, 0), "4 0", True),
    ("37.5 MHz, delays 2/2", 4, (2, 2), "4 0", True),
    ("25 MHz, delays 0/4", 6, (0, 4), "4 0", True),
]

SPEED = [
    ("RAM read, random 1/2/4 B",                0x001, STRESS_WORDS, 0, 4, False, 0),
    ("RAM write, random 1/2/4 B",               0x002, STRESS_WORDS, 0, 4, False, 0),
    ("RAM fetch, runs of 256 words (NEXT)",     0x004, STRESS_WORDS, 0, 8, False, 0),
    ("RAM data read, runs of 256 words (NEXT)", 0x008, STRESS_WORDS, 0, 8, False, 0),
    ("ROM read, random word in 16 KB",          0x010, 0, FLASH_WORDS, 4, False, 0),
    ("ROM fetch, runs of 256 words",            0x020, 0, FLASH_WORDS, 8, False, 0),
    ("timer register",                          0x040, 0, 0, 4, False, 0),
    ("scratch register write and read",         0x080, 0, 0, 4, False, 0),
    ("console echo",                            0x100, 0, 0, 4, False, 0),
    ("disk: write a sector, read it back",      0x200, 0, 0, 4, False, 0),
    ("interrupts while reading the timer",      0x840, 0, 0, 4, True, 0),
    ("all user RAM (188 KB): reads, writes, runs, interrupts", 0x80F, ALL_USER_WORDS, 0, 4, True, 0),
]


class Bench:
    def __init__(self, pico, de10):
        self.pico = pico
        self.de10 = de10
        self.src = 0
        self.cm_clr = 0
        self.cm_lo = 0
        self.cm_hi = 255
        self.chip_div = 6
        self.ck_sel = 0       # 1: the chip clock comes from the Pico's CLK wire (torture)
        self.rp_hz = SYS_HZ   # the Pico's system clock (changed by the pscale phase)

    def set_src(self, **fields):
        for name, x in fields.items():
            self.src = with_field(self.src, S[name], x)
        self.de10.write(self.src, "T4A")

    def probe(self):
        v = self.de10.read("T4A")
        return {name: field(v, f) for name, f in P.items()}

    def counts(self):
        v = self.de10.read("T4B")
        out = {name: (v >> (32 * k)) & 0xFFFFFFFF for k, name in enumerate(KINDS[:14])}
        out["ovl_long"] = (v >> 448) & 0xFFFFFFFF
        out["ovl_events"] = (v >> 480) & 0xFFFF   # wraps at 16 bits
        return out

    def pico_stats(self):
        parts = self.pico.cmd("s").split()[2:]
        out = {}
        for x in parts:
            k, v = x.split("=")
            out[k] = int(v, 16 if k in ("pins", "flevel") else 10)
        return out

    def restart(self, div, dly, irq=False, uncached=0, clk="4 0", drive="4 0"):
        """Hold the chip in reset, set CLK, delays and the RP options,
        restart the RP server with cleared RAM and disk state, release."""
        self.set_src(stress=0)
        self.pico.cmd("r 0")
        self.pico.cmd(f"k {clk}")
        self.pico.cmd(f"d {drive}")
        # The Pico's CLK wire, watched only. Its divider follows the Pico's
        # system clock so the wire keeps the chip clock's frequency (and the
        # CLK monitor's bounds stay right) when pscale changes that clock.
        self.pico.cmd(f"c {max(1, round(div * self.rp_hz / SYS_HZ))}")
        self.set_chip_div(div)
        hz = SYS_HZ // div                # the chip clock: the DE10's 150 MHz / div
        time.sleep(0.005)
        self.set_src(dly_in=dly[0], dly_out=dly[1])
        self.pico.cmd("g")
        self.pico.cmd("z")
        self.pico.cmd(f"T {t_rp_us(hz)}")
        self.pico.cmd("F 0 0")
        self.pico.cmd("V 0")
        self.pico.cmd(f"n {1 if irq else 0}")
        time.sleep(0.01)
        self.pico.cmd("r 1")
        time.sleep(0.02)
        p = self.probe()
        if p["tx"] != 0 or p["stress_active"] or not p["bus_idle"]:
            raise BenchError(f"the DE10 did not come out of reset clean: {p}")
        p.update({"c0_" + k: v for k, v in self.counts().items()})
        p.update({"l0_" + k: v for k, v in self.link().items()})
        p.update({"f0_" + k: v for k, v in self.frame().items()})
        return hz, p

    def manual(self, hdr, addr, wdata):
        # Fields first, then the go toggle in a separate write: the source
        # bits cross into the chip clock one by one.
        self.set_src(hdr=hdr, addr=addr, wdata=wdata)
        before = self.probe()["man_done"]
        self.set_src(man_go=field(self.src, S["man_go"]) ^ 1)
        end = time.time() + 2.0
        while time.time() < end:
            p = self.probe()
            if p["man_done"] != before:
                return p["man_status"], p["man_rdata"]
        raise BenchError(f"manual transaction hdr {hdr:02X} addr {addr:06X} never finished")

    def trace(self):
        lines, final = self.pico.lines("t", timeout=20)
        return lines

    def _write_c(self):
        self.de10.write(self.cm_clr | (self.cm_lo << 1) | (self.cm_hi << 9) | (self.chip_div << 17)
                        | (self.ck_sel << 25), "T4C")

    def set_ck_sel(self, sel):
        """Chip clock source: 0 the DE10's PLL, 1 the Pico's CLK wire.
        Only while RST_N is low."""
        self.ck_sel = sel
        self._write_c()

    def link(self):
        """The chip side's link fault counters (T4C, 32-bit wrapping)."""
        v = self.de10.read("T4C")
        return {k: (v >> (224 + 32 * i)) & 0xFFFFFFFF
                for i, k in enumerate(("timeouts", "bad_answers", "bad_states", "link_errors"))}

    def frame(self):
        """The frame checks at the pins (T4C): rises and balance (32-bit
        wrapping), REQ changes and uio_oe outside frame (16-bit
        saturating)."""
        v = self.de10.read("T4C")
        f = lambda lsb, w: (v >> lsb) & ((1 << w) - 1)
        return {"rises": f(352, 32), "balance": f(384, 32), "req_out": f(416, 16), "oe_out": f(432, 16)}

    def tears(self):
        """Tears applied by the DE10's tear test (T4C, 32-bit wrapping)."""
        return (self.de10.read("T4C") >> 448) & 0xFFFFFFFF

    def set_chip_div(self, div):
        """The chip clock on the DE10: 150 MHz / div from its PLL. Only
        while RST_N is low."""
        self.chip_div = div
        self._write_c()

    def clkmon_start(self, div):
        """Clear the CLK wire monitor (T4C) and set its bounds: a period of
        the Pico's CLK is div * 4 / 3 fast cycles (200 MHz against
        150 MHz / div), give or take one for the sampling."""
        nom = div * 4 / 3
        self.cm_lo = max(math.floor(nom) - 1, 0)
        self.cm_hi = min(math.ceil(nom) + 1, 255)
        self.cm_clr ^= 1
        self._write_c()
        return self.cm_lo, self.cm_hi

    def clkmon(self):
        v = self.de10.read("T4C")
        f = lambda lsb, w: (v >> lsb) & ((1 << w) - 1)
        out = {"periods": f(0, 32), "short": f(32, 32), "long": f(64, 32),
               "per_min": f(96, 8), "per_max": f(104, 8), "high_min": f(112, 8), "low_min": f(120, 8)}
        now, t_anom, t_req = f(192, 32), f(128, 32), f(160, 32)
        out["anomaly_s_ago"] = ((now - t_anom) & 0xFFFFFFFF) / 200e6 if t_anom else None
        out["req_change_s_ago"] = ((now - t_req) & 0xFFFFFFFF) / 200e6
        return out

    def glitches(self):
        lines, final = self.pico.lines("w", timeout=5)
        return lines

    def profile(self):
        parts = self.pico.cmd("P").split()[2:]
        return {k: int(v) for k, v in (x.split("=") for x in parts)}

    def safe(self):
        try:
            self.set_src(stress=0)
        except Exception:
            pass
        try:
            self.pico.ask("n 0")
            self.pico.ask("r 0")
            time.sleep(0.01)
            self.pico.ask("x")
        except Exception:
            pass


def mhz(hz):
    return f"{hz / 1e6:g} MHz"


def link_str(bench, p0):
    lk = bench.link()
    d = [(lk[k] - p0["l0_" + k]) & 0xFFFFFFFF for k in ("timeouts", "bad_answers", "bad_states", "link_errors")]
    return "/".join(str(x) for x in d)


def phase_prep(bench):
    bench.set_src(stress=0)
    bench.pico.cmd("r 0")
    f = bench.pico.cmd(f"f {FSEED:08x}")
    v = bench.pico.cmd(f"v {FSEED:08x}")
    print(f"ROM: {f}; {v}")
    return {"fill": f, "verify": v, "pass": " bad=0 " in v}


def run_manual(bench, div, label):
    hz, p0 = bench.restart(div, TT_DELAY)
    print(f"Manual transactions at {mhz(hz)}, delays {TT_DELAY} ({label})")
    model = Model(FSEED)
    rows, fails = [], 0
    timer_prev = None
    t_prev = None
    for name, hdr, addr, wdata in MANUAL:
        est, erd, checked = model.access(hdr, addr, wdata)
        t_issue = time.time()
        st, rd = bench.manual(hdr, addr, wdata)
        now = time.time()
        ok = st == est and (rd == erd or not checked)
        note = ""
        if not checked and st == OK:
            # TIMER: must not go back, and must not move more than the
            # host time from issuing the previous read to this one's
            # return (the JTAG polling delay sits inside that window).
            if timer_prev is not None:
                dt_us = (rd - timer_prev) & 0xFFFFFFFF
                wall_us = (now - t_prev) * 1e6
                note = f"  +{dt_us} us (host window {wall_us:.0f} us)"
                if dt_us >= 0x80000000 or dt_us > wall_us + 1000:
                    ok = False
            timer_prev, t_prev = rd, t_issue
        if "DISK_CMD 1" in name:
            time.sleep(0.02)   # the sector read finishes long before this
        fails += 0 if ok else 1
        rows.append({"name": name, "hdr": hdr, "addr": addr, "wdata": wdata,
                     "want_status": est, "want_rdata": erd if checked else None,
                     "got_status": st, "got_rdata": rd, "ok": ok})
        mark = "ok  " if ok else "FAIL"
        print(f"  {mark} {name:42s} {ST_NAME.get(st, hex(st)):10s} {rd:08X}"
              + ("" if ok else f"   want {ST_NAME.get(est)} {erd:08X}") + note)
    stats = bench.pico_stats()
    p = bench.probe()
    c = bench.counts()
    ovl_ev = (c["ovl_events"] - p0["c0_ovl_events"]) & 0xFFFF
    ovl_long = c["ovl_long"] - p0["c0_ovl_long"]
    counts_ok = (stats["tx"] == model.tx and stats["faults"] == model.faults
                 and stats["viol"] == 0 and stats["running"] == 1 and ovl_long == 0
                 and stats["con_deliv"] == model.con_deliv and stats["con_echo"] == model.con_echo
                 and stats["con_bad"] == 0 and stats["disk_r"] == 1 and stats["disk_err"] == 1)
    print(f"  Pico: tx={stats['tx']} faults={stats['faults']} viol={stats['viol']} "
          f"console in={stats['con_deliv']} echo={stats['con_echo']} bad={stats['con_bad']} "
          f"disk reads={stats['disk_r']} err={stats['disk_err']}  DE10 overlaps: "
          f"{ovl_ev} events, {ovl_long} long  "
          f"model tx={model.tx} faults={model.faults}  " + ("ok" if counts_ok else "FAIL"))
    return {"label": label, "hz": hz, "rows": rows, "fails": fails, "pico": stats,
            "de10_viol_samples": (p["viol"] - p0["viol"]) & 0xFFFF, "ovl_events": ovl_ev,
            "ovl_long": ovl_long, "pass": fails == 0 and counts_ok}


def run_stress(bench, div, dly, mask, ram_words, flash_words, log2, irq, uncached,
               seconds, max_seconds, label, hang_capture=False, poll=0.5, log_every=None,
               profile=False, clk="4 0", drive="4 0", pico_extra=(), stop_on_err=True,
               frame_checks=True):
    """One stress run, stopped on the first error (unless stop_on_err is
    False: then it runs on and errors are counted). Rates are measured over
    the window after the RAM fill; the core 1 profile (if asked) too."""
    hz, p0 = bench.restart(div, dly, irq, uncached, clk, drive)
    for c in pico_extra:
        bench.pico.cmd(c)
    cm_bounds = bench.clkmon_start(div)
    seed = random.getrandbits(32)
    bench.set_src(stop_err=1 if stop_on_err else 0, mask=mask, seed=seed, fseed=FSEED,
                  ram_words=ram_words, flash_words=flash_words, log2=log2)
    bench.set_src(stress=1)
    fill = ram_words if (mask & 0xF) else 0
    t0 = time.time()
    first = None
    last = None
    stop_reason = "target"
    stall_polls = 0
    prev_tx = 0          # restart checked that the DE10 count is 0
    hang = None
    tx_total = 0         # running total; the 32-bit counter wraps in hours
    ev_total = 0         # overlap events, from a 16-bit wrapping counter
    prev_ev = p0["c0_ovl_events"]
    next_log = t0 + log_every if log_every else None
    while True:
        time.sleep(poll)
        a = bench.probe()
        cc = bench.counts()
        now = time.time()
        tx_total += (a["tx"] - prev_tx) & 0xFFFFFFFF
        ev_total += (cc["ovl_events"] - prev_ev) & 0xFFFF
        prev_ev = cc["ovl_events"]
        if first is None and (fill == 0 or tx_total >= fill + 16):
            first = (a, cc, now, tx_total)
            if profile:
                bench.pico.cmd("p 1")
        if last is None or tx_total != last[2]:
            last = (a, now, tx_total)   # the rate window ends at the last progress
        if not a["stress_active"]:
            stop_reason = "stopped by itself"
            break
        if (stop_on_err and a["err"]) or cc["ovl_long"] != p0["c0_ovl_long"]:
            stop_reason = "error"
            break
        if a["tx"] == prev_tx:
            stall_polls += 1
            if stall_polls * poll >= max(2.0, 2 * poll):
                stop_reason = "hang"
                break
        else:
            stall_polls = 0
        prev_tx = a["tx"]
        if next_log and now >= next_log:
            st = bench.pico_stats()
            print(f"    {time.strftime('%H:%M:%S')} {(now - t0) / 3600:6.2f} h  tx={tx_total}  "
                  f"err={a['err']}  overlaps={ev_total}/"
                  f"{cc['ovl_long'] - p0['c0_ovl_long']} long  Pico viol={st['viol']}  sectors(DE10)={cc['sectors']}  "
                  f"irq={st['irq']} max {st['irq_max_us']} us  link faults={link_str(bench, p0)}  "
                  f"rst_glitch={(a['rst_short'] - p0['rst_short']) & 0xFFFFFFFF} "
                  f"ack_glitch={(a['ack_glitch'] - p0['ack_glitch']) & 0xFFFFFFFF}",
                  flush=True)
            next_log = now + log_every
        if first is not None and now - first[2] >= seconds:
            break
        if now - t0 >= max_seconds:
            stop_reason = "time limit"
            break
    prof = None
    if profile and first is not None:
        prof = bench.profile()
        bench.pico.cmd("p 0")
    if stop_reason == "hang" and hang_capture:
        hang = {"de10": bench.probe(), "counts": bench.counts(), "pico": bench.pico_stats(),
                "trace": bench.trace(), "glitches": bench.glitches()}
        hang["bus"] = decode_bus_dbg(hang["de10"]["bus_dbg"])
        hang["seq"] = decode_seq(hang["de10"]["seq"])
    bench.set_src(stress=0)
    end = time.time() + 2.0
    idle_ok = False
    while time.time() < end:
        a = bench.probe()
        if not a["stress_active"] and a["bus_idle"]:
            idle_ok = True
            break
        time.sleep(0.05)
    if not idle_ok and stop_reason == "target":
        stop_reason = "bus hung at stop"
    a = bench.probe()
    c = bench.counts()
    st = bench.pico_stats()
    cm = bench.clkmon()
    cm["bounds"] = list(cm_bounds)
    if hang is not None:
        hang["clkmon"] = cm
    if hang is None and stop_reason in ("hang", "bus hung at stop", "error"):
        hang = {"de10": a, "counts": c, "pico": st, "trace": bench.trace(),
                "glitches": bench.glitches(),
                "bus": decode_bus_dbg(a["bus_dbg"]), "seq": decode_seq(a["seq"])}

    res = {"label": label, "div": div, "tx_total": tx_total, "hz": hz, "dly": list(dly), "mask": mask,
           "ram_words": ram_words, "flash_words": flash_words, "log2": log2,
           "irq": irq, "uncached": uncached, "seed": seed, "stop_reason": stop_reason,
           "clk": clk, "drive": drive, "de10": a, "counts": c, "pico": st, "wall_seconds": time.time() - t0,
           "profile": prof, "clkmon": cm}
    if first is not None:
        res["seconds_to_stop"] = (last[1] - t0) if last else 0.0
    if first is not None and last is not None:
        fa, fc, ft, ftot = first
        la, lt, ltot = last
        dcyc = la["cycles"] - fa["cycles"]
        dtx = ltot - ftot
        drt = la["rt"] - fa["rt"]
        secs = dcyc / hz if hz else 0.0
        res["window"] = {
            "seconds": secs, "tx": dtx, "round_trips": drt,
            "tx_per_s": dtx / secs if secs else 0.0,
            "ns_per_tx": 1e9 * secs / dtx if dtx else 0.0,
            "ns_per_round_trip": 1e9 * secs / drt if drt else 0.0,
            "clocks_per_round_trip": dcyc / drt if drt else 0.0,
            "round_trips_per_tx": drt / dtx if dtx else 0.0,
        }
        res["window"]["sectors_per_s"] = ((c["sectors"] - fc["sectors"]) / secs) if secs else 0.0
    w = res.get("window", {})
    resets = (a["rst_count"] - p0["rst_count"]) & 0xFFFF
    res["resets_during_run"] = resets
    res["rst_glitches_rejected"] = (a["rst_short"] - p0["rst_short"]) & 0xFFFFFFFF
    res["ack_glitches_rejected"] = (a["ack_glitch"] - p0["ack_glitch"]) & 0xFFFFFFFF
    res["viol_samples"] = (a["viol"] - p0["viol"]) & 0xFFFF
    res["ovl_events"] = ev_total + ((c["ovl_events"] - prev_ev) & 0xFFFF)
    res["ovl_long"] = c["ovl_long"] - p0["c0_ovl_long"]
    res["txmax_us"] = a["txmax"] / hz * 1e6 if hz else 0.0
    lk = bench.link()
    res["link"] = {k: (v - p0["l0_" + k]) & 0xFFFFFFFF for k, v in lk.items()}
    fr = bench.frame()
    bal = (fr["balance"] - p0["f0_balance"]) & 0xFFFFFFFF
    res["frame"] = {"rises": (fr["rises"] - p0["f0_rises"]) & 0xFFFFFFFF,
                    "balance": bal - (1 << 32) if bal >= (1 << 31) else bal,
                    "req_out": fr["req_out"] - p0["f0_req_out"],
                    "oe_out": fr["oe_out"] - p0["f0_oe_out"]}
    # err counts every failed transaction, LINK_ERROR included
    res["data_errors"] = a["err"] - res["link"]["link_errors"]
    res["hang"] = hang
    checks = {
        "reached target": stop_reason == "target",
        "no data errors": res["data_errors"] == 0,
        "no long overlap (DE10)": res["ovl_long"] == 0,
        "no safety-net violation (Pico)": st["viol"] == 0,
        "Pico server running": st["running"] == 1,
        "same transaction count": st["tx"] == a["tx"],
        "faults only where expected": st["faults"] == c["fault"],
        "every interrupt answered once": st["irq"] == c["irq_ack"],
        "interrupts raised and served": (not irq) or (st["irq"] > 0 and st["irq_raised"] - st["irq"] <= 1),
        "console echo clean": st["con_bad"] == 0,
        "disk writes verified": st["disk_bad"] == 0 and st["disk_err"] == 0,
        "no reset during the run": resets == 0,
        "no LINK_ERROR": res["link"]["link_errors"] == 0,
    }
    if frame_checks:
        checks["frame: every REQ change inside frame"] = res["frame"]["req_out"] == 0
        checks["frame: uio_oe only inside frame"] = res["frame"]["oe_out"] == 0
        checks["frame: every rise a start or a retry"] = res["frame"]["balance"] == 0
        checks["frame: one rise per transaction at least"] = res["frame"]["rises"] >= tx_total
    res["checks"] = checks
    res["pass"] = all(checks.values())
    failed = [k for k, v in checks.items() if not v]
    print(f"  {label:58s} {mhz(hz):>10s} dly {dly[0]}/{dly[1]}  tx={a['tx']:>10d}  "
          f"{w.get('tx_per_s', 0) / 1e3:8.1f} k tx/s  {w.get('ns_per_tx', 0):7.0f} ns/tx  "
          f"{w.get('ns_per_round_trip', 0):6.1f} ns/rt  {w.get('round_trips_per_tx', 0):5.2f} rt/tx  "
          f"ovl {res['ovl_events']}/{res['ovl_long']}  reqgl {st['req_gl_hi']}/{st['req_gl_lo']}  "
          f"link {res['link']['timeouts']}/{res['link']['bad_answers']}/{res['link']['bad_states']}/"
          f"{res['link']['link_errors']}  data err {res['data_errors']}  "
          f"frame {res['frame']['rises']}/{res['frame']['balance']}/{res['frame']['req_out']}/"
          f"{res['frame']['oe_out']}  "
          f"CLK wire {cm['short']}/{cm['long']} per {cm['per_min']}-{cm['per_max']} "
          f"hi/lo>={cm['high_min']}/{cm['low_min']}  "
          + ("PASS" if res["pass"] else f"FAIL ({stop_reason}; {', '.join(failed)})"), flush=True)
    if a["e_valid"]:
        print(f"    first error: {E_KIND.get(a['e_kind'], a['e_kind'])} hdr {a['e_hdr']:02X} "
              f"addr {a['e_addr']:06X} status {ST_NAME.get(a['e_status'], hex(a['e_status']))} "
              f"want {a['e_exp']:08X} got {a['e_got']:08X}")
    if hang and cm["anomaly_s_ago"] is not None:
        print(f"    last CLK anomaly {cm['anomaly_s_ago']:.3f} s ago, last REQ change {cm['req_change_s_ago']:.3f} s ago")
    if hang:
        print(f"    state at stop: DE10 {hang['bus']}; {hang['seq']}; "
              f"Pico phase={hang['pico']['phase']} pio_pc={hang['pico']['pio_pc']} "
              f"flevel={hang['pico']['flevel']:x} pins={hang['pico']['pins']:04x}")
    return res


def summarize_speed(res):
    p = res["pico"]
    w = res.get("window", {})
    extra = []
    if res["mask"] & 0x200:
        extra.append(f"{w.get('sectors_per_s', 0):.1f} sectors/s verified, "
                     f"RP write max {p['disk_wmax_us']} us, read max {p['disk_rmax_us']} us")
    if res["irq"]:
        n = max(p["irq"], 1)
        extra.append(f"IRQ latency mean {p['irq_sum_us'] / n:.1f} us, max {p['irq_max_us']} us over {p['irq']}")
    if res["mask"] & 0x100:
        extra.append(f"echoed {p['con_echo']} bytes")
    extra.append(f"Pico: replays {p['replay']}, recoveries {p['resync']} (timeouts {p['rp_to']}, "
                 f"stale {p['stale']}, safety net {p['viol']}), ACK repairs {p['ackfix']}, injected {p['inj']}, "
                 f"T_rp {p['t_rp_us']} us")
    extra.append(f"longest transaction {res['txmax_us']:.1f} us")
    pr = res.get("profile")
    if pr and pr.get("n"):
        n = pr["n"]
        extra.append(f"core 1 cycles per tx: wait {pr['idle'] / n:.0f}, receive {pr['recv'] / n:.0f}, "
                     f"decide {pr['decide'] / n:.0f}, answer {pr['answer'] / n:.0f}")
    print("    " + "; ".join(extra))


FAULT_MODES = [
    (1, "drop the transaction before doing it", "timeouts", False),
    (2, "answer with the wrong SEQ", "bad_answers", True),
    (3, "stall after the header", "timeouts", False),
    (4, "do it, lose the answer", "timeouts", True),
    (5, "do it, answer too late (stale check)", "timeouts", True),
]


def phase_faults(bench, div, args):
    """Faults injected by the Pico, one in every N transactions, full mix
    at the top clock. Each must cost one chip-side fault of the expected
    kind, be retried, and (where the Pico had done the access) be answered
    from its copy; nothing else may change."""
    every = args.fault_every
    secs = args.fault_seconds
    print(f"Fault injection at {mhz(SYS_HZ / div)}, delays {TT_DELAY}, full mix, "
          f"one fault in every {every} transactions, {secs:g} s per mode")
    out = {"modes": [], "pass": True}
    for mode, name, kind, replays in FAULT_MODES:
        res = run_stress(bench, div, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4, True, 0,
                         secs, secs + 60, f"fault {mode}: {name}", hang_capture=True,
                         pico_extra=[f"F {mode} {every}"])
        p, lk = res["pico"], res["link"]
        others = sum(v for k, v in lk.items() if k not in (kind, "link_errors"))
        checks = {
            "faults were injected": p["inj"] >= 100,
            "one chip fault per injection": lk[kind] == p["inj"],
            "no other link fault": others == 0,
            "answered from the copy": (p["replay"] == p["inj"]) if replays else (p["replay"] == 0),
            "ACK repaired where needed": p["ackfix"] == (0 if mode == 2 else p["inj"]),
        }
        res["fault_checks"] = checks
        ok = res["pass"] and all(checks.values())
        res["pass"] = ok
        print(f"    injected {p['inj']}, chip {kind} {lk[kind]}, other link faults {others}, "
              f"LINK_ERROR {lk['link_errors']}, replays {p['replay']}, Pico recoveries {p['resync']} "
              f"(timeouts {p['rp_to']}, stale {p['stale']}), ACK repairs {p['ackfix']}  "
              + ("PASS" if ok else "FAIL (" + ", ".join(k for k, v in checks.items() if not v) + ")"),
              flush=True)
        out["modes"].append(res)
        out["pass"] &= ok
    # A dead Pico: every transaction dropped. ROM reads only (no state that
    # a dropped write would spoil), keep going past errors: each
    # transaction must end with LINK_ERROR after four attempts (a few ms
    # each), never hang, and be the only kind of error; once the Pico
    # answers again, no new errors; then a clean full-mix run.
    hz, p0 = bench.restart(div, TT_DELAY)
    bench.set_src(stop_err=0, mask=0x010, seed=random.getrandbits(32), fseed=FSEED, ram_words=0,
                  flash_words=FLASH_WORDS, log2=4)
    bench.pico.cmd("F 1 1")
    bench.set_src(stress=1)
    time.sleep(3.0)
    bench.pico.cmd("F 0 0")
    time.sleep(0.2)
    a1 = bench.probe()
    time.sleep(1.0)
    a2 = bench.probe()
    lk2 = bench.link()
    bench.set_src(stress=0)
    time.sleep(0.2)
    le = (lk2["link_errors"] - p0["l0_link_errors"]) & 0xFFFFFFFF
    moved = (a2["tx"] - a1["tx"]) & 0xFFFFFFFF
    dead_ok = le >= 50 and a2["err"] == le and a2["err"] == a1["err"] and moved > 1000
    print(f"  dead Pico for 3 s: {le} transactions ended with LINK_ERROR, {a2['err']} errors counted, "
          f"then {moved} transactions and {a2['err'] - a1['err']} errors in 1 s once it answers again  "
          + ("PASS" if dead_ok else "FAIL"), flush=True)
    bench.set_src(stop_err=1)
    clean = run_stress(bench, div, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4, True, 0,
                       10, 70, "after the dead Pico: full mix", hang_capture=True)
    out["dead"] = {"link_errors": le, "err": a1["err"], "tx_after": moved, "pass": dead_ok}
    out["clean_after"] = clean
    out["pass"] &= dead_ok and clean["pass"]
    return out


# The tear test's states: (T4A tear_sel, name, whether the Pico has always
# done the access by then, so every retry is answered from its copy). The
# Pico does the access as soon as the last byte is in, before the handoff:
# a tear in the send phase on the last byte is answered from the copy too.
TEAR_STATES = [
    (0, "SEND_TOGGLE", False),
    (1, "SEND_WAIT", False),
    (2, "HANDOFF", True),
    (3, "RECV_TOGGLE", True),
]


def phase_tear(bench, div, args):
    """The torn clock edge of the torture test, on demand: the DE10 loads
    IDLE into ab_bus's state register for one clock, in the middle of one
    transaction in every 2**tear_log2, in the chosen state. ab_bus's open
    check must turn each tear into one bad-state fault and one retry; the
    full mix runs on at the top clock with no error and no LINK_ERROR."""
    secs = args.tear_seconds
    n = args.tear_log2
    print(f"Tear test at {mhz(SYS_HZ / div)}, delays {TT_DELAY}, full mix, "
          f"one tear in every {1 << n} transactions, {secs:g} s per state")
    out = {"states": [], "pass": True}
    try:
        for sel, name, replays in TEAR_STATES:
            bench.set_src(tear_on=1, tear_sel=sel, tear_log2=n)
            t0 = bench.tears()
            res = run_stress(bench, div, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4, True, 0,
                             secs, secs + 60, f"tear in {name}", hang_capture=True)
            bench.set_src(tear_on=0)
            torn = (bench.tears() - t0) & 0xFFFFFFFF
            p, lk = res["pico"], res["link"]
            # A tear on the last byte of the read data comes after the Pico
            # has seen that byte taken: it releases uio, hands back, and
            # counts the transaction; the retry is answered from the copy
            # and counted again. Every other tear leaves the Pico waiting
            # until its timeout. So each tear is exactly one of the two.
            extra = p["tx"] - res["de10"]["tx"]
            checks = {
                "tears applied": torn >= 1000,
                "one bad-state fault per tear": lk["bad_states"] == torn,
                "no other link fault": lk["timeouts"] == 0 and lk["bad_answers"] == 0,
                "answered from the copy": (p["replay"] == torn) if replays else (p["replay"] <= torn),
                "each tear one Pico timeout or one finished transaction":
                    p["rp_to"] + extra == torn and (extra == 0 or sel == 3),
                "no ACK repair needed": p["ackfix"] == 0,
            }
            res["tears"] = torn
            res["tear_checks"] = checks
            base = {k: v for k, v in res["checks"].items() if k != "same transaction count"}
            ok = all(base.values()) and all(checks.values())
            res["pass"] = ok
            failed = [k for k, v in base.items() if not v] + [k for k, v in checks.items() if not v]
            print(f"    tears {torn}, bad states {lk['bad_states']}, timeouts {lk['timeouts']}, "
                  f"bad answers {lk['bad_answers']}, LINK_ERROR {lk['link_errors']}, replays {p['replay']}, "
                  f"Pico recoveries {p['resync']} (timeouts {p['rp_to']}, stale {p['stale']}), "
                  f"Pico extra count {extra}, ACK repairs {p['ackfix']}  "
                  + ("PASS" if ok else "FAIL (" + ", ".join(failed) + ")")
                  + ("  (the line above checks Pico count = DE10 count; here it is checked as above)"
                     if extra else ""),
                  flush=True)
            out["states"].append(res)
            out["pass"] &= ok
    finally:
        bench.set_src(tear_on=0)
    return out


def phase_torture(bench, args):
    """The chip clock from the Pico's CLK jumper wire, which picks up
    extra edges from the DE10's own pin switching (the 25 MHz hangs of
    Tests 3 and 4). A measurement: clock glitches corrupt the chip side's
    state now and then; protocol v1 should turn most of that into link
    faults that are retried. Data errors are counted, not hidden."""
    secs = args.torture_seconds
    out = []
    print(f"Torture: chip clock from the Pico's CLK wire, delays {TT_DELAY}, full mix, {secs:g} s")
    try:
        bench.set_src(stress=0)
        bench.pico.cmd("r 0")
        bench.set_ck_sel(1)
        # The Pico recovers from safety-net violations as devkit firmware
        # would, and the run goes on through errors so they are counted.
        res = run_stress(bench, DIV_25, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4, True, 0,
                         secs, secs + 60, "torture: CLK wire clock, full mix", hang_capture=True,
                         pico_extra=("V 1",), stop_on_err=False, frame_checks=False)
        summarize_speed(res)
        out.append(res)
    finally:
        bench.set_src(stress=0, stop_err=1)
        bench.pico.ask("r 0")
        bench.pico.ask("V 0")
        bench.set_ck_sel(0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phases", nargs="+",
                    choices=["prep", "manual", "matrix", "speed", "long25", "soak", "diag25", "clkidle",
                             "pscale", "faults", "torture", "tear"])
    ap.add_argument("--div", type=int, default=0, help="top clock divider (default: fastest passing, else 3)")
    ap.add_argument("--hours", type=float, default=1.0)
    ap.add_argument("--speed-seconds", type=float, default=8.0)
    ap.add_argument("--matrix-seconds", type=float, default=15.0)
    ap.add_argument("--long-seconds", type=float, default=600.0)
    ap.add_argument("--diag-seconds", type=float, default=120.0)
    ap.add_argument("--diag-reps", type=int, default=2)
    ap.add_argument("--diag-only", default="", help="comma list of diag variant numbers to run")
    ap.add_argument("--fault-seconds", type=float, default=30.0)
    ap.add_argument("--fault-every", type=int, default=500)
    ap.add_argument("--torture-seconds", type=float, default=60.0)
    ap.add_argument("--tear-seconds", type=float, default=30.0)
    ap.add_argument("--tear-log2", type=int, default=9, help="one tear in every 2**n transactions")
    args = ap.parse_args()

    pico = Pico()
    ident = pico.cmd("i")
    if "bench_mem" not in ident:
        raise BenchError(f"the Pico is not running bench/firmware: {ident}")
    de10 = De10("T4A", 200, 464, extra={"T4B": (1, 496), "T4C": (26, 480)})
    bench = Bench(pico, de10)
    out = {"started": time.strftime("%Y-%m-%d %H:%M:%S"), "pico": ident, "args": vars(args)}
    stamp = time.strftime("%Y%m%d-%H%M%S")
    rdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
    os.makedirs(rdir, exist_ok=True)
    path = os.path.join(rdir, f"memtest-{stamp}-{'-'.join(args.phases)}.json")

    def save():
        with open(path, "w") as f:
            json.dump(out, f, indent=1)

    top_div = args.div or 3
    ok_all = True
    try:
        bench.set_src(stress=0, stop_err=1)
        # The ROM copy must hold h(o, FSEED) for every phase.
        out["prep"] = phase_prep(bench)
        ok_all &= out["prep"]["pass"]
        save()
        if "manual" in args.phases:
            out["manual_slow"] = run_manual(bench, MANUAL_DIV, "1 MHz")
            ok_all &= out["manual_slow"]["pass"]
            save()
        if "matrix" in args.phases:
            print(f"Matrix: full mix (every memory and device, interrupts), "
                  f"{args.matrix_seconds:g} s per point, stop on first error")
            out["matrix"] = []
            for dly in MATRIX_DELAYS:
                for div in MATRIX_DIVS:
                    res = run_stress(bench, div, dly, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4,
                                     True, 0, args.matrix_seconds, args.matrix_seconds + 30,
                                     f"full mix", hang_capture=True)
                    out["matrix"].append(res)
                    save()
                    if not res["pass"]:
                        break
            tt = [r for r in out["matrix"] if r["dly"] == list(TT_DELAY) and r["pass"]]
            if not args.div and tt:
                top_div = min(r["div"] for r in tt)
            ok_all &= all(r["pass"] for r in out["matrix"])
        out["top_div"] = top_div
        if "manual" in args.phases:
            out["manual_top"] = run_manual(bench, top_div, "top clock")
            ok_all &= out["manual_top"]["pass"]
            save()
        if "speed" in args.phases:
            print(f"Speed, one memory type at a time, at {mhz(SYS_HZ / top_div)} with delays {TT_DELAY}")
            out["speed"] = []
            for name, mask, rw, fw, log2, irq, unc in SPEED:
                secs = args.speed_seconds * (2 if rw == ALL_USER_WORDS else 1)
                res = run_stress(bench, top_div, TT_DELAY, mask, rw, fw, log2, irq, unc,
                                 secs, secs + 60, name, hang_capture=True, profile=True)
                summarize_speed(res)
                out["speed"].append(res)
                ok_all &= res["pass"]
                save()
        if "long25" in args.phases:
            print(f"Long run at 25 MHz, delays {TT_DELAY}, full mix, {args.long_seconds:g} s")
            res = run_stress(bench, DIV_25, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4,
                             True, 0, args.long_seconds, args.long_seconds + 120,
                             "long run, full mix", hang_capture=True, log_every=60)
            summarize_speed(res)
            out["long25"] = res
            ok_all &= res["pass"]
            save()
        if "pscale" in args.phases:
            print(f"RP clock scaling at chip {mhz(SYS_HZ / top_div)}, delays {TT_DELAY}, "
                  f"{args.speed_seconds:g} s per point")
            out["pscale"] = []
            picks = [SPEED[0], SPEED[2], ("full mix", MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4, True, 0)]
            try:
                for khz in (100000, 150000, 200000):
                    bench.pico.cmd("r 0")
                    got = int(bench.pico.cmd(f"S {khz}").split()[2])
                    bench.rp_hz = got
                    for name, mask, rw, fw, log2, irq, unc in picks:
                        res = run_stress(bench, top_div, TT_DELAY, mask, rw, fw, log2, irq, unc,
                                         args.speed_seconds, args.speed_seconds + 60,
                                         f"RP {got / 1e6:g} MHz: {name}", hang_capture=True, profile=True)
                        summarize_speed(res)
                        res["rp_hz"] = got
                        out["pscale"].append(res)
                        ok_all &= res["pass"]
                        save()
            finally:
                bench.pico.ask("r 0")
                bench.pico.ask("S 150000")
                bench.rp_hz = SYS_HZ
        if "clkidle" in args.phases:
            print("CLK monitor with the bus quiet (chip out of reset, no transactions), 10 s each")
            out["clkidle"] = []
            for div in (15, 6, 5, 4, 3):
                hz, _ = bench.restart(div, TT_DELAY, False, 0)
                bench.clkmon_start(div)
                time.sleep(10)
                cm = bench.clkmon()
                cm.update({"div": div, "hz": hz})
                out["clkidle"].append(cm)
                print(f"  {mhz(hz):>10s}  periods {cm['periods']}  short {cm['short']}  long {cm['long']}  "
                      f"per {cm['per_min']}-{cm['per_max']}  high/low >= {cm['high_min']}/{cm['low_min']}", flush=True)
                save()
            ok_all = False   # a measurement, not a pass/fail phase
        if "diag25" in args.phases:
            print(f"25 MHz diagnosis: full mix until the first failure, at most "
                  f"{args.diag_seconds:g} s, {args.diag_reps} runs per variant")
            out["diag25"] = []
            only = {int(x) for x in args.diag_only.split(",") if x}
            for k, (name, div, dly, clk, dirq, *more) in enumerate(DIAG):
                drive = more[0] if more else "4 0"
                if only and k not in only:
                    continue
                for rep in range(args.diag_reps):
                    res = run_stress(bench, div, dly, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4,
                                     dirq, 0, args.diag_seconds, args.diag_seconds + 60,
                                     f"[{k}] {name}", hang_capture=True, clk=clk, drive=drive)
                    res["variant"] = k
                    out["diag25"].append(res)
                    save()
            ok_all = False   # a diagnosis, not a pass/fail phase
        if "faults" in args.phases:
            out["faults"] = phase_faults(bench, top_div, args)
            ok_all &= out["faults"]["pass"]
            save()
        if "tear" in args.phases:
            out["tear"] = phase_tear(bench, top_div, args)
            ok_all &= out["tear"]["pass"]
            save()
        if "torture" in args.phases:
            out["torture"] = phase_torture(bench, args)
            # A measurement, so data errors and LINK_ERRORs under the
            # corrupted clock are reported, not judged. A hang is judged:
            # the bus unit must always end a transaction.
            ok_all &= all(r["checks"]["reached target"] for r in out["torture"])
            save()
        if "soak" in args.phases:
            secs = args.hours * 3600
            print(f"Soak at {mhz(SYS_HZ / top_div)}, delays {TT_DELAY}, full mix, {args.hours:g} h")
            res = run_stress(bench, top_div, TT_DELAY, MASK_ALL, STRESS_WORDS, FLASH_WORDS, 4,
                             True, 0, secs, secs + 600, "soak, full mix", hang_capture=True,
                             poll=2.0, log_every=300)
            summarize_speed(res)
            out["soak"] = res
            ok_all &= res["pass"]
            save()
        out["pass"] = bool(ok_all)
    finally:
        bench.safe()
        de10.close()
        pico.close()
        out["finished"] = time.strftime("%Y-%m-%d %H:%M:%S")
        save()
    print("RESULT:", "PASS" if out.get("pass") else "FAIL", "-", path)
    return 0 if out.get("pass") else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BenchError as e:
        print("BENCH ERROR:", e)
        sys.exit(2)
