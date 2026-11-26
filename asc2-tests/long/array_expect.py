"""Prints the lines that `array_random.s` must print.

    python3 array_expect.py

The program runs the instructions of the array on random words and prints
one total. This script computes the total by the rules of the instructions,
with the integers of Python, so that a mistake of the machine does not pass.
"""

ROUNDS = 0x1000
ROWS = 3

MASK = 0xFFFFFFFF


def next_word(state):
    state ^= (state << 13) & MASK
    state ^= state >> 17
    state ^= (state << 5) & MASK
    return state


def signed_byte(word, index):
    byte = (word >> (8 * index)) & 0xFF
    return byte - 256 if byte >= 128 else byte


def signed_word(word):
    return word - (1 << 32) if word & 0x80000000 else word


def row_sum(weights, inputs):
    return sum(signed_byte(weights, i) * signed_byte(inputs, i) for i in range(4))


def hold(value, low, high):
    return max(low, min(high, value))


def main():
    state = 0x2545F491
    total = 0

    def random():
        nonlocal state
        state = next_word(state)
        return state

    for _ in range(ROUNDS):
        weights = [random() for _ in range(ROWS)]
        inputs = [random() for _ in range(ROWS)]

        # dotv: all products into one sum; Z is 2, S is 4, and C and O are clear
        result = (random() + sum(row_sum(w, x) for w, x in zip(weights, inputs))) & MASK
        flags = (2 if result == 0 else 0) | (4 if result & 0x80000000 else 0)
        total += flags + result

        # dotm: the products of a row into a sum of its own
        for w, x in zip(weights, inputs):
            total += (random() + row_sum(w, x)) & MASK

        # learn: a weight gains its input times the error of its row, in 128ths, rounded down after a half is added
        errors = random()
        for row in range(ROWS):
            error = signed_byte(errors, row)
            new = 0
            for i in range(4):
                weight = signed_byte(weights[row], i) + ((error * signed_byte(inputs[row], i) + 64) >> 7)
                new |= (hold(weight, -128, 127) & 0xFF) << (8 * i)
            weights[row] = new
            total += new

        # xsh: the twelve inputs are one line of bytes, and a new byte enters at its low end
        line = sum(x << (32 * row) for row, x in enumerate(inputs))
        line = ((line << 8) | (random() & 0xFF)) & ((1 << (32 * ROWS)) - 1)
        for row in range(ROWS):
            inputs[row] = (line >> (32 * row)) & MASK
            total += inputs[row]

        value = random()
        total += hold(signed_word(value), -128, 127) & MASK
        total += hold(signed_word(value), 0, 127)
        total += bin(value).count("1")

    print("array random: ready")
    print(f"{total & MASK:08x}")
    print("array random: done")


if __name__ == "__main__":
    main()
