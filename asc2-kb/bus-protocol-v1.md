Almurut II Bus Protocol v1
==========================

Copied on 2026-11-22 from the bench directory of Almurut I (`asc1/bench/`), where the protocol was developed and tested. The file names in the text are those of the bench: `chip-rtl/ab_bus.sv`, `ab_irq.sv`, and `ab_reset.sv` are [asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv), [asc_irq_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_irq_filter.sv), and [asc_reset_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_reset_filter.sv) in the chip's repository, and the other bench files move into [asc2-bridge/](../asc2-bridge/) in phase 7 of the plan.

Status: approved on 2026-11-21 ("bullet-proof, patchable in software"). v1 keeps everything in [v0](bus-protocol-v0.md) (signals, pins, handshake, transaction shape, header fields, status codes, round-trip costs) and adds the rules below so that any single disturbance on the wires, or any mistake or delay on the RP side, costs a retry instead of a hang or a wrong value. Every rule on the RP side is firmware, so it can be changed after the chip is made.

## What can go wrong, and what catches it

| Disturbance | Caught by | Result |
| --- | --- | --- |
| Short glitch on `ACK` | chip: 3-sample filter | ignored |
| Short glitch on `REQ` | RP: PIO takes a level only if it holds 3 cycles (20 ns at 150 MHz) | ignored |
| Short glitch on `RST_N` | chip: 8-clock reset filter (v0) | ignored |
| RP answers late, never, or with garbage | chip: answer check and timeout | retry |
| Chip goes quiet mid-transaction | RP: timeout | RP releases `uio`, waits for the retry |
| An answer is lost after the RP did the access | RP: copy of the last answer | the retry gets the same answer; the access happens once |
| Chip state register upset (an illegal state code) | chip: illegal state is a fault | retry |
| RP dead or unplugged | chip: 4 attempts | LINK_ERROR to the CPU, never a hang |

What v1 cannot catch: glitches on the chip's `CLK` itself (5-15 ns after a real edge they corrupt any synchronous logic), and a wrong value that still passes every check (for example a corrupted address byte on the way to the RP). These are the limits of a protocol without a checksum. The clock is driven by the RP and its frequency is a firmware setting, which is the lever against a noisy clock on the board.

## The address: four bytes (changed on 2026-11-23)

A request with an address carries four address bytes, low byte first, in place of the three of v0: a physical address has 32 bits. A write of a word is nine bytes: the header, four of the address, and four of data. A fetch of "the next word" carries no address, as before. Nothing else of the frame changed. The change was made on 2026-11-23; [machine-contract.md](machine-contract.md) has the rules of the addresses, and the firmware, the emulator, and the test programs changed together with the chip.

## Header: the sequence bit

`HDR` bit 0 is `SEQ`. Bit 1 stays reserved and 0.

* The chip resets `SEQ` to 0.
* The chip flips `SEQ` only when a transaction completes validly (a good status byte, every data byte, and the handback).
* A retry carries the same `SEQ` as the attempt it repeats.

## Status byte

```
STAT = { SEQ, ~SEQ, 0, 0, 0, code[2:0] }
```

* `code` is the v0 status code, 0 to 4.
* With `SEQ` 0 an OK is 0x40; with `SEQ` 1 it is 0x80. 0x00 and 0xFF are never valid, so a floating or stuck bus never reads as a good answer.
* The chip checks every status byte: bit 7 equal to its `SEQ`, bit 6 the opposite, bits 5:3 zero, code 4 or less. Anything else is a bad answer.

## The chip side (in silicon, fixed)

