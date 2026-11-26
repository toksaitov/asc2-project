Almurut II Bus Protocol v0
==========================

Copied on 2026-11-22 from the bench directory of Almurut I (`asc1/bench/`), where the protocol was developed and tested. The file names in the text are those of the bench: `chip-rtl/ab_bus.sv`, `ab_irq.sv`, and `ab_reset.sv` are [asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv), [asc_irq_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_irq_filter.sv), and [asc_reset_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_reset_filter.sv) in the chip's repository, and the other bench files move into [asc2-bridge/](../asc2-bridge/) in phase 7 of the plan. Status: approved on 2026-11-21 with all five recommended decisions (24-bit address, status on writes, NEXT for fetches, little-endian, no bursts in v0).

This bus connects the chip to the RP (the RP2040 Pico on the bench, the RP2350 on the Tiny Tapeout devkit). The RP serves all memory and all devices. The chip has no RAM of its own: every instruction fetch and every load or store is a bus transaction.

## Signals

| Signal | Driven by | Devkit pin | Bench DE10 pin | Bench Pico pin |
| --- | --- | --- | --- | --- |
| `uio[7:0]` | whoever owns the bus | uio[0..7], GPIO25-32 | 8 to 24 | GP2 to GP9 |
| `REQ` | chip | uo_out[0], GPIO33 | 28 | GP10 |
| `ACK` | RP | ui_in[0], GPIO17 | 32 | GP11 |
| `IRQ` | RP | ui_in[1], GPIO18 | 34 | GP12 |
| `RST_N` | RP | rst_n, GPIO14 | 2 | GP0 |
| `CLK` | RP | clk, GPIO16 | 4 | GP1 |

The debug port (DBG_CK, DBG_DI, DBG_DO) is separate and not part of this protocol. The devkit pins ui_in[3] and uo_out[4] stay free for the devkit's UART. The other unused devkit pins (ui_in[5..7], uo_out[2,3,5..7]) are spare for later versions.

## Handshake

The bus is asynchronous. Neither side assumes anything about the other's clock, so it works at any speed and on the bench and the chip alike.

* Two-phase signaling: every change of `REQ` or `ACK` is one event, rising or falling alike. This needs half the edges of a return-to-zero handshake.
* One byte moves per event pair. The owner puts the byte on `uio`, then toggles its strobe (`REQ` for the chip, `ACK` for the RP). The other side sees the toggle, takes the byte, and toggles its own strobe back. The owner may change `uio` only after that answer.
* A sender sets the byte at least one of its own clock cycles before toggling. A receiver passes the strobe through a two-flop synchronizer before it reads `uio`.
* The chip also filters `ACK`: it counts an event only when two consecutive synchronized samples agree. A pulse shorter than one clock (crosstalk on the wire) is ignored, at the cost of one clock per event. Added after Test 3, where fake `ACK` events broke the bus at 41.7 MHz and above. The RP side takes `REQ` through the PIO input synchronizer without a filter.

## Who drives uio

Only the owner drives `uio`, and ownership changes hands only one way: the owner first turns its `uio` outputs off, at least one clock cycle before it toggles its strobe. The other side drives only after it has seen that toggle. This is the same rule the Test 2 interlock checked on the bench, but carried by `REQ` and `ACK` instead of two extra wires.

* After reset the chip owns the bus but drives nothing until it starts a transaction.
* The RP keeps weak pull-downs on `uio`, so the bus never floats while nobody drives it.

## Transactions

Every transaction has the same shape, which keeps the chip's bus logic to one small state machine.

```
chip sends:  HDR  [A0 A1 A2]  [D0 .. Dn-1]     address unless NEXT; data only for a write
handoff:     chip turns uio off, toggles REQ
RP sends:    STAT [D0 .. Dn-1]                 data only for a read that succeeded
handback:    RP turns uio off, toggles ACK
```

Addresses and data travel least significant byte first, so the bus is little-endian. Header byte `HDR`:

| Bits | Field | Meaning |
| --- | --- | --- |
| 7 | `write` | 0 read, 1 write |
| 6 | `next` | 1: no address bytes; the address continues where the previous transaction of the same kind (fetch or data) ended |
| 5:4 | `size` | 00 = 1 byte, 01 = 2 bytes, 10 = 4 bytes, 11 = reserved |
| 3 | `fetch` | 1: instruction fetch, 0: data access |
| 2 | `kernel` | 1: the CPU is in kernel mode |
| 1:0 | reserved | 0; could later carry address bits 25:24 |

The address is 24 bits (16 MB). Addresses must be aligned to the access size.

Status byte `STAT`:

| Code | Name | Meaning |
| --- | --- | --- |
| 0x00 | OK | done; a read's data follows |
| 0x01 | BUS_ERROR | nothing at this address |
| 0x02 | PROTECTION | kernel-only address used in user mode, or a fetch from a no-execute region |
| 0x03 | MISALIGNED | the address is not aligned to the size |
| 0x04 | BAD_NEXT | `next` used with no valid previous address (after reset or a fault) |

Any status other than OK is a precise fault: the chip traps on the instruction that made the access. The `fetch` and `kernel` bits are what an RP-side memory map can use to enforce protection later, without an MMU on the chip.

