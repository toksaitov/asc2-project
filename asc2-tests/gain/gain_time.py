"""
Measures the time of the jobs of `gain.py`: the clocks of the simulated chip,
or the microseconds on the board.

`gain.py` counts the requests on the bus. This script runs the same jobs in
the same three ways and prints the time of one unit of work: the difference
of two runs with two amounts of work.

* `clocks`: the simulated chip runs each program on its own time, and the
  test counts the clocks. The RP2040 of the test answers a change of REQ
  after a number of clocks. The script prints one table for an answer at
  once, which is the work of the chip alone, and one for an answer after 4
  clocks, which is close to the bridge of the bench at 12.5 MHz.
* `board`: each program reads the clock of the bridge, in microseconds, at
  its start and at its end and prints the difference. The script takes the
  least of three runs.

Every program first runs on the emulator, where the values that it stores
are compared with the model in Python, as in `gain.py`.

Usage: python3 -I gain_time.py clocks ASSEMBLER RUNNER TRACE BUILD_DIR
       python3 -I gain_time.py board ASSEMBLER RUNNER BRIDGE BUILD_DIR
"""

import os
import random
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gain

FACTORS = (4, 8)        # the two amounts of work, in the amounts of `gain.py`: more work, so that the clock of the bridge is fine enough
ANSWER_DELAYS = (0, 4)  # clocks before the RP2040 of the test answers a change of REQ
BOARD_RUNS = 3          # runs of a program on the board; the least time counts
BOARD_SECONDS = "2.5"   # seconds that the bridge waits for the line of a program

# The frame of `gain.py` with the time: the program keeps the clock of the
# bridge in memory at its start and prints `T=` and the microseconds that
# have passed, as 8 digits in hex, at its end
FRAME = """.section .data
hex_digits:
    .ascii "0123456789abcdef"
{data}

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3
    lr 0xc(r3) r1
    li 0x8001f000 r2
    sr r1 0(r2)
{cache}
{setup}
    jmp _loop

    # The loop starts at a line of the cache, so that it takes as few lines as it can
    .align 0x20
_loop:
{loop}
    li 0x80ff0000 r3
    lr 0xc(r3) r4
    li 0x8001f000 r2
    lr 0(r2) r1
    sub r1 r4

    li 0x54 r0
    call print_byte
    li 0x3d r0
    call print_byte
    li 0x8 r5
_digit:
    mov r4 r2
    shr 0x1c r2
    li hex_digits r6
    add r6 r2
    lrbu 0(r2) r0
    call print_byte
    shl 0x4 r4
    sub 0x1 r5
    jne _digit
    li 0xa r0
    call print_byte

    li 0x600d r0
_sleep:
    hlt
    jmp _sleep

# Prints the byte in `r0`: waits until the console is ready, then writes the
# byte to the data register.
print_byte:
    lr 0x10(r3) r1
    test 0x2 r1
    je print_byte
    sr r0 0x14(r3)
    ret
"""


def clocks_of(trace, image, delay):
    """Returns the clocks that the simulated chip takes for a program."""
    result = subprocess.run([trace, "-q", "-r", "-t", "-n", "100000000", "-w", str(delay), image],
                            env=dict(os.environ, ASC_CLOCKS="1"), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    found = re.search(r"The run took (\d+) clocks", result.stderr)
    if result.returncode != 0 or found is None:
        sys.exit("%s: the run on the simulated chip has ended with the status %d" % (image, result.returncode))
    return int(found.group(1))


def micros_of(bridge, image):
    """Returns the microseconds that a program has measured on the board: the least of some runs."""
    times = []
    for _ in range(BOARD_RUNS):
        result = subprocess.run(["python3", "-I", bridge, "run", image, "--for", BOARD_SECONDS], capture_output=True, text=True)
        found = re.search(r"T=([0-9a-f]{8})", result.stdout)
        if found is None:
            sys.exit("%s: the program has printed no time on the board\n%s%s" % (image, result.stdout, result.stderr))
        times.append(int(found.group(1), 16))
    return min(times)


def table(title, unit_format, measure, images):
    """Prints the time of one unit of work of every job; `measure` gives the time of one program."""
    print(title)
    print()
    print("%-56s %14s %14s %14s %8s" % ("unit of work", "old code", "with cache", "cache + array", "gain"))
    for name, unit, _, count in gain.JOBS:
        row = []
        for kind, has_cache in (("old", False), ("old", True), ("new", True)):
            times = [measure(images[name, kind, has_cache, factor]) for factor in FACTORS]
            row.append((times[1] - times[0]) / ((FACTORS[1] - FACTORS[0]) * count))
        print(("%-56s " + unit_format + " " + unit_format + " " + unit_format + " %7.1fx") %
              (unit, row[0], row[1], row[2], row[0] / row[2]), flush=True)
    print()


def main():
    if len(sys.argv) != 6 or sys.argv[1] not in ("clocks", "board"):
        sys.exit(__doc__)
    mode, assembler, runner, tool, build_dir = sys.argv[1:]
    os.makedirs(build_dir, exist_ok=True)

    # The programs, each checked on the emulator against the model
    images = {}
    for name, _, job, count in gain.JOBS:
        for kind, has_cache in (("old", False), ("old", True), ("new", True)):
            for factor in FACTORS:
                data, old, new, stores = job(factor * count, random.Random(20261125))
                tag = "%s_%s_%s_%d" % (name.replace(" ", "_"), kind, "cache" if has_cache else "plain", factor * count)
                gain.run(assembler, runner, build_dir, tag, data, old if kind == "old" else new, has_cache, stores, FRAME)
                images[name, kind, has_cache, factor] = os.path.join(build_dir, tag + ".bin")

    if mode == "clocks":
        for delay in ANSWER_DELAYS:
            table("Clocks of the chip for one unit of work (the RP2040 answers after %d clocks)" % delay, "%14.1f",
                  lambda image: clocks_of(tool, image, delay), images)
    else:
        table("Microseconds on the board for one unit of work", "%14.1f", lambda image: micros_of(tool, image), images)


if __name__ == "__main__":
    main()