* **Timeout:** while waiting for an `ACK` event the chip counts clocks. At 2^16 - 1 (65535) clocks it gives up on the attempt. At 50 MHz that is 1.3 ms.
* **Fault:** a timeout, a bad answer, or an illegal state code. On a fault the chip turns its `uio` outputs off at once and enters QUIET.
* **QUIET:** a fixed 65535 clocks with `uio` off and `REQ` held. During QUIET the chip takes the present `ACK` level as its new baseline, so an odd number of `ACK` toggles from a broken attempt does no harm.
* **Retry:** after QUIET the chip sends the same transaction again (same header with the same `SEQ`, same address and data). Up to 4 attempts in all.
* **LINK_ERROR:** after the 4th failed attempt the transaction ends with status 5, LINK_ERROR (chip-internal, never on the wire). The CPU traps on it like any other bus fault, so software can report the dead link or try again. `SEQ` does not flip.
* **Output enable** is computed from the next state every clock, so no state, including an illegal one, can leave the chip driving `uio` by accident.
* **Request check: none (decided 2026-11-22, user's go).** The answer is checked, the request is not. Under a corrupted chip clock the RP can carry out a garbled request (seen under torture); with a clean clock it was never seen. A check byte per request would cost one byte per transaction.
* **Open check (added 2026-11-22, user's go, after the torture hang):** a one-bit flag is set on the clock a transaction starts and cleared only in the two places that end it (a valid end, or LINK_ERROR). Being idle with the flag set means a bad clock edge threw the state machine back to idle in the middle of a transaction (the state codes 001 and 010 differ in two bits, so a torn change between them can read 000). That counts as a fault like an illegal state code: `uio` off, QUIET, then a retry of the same transaction. Without it the chip sat idle forever and the CPU waited forever (found on the bench under the noisy clock, 3 hangs in 3 runs). The chip's `idle` signal to the CPU is low while the flag is set. Proven on the board with a bench-only input that forces the torn edge (58,387 forced tears, each one fault and one retry, no error; bench/test4-report.md).
* **The CPU's side of the bus unit:** the CPU holds its request (start, header, address, write data) until the bus unit's `idle` goes low, then waits for `idle` to go high again and reads the status and data. It never waits for the one-clock done pulse, which a bad clock edge can make it miss. The CPU has no deadline counter of its own (decided 2026-11-22, user's go): the bus unit always ends a transaction within four attempts.
* **Two status outputs for the RP (added 2026-11-22, user's go):**
  * `uo_out[2]` = `uio_oe`, high while the chip drives `uio`. The RP can refuse to drive while it is high (the bench safety net, `V 1` recovery) and check every bus turnaround.
  * `uo_out[3]` = `FRAME`, high from the clock the header goes out until the transaction ends or the chip gives up; low while idle and during QUIET. It rises one clock before the header's `REQ` change, together with `uio_oe`, and falls only after the RP's last answer has been seen, so every `REQ` change falls inside `FRAME`. Every rise starts a header, a retry included. Nothing inside the chip uses it; firmware may ignore it.
* **Interrupt input:** `IRQ` (ui_in[1]) passes through a two-flop synchronizer and the same three-sample filter as `ACK` (chip-rtl/ab_irq.sv), so a crosstalk pulse one or two samples long never reaches the CPU.

Cost: nothing on a clean transaction (the checks run in parallel with the handshake). About 2.6 ms per fault at 50 MHz (timeout plus QUIET).

## The RP side (firmware, adjustable)

* **REQ filter:** the PIO accepts a new `REQ` level only if it is still there 3 cycles later.
* **Echo:** the status byte carries the header's `SEQ` as above.
* **When the chip sends NEXT** is a rule of the CPU and is in the comment at `cr0` in [asc2-emulated/asc.h](../asc2-emulated/asc.h) ("Sequential Fetch").
* **Copy of the last answer:** the RP keeps the last transaction's header, address, write data, status and read data. A transaction with the same header (same `SEQ`), same address and same write data is a retry: the RP sends the copied answer and does not do the access again. This makes every access happen exactly once, which matters for writes, console reads (which consume a byte) and NEXT (which advances a pointer). The copy is cleared at every chip reset.
* **A lost answer on order (the test register `RP_LOSE_ADDR` of [asc2-emulated/asc.h](../asc2-emulated/asc.h)):** the RP does the access and leaves all four attempts at it without an answer, so that the CPU gets LINK_ERROR. A repeat of the request must stay unanswered too: the copy would otherwise turn the lost answer into a late one. After the fourth attempt an equal request is a new transaction. [asc2-tests/chip/asc_trace.cpp](../asc2-tests/chip/asc_trace.cpp) does it this way.
* **RP timeout, T_rp:** mid-transaction, if the chip shows no progress for T_rp, the RP gives up: it stops the PIO, releases `uio` at once (`ACK` keeps its level), waits until `REQ` has been still for T_rp, then restarts the PIO waiting for a header at `REQ`'s present level. T_rp is 4096 chip clocks (82 us at 50 MHz), far below the chip's 65535-clock timeout and QUIET, so the RP is always ready before the chip retries.
* **ACK repair:** the PIO answers an event by setting `ACK` to `REQ`'s new level, so between transactions `ACK` equals `REQ`. If the RP gave up without answering the chip's last `REQ` change, the two differ, and the first answer after the restart would set `ACK` to the level it already has: the chip would see nothing and time out again. So in that case the RP sets `ACK` equal to `REQ` before restarting, at 98304 chip clocks (24 T_rp) after the last `REQ` change. That is the middle of the chip's QUIET (it waits 65535 clocks for the missing answer, then stays quiet 65535 more), where a change of `ACK` only becomes the chip's new baseline; the margin is 32768 clocks each way. A `REQ` change during the wait is another unanswered attempt and restarts the wait. Found on the bench on 2026-11-21: without the repair, every fault the RP left unanswered cost two retries instead of one.
* **Stale check:** before driving an answer, the RP checks that the chip has waited less than T_rp since its last byte. If not, the chip may be about to time out and retry, and driving could collide with the retried header, so the RP recovers as above instead. The retry then gets the copied answer.
* **No slow addresses:** every address the chip can read is served from RP RAM or a register, so no answer can take longer than a few microseconds. Flash is used only behind the disk registers (the buffer is in RAM), never in the answer path.
* **Recovery by FRAME (optional, instead of T_rp and the ACK repair):** if `FRAME` goes low while the RP is in the middle of a transaction (after a header, before its handback), the chip has given up and is in QUIET: it goes idle only after the RP's handback or after a QUIET. So the RP releases `uio`, sets `ACK` to `REQ`'s level at once (QUIET takes it as the new baseline) and waits for the next header. No clock counting. The RP must never set `ACK` on `FRAME` low outside a transaction: low also means idle, where a change of `ACK` would look like an answer to the next header. Firmware should read the pin's latched edge event (RP2040/RP2350 GPIO interrupt status), not only its level, and fall back to the timer path if it finds `FRAME` high again after a fall it did not handle (a QUIET it missed entirely, 65535 clocks). Simulated in chip-rtl/tb/tb_ab_bus.sv phase 4 (every RP fault kind, one chip fault per injection, the timer path never used); not used by the bench firmware, which has no wire for it.
  * A fault caught by the open check (a torn edge into idle) can come one or two clocks after a `REQ` change, so `FRAME` can fall before the RP has seen that change. An RP may then take a header and find `FRAME` already low. That is still the middle of a transaction (after a header): recover as above. Shown in the bench simulation; with the frame path in chip-rtl `make tear` (phase 3c).
* **Watchdog (decided 2026-11-22, user's go; devkit firmware, not the chip):** the last resort if the chip ever stops working without ending a transaction: the firmware resets the chip. The exact trigger is set in the SoC plan; it must not fire on a CPU that is only waiting for an interrupt and so makes no requests.

## Rules for any RP firmware

1. Answer within a few microseconds, always (never more than a small fraction of 65535 chip clocks).
2. Keep T_rp at 4096 chip clocks and the ACK repair at 98304; recompute both when the chip clock changes. The RP drives `CLK`, so it knows the clock exactly.
3. Keep the answer copy and clear it on chip reset.
4. Never drive `uio` except for the status and read data of the current transaction, and release it before the handback.
5. Hold `RST_N` low for at least 20 chip clocks, and stop or slow `CLK` only while `RST_N` is low. The chip's registers get their reset values on the 11th clock at the latest (two clocks of the synchronizer, eight of the filter, one for the registers), and until then `REQ`, `FRAME` and the chip's drivers of `uio` can have any level. So during these 20 clocks the RP leaves `uio` as inputs and does not listen to `REQ` or `FRAME`; it starts to listen after it has released `RST_N`. (Was 8 clocks; changed on 2026-11-22 after a review of the chip.)

## Rules for the devkit (TT ETR demoboard v3, RP2350B)

From asc2/asc2-kb/tt-devboard.md and rp-notes.md (retrieved 2026-11-21). All of them are RP firmware or board setup.

1. Pins: `uio` GPIO25-32, `REQ` GPIO33 (right after `uio[7]`, so the PIO IN window is GPIO25-33 as on the bench), `uio_oe` copy GPIO35 (uo_out[2]), `FRAME` GPIO36 (uo_out[3]), `ACK` GPIO17, `IRQ` GPIO18, `CLK` GPIO16, `RST_N` GPIO14. The PIO block needs `pio_set_gpio_base(pio, 16)` before its program is loaded. GPIO0-2 (mux control) and GPIO14 lie outside that window: drive them from SIO at 4 mA or more. Never drive GPIO15 (v3.3) or GPIO12.
2. The bench side-set pin (PDRV) is a bench-only safety net, and GPIO14 is `RST_N` on the devkit. The state machine keeps its side-set (so its program is the same on both boards) and the devkit build puts it on GPIO46, which both versions of the board join to nothing. The CDRV check works on the devkit as on the bench, since the chip puts `uio_oe` on uo_out[2] (GPIO35). (Was: the devkit build drops the side-set; changed on 2026-11-25 with the firmware for the devkit.)
3. Pull-ups on `uio` (the board has no pulls, and on RP2350 stepping A2 the pull-down can fail, erratum E9). An undriven bus then reads 0xFF, which is never a valid status.
4. `CLK` from an even divider (50% duty), slow slew, 4 mA to start. v3.3 has no series resistor on the clock trace.
5. Release `RST_N` with `CLK` stopped, wait 200 us, then start `CLK`. On v3.2 the reset net has 100 nF (a 17 us edge); with the clock stopped, the chip's reset filter sees only the clean final level. The falling edge is as slow, up to 29 us with the 8 mA that the RP2350 gives at least, so the 20 clocks of rule 5 above count 200 us after the RP has begun to pull (2026-11-25; before, a second reset on v3.2 could end before the line was low).
6. Select the project before enabling (ctrl_ena low, pulse ctrl_sel_rst_n, N pulses on ctrl_sel_inc, ctrl_ena high), with `RST_N` held low. Treat `REQ` as meaningless until then.
7. All DIP switches OFF (each ON switch ties a `ui_in` pin, including `ACK` and `IRQ`, to 3.3 V through 1k). All PMOD ports empty.
8. `REQ` also lights 7-segment segment A (about 2 mA through 510 ohm). It is a resistive load within the pad's 4 mA; cut the solder jumper if margin is needed.
9. The reset button of the board (SW4) can pull `RST_N` low in the middle of a transaction, without the firmware. The firmware must watch `RST_N` and, as soon as it reads low, stop driving `uio`, forget the transaction, and leave `ACK` at its level: the chip takes the level of `ACK` at the reset as its start. After the end of the reset the chip first drives the bus 15 clocks later. Found in the simulated start of 2026-11-25 ([../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv)), which presses the button in the middle of a request and of an answer on both versions of the board.

## Memory map (PC-style, v1)

The machine works like a classic PC: a slow disk, programs loaded into RAM, run from RAM. Nothing executes in place from flash.

| Range | What | RP side |
| --- | --- | --- |
| 0x000000 to 0x7FFFFF | RAM (main memory) | RP SRAM: about 192 KB on the RP2040, about 400 KB on the RP2350; the top 4 KB served is kernel-only |
| 0x800000 to 0x803FFF | ROM, 16 KB (the BIOS: boot loader, disk and console routines) | a RAM copy on the RP, loaded from its flash at power-up, so it can be fixed by a firmware update |
| 0xFF0000 to 0xFFFFFF | device registers | as in v0: ID, SCRATCH, TXCOUNT, TIMER, console, IRQ_ACK, disk |
| everything else | BUS_ERROR | |

The disk is the RP's flash, in 4 KB sectors behind the v0 disk registers. A disk command copies a sector to or from a RAM buffer while the bus keeps serving every address.

## Reset values and software levers

| Lever | Where | Effect |
| --- | --- | --- |
| Chip clock frequency | RP firmware (it drives `CLK`) | slower clock if the board or the chip is marginal |
| T_rp, answer timing | RP firmware | adjust to the clock |
| Fault injection | RP firmware (bench and test builds) | proves recovery on the real chip |
| ROM contents | RP firmware or a file in its flash | fix the boot code without touching the chip |
| Memory map, protection | RP firmware | change what the `kernel` and `fetch` bits allow |
| LINK_ERROR handling | chip software (trap handler) | report, retry, or reset |
