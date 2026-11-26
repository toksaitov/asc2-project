"""Prints the lines that `dot4_random.s` or `dot4_irq.s` must print.

    python3 dot4_expect.py random
    python3 dot4_expect.py irq

The programs check `dot4` against `mul` on the machine itself. This script
computes the last sum a third way, with the integers of Python, so that a
mistake that `dot4` and `mul` share does not pass.
"""

import sys

PAIRS = {"random": 0x4000, "irq": 0x1000}

MASK = 0xFFFFFFFF


def next_word(state):
    state ^= (state << 13) & MASK
    state ^= state >> 17
    state ^= (state << 5) & MASK
    return state


def signed_byte(word, index):
    byte = (word >> (8 * index)) & 0xFF
    return byte - 256 if byte >= 128 else byte


def main():
    name = sys.argv[1]
    state = 0x2545F491
    total = 0
    for _ in range(PAIRS[name]):
        state = first = next_word(state)
        state = second = next_word(state)
        state = next_word(state)
        total += state
        total += sum(signed_byte(first, i) * signed_byte(second, i) for i in range(4))
    print(f"dot4 {name}: ready")
    print(f"{total & MASK:08x}")
    print(f"dot4 {name}: done")


if __name__ == "__main__":
    main()