## Example: a sequential 4-byte instruction fetch (NEXT)

```
step  strobe      uio carries    meaning
  1   REQ toggle  HDR (chip)     read, next, size 4, fetch
  2   ACK toggle  -              header taken
  3   REQ toggle  released       handoff to the RP
  4   ACK toggle  STAT (RP)      0x00 OK
  5   REQ toggle  -              status taken
  6   ACK toggle  D0 (RP)
  7   REQ toggle  -
  ... D1, D2, D3 the same way ...
 13   REQ toggle  -              D3 taken
 14   ACK toggle  released       handback; the chip may start the next transaction
```

## Cost in round trips

One round trip is one byte or handover moved: one strobe toggle and its answer.

| Transaction | Round trips |
| --- | --- |
| 4-byte fetch, NEXT | 7 |
| 4-byte read, full address | 10 |
| 4-byte write, full address | 10 |
| 1-byte read, full address | 7 |

Test 3 measures how long a round trip takes on the bench. For example, at 100 ns per round trip a sequential fetch takes 0.7 us, about 1.4 million instructions per second when every instruction is fetched in sequence.

## Reset, interrupts, memory map

* **Reset:** the RP holds `RST_N` low until the boot image is in its RAM, `uio` is released and `ACK` is low. The chip resets `REQ` low. The reset PC is an ISA decision; 0x000000 is proposed.
* **Reset filter:** the chip synchronizes `RST_N` and changes its reset state only after the new level has held for 8 clocks, so short glitches neither reset nor release it. Added after Test 3, where `RST_N` glitches reset the chip side mid-run at 12.5 MHz and above. The RP must hold `RST_N` low for at least 8 chip clocks.
* **IRQ:** a level signal, high while any device interrupt is pending. The chip takes it between instructions. The handler finds and clears the cause through device registers.
* **Memory map (proposal, as served in Test 4):**
  * 0x000000 to 0x7FFFFF: RAM. The RP2040 offers about 192 KB, the RP2350 about 400 KB, and up to 8 MB with PSRAM. The top 4 KB served is kernel-only (user accesses get PROTECTION).
  * 0x800000 to 0xFEFFFF: a read-only window onto the RP's flash, for data reads and instruction fetches; writes get BUS_ERROR. Test 4 serves 768 KB of the Pico's flash here.
  * 0xFF0000 to 0xFFFFFF: device registers, 4-byte data accesses only (other sizes get BUS_ERROR, fetches get PROTECTION). Test 4 serves:

    | Address | Register | Access |
    | --- | --- | --- |
    | 0xFF0000 | ID, "ALM2" | read |
    | 0xFF0004 | SCRATCH | read, write |
    | 0xFF0008 | transactions served | read |
    | 0xFF000C | TIMER, microseconds | read |
    | 0xFF0010 | CON_STAT: bit 0 input byte waiting, bit 1 output ready | read |
    | 0xFF0014 | CON_DATA: read the next input byte (0x100 if none), write an output byte | read, write |
    | 0xFF0018 | IRQ_ACK: read the IRQ line, write to drop it | read, write |
    | 0xFF0020 | DISK_SEC, sector number | read, write |
    | 0xFF0024 | DISK_BUF, RAM address of a 4 KB buffer | read, write |
    | 0xFF0028 | DISK_CMD: 1 read the sector into the buffer, 2 write the buffer to the sector | read, write |
    | 0xFF002C | DISK_STAT: bit 0 busy, bit 1 error, bits 31:16 commands done | read |

    The disk is a set of 4 KB flash sectors on the RP. While it is busy the RP owns the buffer; the bus keeps serving RAM and devices, and only reads of the flash window wait until the flash is free again.
  * Everything else is BUS_ERROR.

## Decisions (approved 2026-11-21)

1. **Address width:** 24 bits (16 MB, three address bytes). The two reserved header bits can extend it to 26 bits later without changing the format.
2. **Status on writes:** yes, so a bad store traps precisely, for user and kernel protection. The cost is about 2 extra round trips per write compared with fire-and-forget writes.
3. **NEXT for fetches:** yes. The chip uses it after any instruction that did not branch, which saves 3 of the 10 round trips without an address comparator on the chip.
4. **Byte order:** little-endian. The ISA has to match.
5. **Bursts** (16 bytes per transaction): left for v1. They only pay off with a prefetch buffer on the chip, which costs area.

## Test 3 on the bench

* **DE10:** implements the chip side of this protocol exactly, as the RTL that is meant to go into the chip, plus a test sequencer. The sequencer writes and reads back pseudo-random addresses and data, and also provokes BUS_ERROR, MISALIGNED and BAD_NEXT on purpose.
* **Pico:** implements the RP side: core 1 runs a polling loop from RAM that watches `REQ`, moves the bytes through the SIO registers and serves a 128 KB RAM array and a few device registers, while core 0 talks to the host. A PIO state machine can replace the loop later if the round-trip time needs it. The Pico also provides `CLK`, as the devkit's RP2350 does.
* **Measured:** round-trip time, transactions per second, and errors, at increasing clock speeds. A speed passes with zero errors over at least a million transactions.
