"""
Measures what the instruction cache and the array of the dot products save.

Every instruction and every word of data that the chip does not hold comes
over the bus from the RP2040, and a request on the bus takes far longer than
the work of the chip between two requests. So the count of the requests is
the measure of the time of a program.

The script writes six small jobs in assembly, each in two ways:

* `old`: with the instructions that the chip had before the array (`dot4`,
  `mul`, shifts, compares), as well as they can be used;
* `new`: with the instructions of the array (`dotv`, `dotm`, `learn`, `xsh`,
  `ssat`, `usat`, `popcnt`).

It runs `old` without the cache (the chip before), `old` with the cache, and
`new` with the cache, each for two amounts of work, and prints the requests
that one unit of work takes: the difference of the two runs. A run without
the cache fetches every instruction, so its fetches are the count of the
instructions.

Every value that a job stores is compared with a model in Python, so the
three runs of a job compute the same thing, and it is the right thing.

Usage: python3 -I gain.py ASSEMBLER RUNNER BUILD_DIR
"""

import os
import random
import subprocess
import sys

OUT_ADDR = 0x80010000  # where the jobs store their results
RAM_ADDR = 0x80011000  # where a job keeps what it changes
RAM_BASE = 0x80000000  # the address of the first byte of RAM, as a program sees it

FRAME = """.section .data
{data}

.section .text
main:
    li 0x80020000 sp
{cache}
{setup}
    jmp _loop

    # The loop starts at a line of the cache, so that it takes as few lines as it can
    .align 0x20
_loop:
{loop}
    li 0x600d r0
    hlt
"""

CACHE_ON = """    mov cr0 r0
    or 0x10 r0
    mov r0 cr0"""

# Holds a register in the range of a signed byte or of 0 to 127 without `ssat` and `usat`: `{lo}` is a register with the lower end
CLAMP = """    cmp {lo} {reg}
    jge _{tag}_low
    mov {lo} {reg}
_{tag}_low:
    cmp 0x7f {reg}
    jle _{tag}_high
    li 0x7f {reg}
_{tag}_high:"""


def to_signed(value, bits=32):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


def pack(cells):
    """Returns the word of four signed bytes, the first one in the low byte."""
    return sum((cell & 0xff) << (8 * i) for i, cell in enumerate(cells))


def clamp(value, low, high):
    return max(low, min(high, value))


def words(label, values):
    return label + ":\n" + "\n".join("    .long 0x%08x" % (value & 0xffffffff) for value in values)


def signed_bytes(rng, count, low=-128, high=127):
    return [rng.randint(low, high) for _ in range(count)]


# ---------------------------------------------------------------------------
# The jobs. Each returns the data, the two programs, and the values that a
# correct run stores at `OUT_ADDR`, as (size in bytes, value) pairs in order.
# ---------------------------------------------------------------------------

def job_layer(count, rng):
    """A layer of a network: `count` neurons of twelve inputs, the weights in memory, ReLU."""
    inputs = signed_bytes(rng, 12)
    weights = [signed_bytes(rng, 12, -64, 64) for _ in range(count)]

    data = words("inputs", [pack(inputs[i:i + 4]) for i in range(0, 12, 4)]) + "\n" + \
           words("weights", [pack(neuron[i:i + 4]) for neuron in weights for i in range(0, 12, 4)])
    stores = [(1, clamp(sum(w * x for w, x in zip(neuron, inputs)) >> 7, 0, 127)) for neuron in weights]

    old_setup = f"""    li inputs r4
    lr 0(r4) r1
    lr 4(r4) r2
    lr 8(r4) r3
    li weights r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6
    li 0x0 fp"""
    old_loop = """    li 0x0 r0
    lr 0(r4) r7
    dot4 r7 r1 r0
    lr 4(r4) r7
    dot4 r7 r2 r0
    lr 8(r4) r7
    dot4 r7 r3 r0
    add 0xc r4
    sar 0x7 r0
""" + CLAMP.format(lo="fp", reg="r0", tag="out") + """
    srb r0 0(r5)
    add 0x1 r5
    sub 0x1 r6
    jne _loop"""

    new_setup = f"""    li inputs r4
    lr 0(r4) r1
    mov r1 x0
    lr 4(r4) r1
    mov r1 x1
    lr 8(r4) r1
    mov r1 x2
    li weights r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""
    new_loop = """    lr 0(r4) r7
    mov r7 w0
    lr 4(r4) r7
    mov r7 w1
    lr 8(r4) r7
    mov r7 w2
    add 0xc r4
    li 0x0 r0
    dotv r0
    sar 0x7 r0
    usat r0
    srb r0 0(r5)
    add 0x1 r5
    sub 0x1 r6
    jne _loop"""

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


def job_small_net(count, rng):
    """A network that the chip holds whole: 4 inputs, 3 neurons with ReLU, 1 output, for `count` inputs."""
    hidden = [signed_bytes(rng, 4) for _ in range(3)]
    output = signed_bytes(rng, 3) + [0]
    samples = [signed_bytes(rng, 4) for _ in range(count)]

    data = words("hidden", [pack(row) for row in hidden]) + "\n" + words("output", [pack(output)]) + "\n" + \
           words("samples", [pack(sample) for sample in samples])
    stores = []
    for sample in samples:
        outs = [clamp(sum(w * x for w, x in zip(row, sample)) >> 5, 0, 127) for row in hidden]
        stores.append((4, sum(w * h for w, h in zip(output, outs))))

    common = f"""    li output r4
    lr 0(r4) fp
    li samples r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""

    old_setup = """    li hidden r4
    lr 0(r4) r1
    lr 4(r4) r2
    lr 8(r4) r3
    li 0x0 sp
""" + common
    old_neuron = """    li 0x0 r0
    dot4 {row} r7 r0
    sar 0x5 r0
""" + CLAMP
    old_loop = "    lr 0(r4) r7\n" + \
        old_neuron.format(row="r1", lo="sp", reg="r0", tag="first") + "\n    mov r0 lr\n" + \
        old_neuron.format(row="r2", lo="sp", reg="r0", tag="second") + "\n    shl 0x8 r0\n    or r0 lr\n" + \
        old_neuron.format(row="r3", lo="sp", reg="r0", tag="third") + "\n    shl 0x10 r0\n    or r0 lr\n" + \
        """    li 0x0 r0
    dot4 fp lr r0
    sr r0 0(r5)
    add 0x4 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    new_setup = """    li hidden r4
    lr 0(r4) r1
    mov r1 w0
    lr 4(r4) r1
    mov r1 w1
    lr 8(r4) r1
    mov r1 w2
""" + common
    new_loop = """    lr 0(r4) r7
    mov r7 x0
    mov r7 x1
    mov r7 x2
    li 0x0 r1
    li 0x0 r2
    li 0x0 r3
    dotm r1
    sar 0x5 r1
    usat r1
    sar 0x5 r2
    usat r2
    shl 0x8 r2
    or r2 r1
    sar 0x5 r3
    usat r3
    shl 0x10 r3
    or r3 r1
    li 0x0 r0
    dot4 fp r1 r0
    sr r0 0(r5)
    add 0x4 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


def job_filter(count, rng):
    """A filter of twelve taps over `count` samples: each output is the sum of the last twelve samples times the taps."""
    taps = signed_bytes(rng, 12)
    samples = signed_bytes(rng, count)

    data = words("taps", [pack(taps[i:i + 4]) for i in range(0, 12, 4)]) + "\nsamples:\n" + \
           "\n".join("    .byte 0x%02x" % (sample & 0xff) for sample in samples) + "\n    .align 0x4"
    window = [0] * 12
    stores = []
    for sample in samples:
        window = [sample] + window[:11]
        stores.append((4, sum(t * x for t, x in zip(taps, window))))

    # The chip before has no register left for the taps but `fp`, `lr`, and `sp`, which a program without a stack may use
    old_setup = f"""    li taps r4
    lr 0(r4) fp
    lr 4(r4) lr
    lr 8(r4) sp
    li 0x0 r1
    li 0x0 r2
    li 0x0 r3
    li samples r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""
    old_loop = """    lrbu 0(r4) r0
    shl 0x8 r3
    mov r2 r7
    shr 0x18 r7
    or r7 r3
    shl 0x8 r2
    mov r1 r7
    shr 0x18 r7
    or r7 r2
    shl 0x8 r1
    or r0 r1
    li 0x0 r0
    dot4 fp r1 r0
    dot4 lr r2 r0
    dot4 sp r3 r0
    sr r0 0(r5)
    add 0x1 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    new_setup = f"""    li taps r4
    lr 0(r4) r1
    mov r1 w0
    lr 4(r4) r1
    mov r1 w1
    lr 8(r4) r1
    mov r1 w2
    li 0x0 r1
    mov r1 x0
    mov r1 x1
    mov r1 x2
    li samples r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""
    new_loop = """    lrbu 0(r4) r0
    xsh r0
    li 0x0 r0
    dotv r0
    sr r0 0(r5)
    add 0x1 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


def job_vertices(count, rng):
    """A matrix of three rows times `count` vertices (x, y, z, 1): the turn and the move of a 3D model."""
    matrix = [signed_bytes(rng, 4) for _ in range(3)]
    vertices = [signed_bytes(rng, 3) + [1] for _ in range(count)]

    data = words("matrix", [pack(row) for row in matrix]) + "\n" + words("vertices", [pack(vertex) for vertex in vertices])
    stores = [(4, sum(m * v for m, v in zip(row, vertex))) for vertex in vertices for row in matrix]

    common = f"""    li vertices r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""

    old_setup = """    li matrix r4
    lr 0(r4) r1
    lr 4(r4) r2
    lr 8(r4) r3
""" + common
    old_loop = """    lr 0(r4) r7
    li 0x0 r0
    dot4 r1 r7 r0
    sr r0 0(r5)
    li 0x0 r0
    dot4 r2 r7 r0
    sr r0 4(r5)
    li 0x0 r0
    dot4 r3 r7 r0
    sr r0 8(r5)
    add 0x4 r4
    add 0xc r5
    sub 0x1 r6
    jne _loop"""

    new_setup = """    li matrix r4
    lr 0(r4) r1
    mov r1 w0
    lr 4(r4) r1
    mov r1 w1
    lr 8(r4) r1
    mov r1 w2
""" + common
    new_loop = """    lr 0(r4) r7
    mov r7 x0
    mov r7 x1
    mov r7 x2
    li 0x0 r1
    li 0x0 r2
    li 0x0 r3
    dotm r1
    sr r1 0(r5)
    sr r2 4(r5)
    sr r3 8(r5)
    add 0x4 r4
    add 0xc r5
    sub 0x1 r6
    jne _loop"""

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


def job_one_bit(count, rng):
    """A layer of a network with weights and inputs of one bit: `count` neurons of 256 inputs each."""
    inputs = [rng.getrandbits(32) for _ in range(8)]
    weights = [[rng.getrandbits(32) for _ in range(8)] for _ in range(count)]

    data = words("inputs", inputs) + "\n" + words("weights", [word for neuron in weights for word in neuron])
    stores = [(4, sum(bin(~(w ^ x) & 0xffffffff).count("1") for w, x in zip(neuron, inputs))) for neuron in weights]

    common = f"""    li weights r4
    li inputs r5
    li {8 * count} r6
    li 0x0 r2"""
    tail = """    add r0 r2
    add 0x4 r4
    add 0x4 r5
    sub 0x1 r6
    test 0x7 r6
    jne _loop
    sr r2 0({out})
    add 0x4 {out}
    li 0x0 r2
    sub 0x20 r5
    cmp 0x0 r6
    jne _loop"""

    # The count of the bits without `popcnt`: sums of 2, 4, and 8 bits side by side, then the four bytes added by a product
    old_setup = f"""    li 0x55555555 fp
    li 0x33333333 lr
    li 0x0f0f0f0f r3
    li 0x01010101 r7
    li 0x{OUT_ADDR:x} sp
""" + common
    old_loop = """    lr 0(r4) r0
    lr 0(r5) r1
    xor r1 r0
    not r0
    mov r0 r1
    shr 0x1 r1
    and fp r1
    sub r1 r0
    mov r0 r1
    shr 0x2 r1
    and lr r1
    and lr r0
    add r1 r0
    mov r0 r1
    shr 0x4 r1
    add r1 r0
    and r3 r0
    mul r7 r0
    shr 0x18 r0
""" + tail.format(out="sp")

    new_setup = f"""    li 0x{OUT_ADDR:x} r7
""" + common
    new_loop = """    lr 0(r4) r0
    lr 0(r5) r1
    xor r1 r0
    not r0
    popcnt r0 r0
""" + tail.format(out="r7")

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


def job_learn(count, rng):
    """A neuron of twelve inputs that learns from `count` examples: a guess, the error, and a step of every weight."""
    start = signed_bytes(rng, 12, -20, 20)
    truth = signed_bytes(rng, 12, -100, 100)
    examples = []
    for _ in range(count):
        inputs = signed_bytes(rng, 12, -60, 60)
        examples.append((inputs, sum(w * x for w, x in zip(truth, inputs)) >> 7))

    data = words("start", [pack(start[i:i + 4]) for i in range(0, 12, 4)]) + "\n" + \
           words("examples", [word for inputs, target in examples
                              for word in [pack(inputs[i:i + 4]) for i in range(0, 12, 4)] + [target]])
    weights = list(start)
    stores = []
    for inputs, target in examples:
        total = sum(w * x for w, x in zip(weights, inputs))
        stores.append((4, total))
        error = clamp((target - (total >> 7)) >> 2, -128, 127)
        weights = [clamp(w + ((error * x + 64) >> 7), -128, 127) for w, x in zip(weights, inputs)]

    common = f"""    li examples r4
    li 0x{OUT_ADDR:x} r5
    li {count} r6"""

    # The weights are bytes in RAM: `dot4` reads them as words, and the step changes them one by one
    old_setup = f"""    li start r4
    li 0x{RAM_ADDR:x} fp
    lr 0(r4) r1
    sr r1 0(fp)
    lr 4(r4) r1
    sr r1 4(fp)
    lr 8(r4) r1
    sr r1 8(fp)
    li 0xffffff80 lr
""" + common
    old_loop = """    li 0x0 r0
    lr 0(r4) r1
    lr 0(fp) r7
    dot4 r7 r1 r0
    lr 4(r4) r1
    lr 4(fp) r7
    dot4 r7 r1 r0
    lr 8(r4) r1
    lr 8(fp) r7
    dot4 r7 r1 r0
    sr r0 0(r5)
    sar 0x7 r0
    lr 12(r4) r3
    sub r0 r3
    sar 0x2 r3
""" + CLAMP.format(lo="lr", reg="r3", tag="error") + """
    li 0x0 r2
_weight:
    lrb 0(r4) r1
    mul r3 r1
    add 0x40 r1
    sar 0x7 r1
    lrb 0(fp) r7
    add r7 r1
""" + CLAMP.format(lo="lr", reg="r1", tag="weight") + """
    srb r1 0(fp)
    add 0x1 r4
    add 0x1 fp
    add 0x1 r2
    cmp 0xc r2
    jne _weight
    sub 0xc fp
    add 0x4 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    new_setup = """    li start r4
    lr 0(r4) r1
    mov r1 w0
    lr 4(r4) r1
    mov r1 w1
    lr 8(r4) r1
    mov r1 w2
    li 0x010101 fp
""" + common
    new_loop = """    lr 0(r4) r1
    mov r1 x0
    lr 4(r4) r1
    mov r1 x1
    lr 8(r4) r1
    mov r1 x2
    li 0x0 r0
    dotv r0
    sr r0 0(r5)
    sar 0x7 r0
    lr 12(r4) r3
    sub r0 r3
    sar 0x2 r3
    ssat r3
    and 0xff r3
    mul fp r3
    learn r3
    add 0x10 r4
    add 0x4 r5
    sub 0x1 r6
    jne _loop"""

    return data, (old_setup, old_loop), (new_setup, new_loop), stores


JOBS = [
    ("layer",     "a neuron of a layer, 12 inputs, weights in memory",   job_layer,     32),
    ("small net", "a guess of a network of 4 inputs, 3 neurons, 1 output", job_small_net, 32),
    ("filter",    "an output of a filter of 12 taps",                    job_filter,    32),
    ("vertices",  "a vertex times a matrix of 3 rows",                   job_vertices,  32),
    ("one bit",   "a neuron of 256 inputs of one bit",                   job_one_bit,   8),
    ("learn",     "a step of learning of a neuron of 12 inputs",         job_learn,     16),
]


def run(assembler, runner, build_dir, name, data, program, has_cache, stores, frame=FRAME):
    """Runs a program and returns its requests on the bus, as (all, fetches). The stored values must be those of the model."""
    setup, loop = program
    source = frame.format(data=data, cache=CACHE_ON if has_cache else "", setup=setup, loop=loop)

    base = os.path.join(build_dir, name)
    with open(base + ".s", "w") as file:
        file.write(source)
    subprocess.run([assembler, base + ".s", base + ".bin"], check=True)
    status = subprocess.run([runner, "-q", "-r", "-n", "100000000", "-b", base + ".bus", base + ".bin"],
                            stdout=subprocess.DEVNULL).returncode
    if status != 0:
        sys.exit("%s: the run has ended with the status %d" % (name, status))

    requests = 0
    fetches = 0
    stored = []
    with open(base + ".bus") as file:
        next(file)
        for line in file:
            kind, _, size, addr, value, _ = line.strip().split(",")
            requests += 1
            fetches += kind == "fetch"
            if kind == "write" and int(addr, 16) >= OUT_ADDR - RAM_BASE and int(addr, 16) < RAM_ADDR - RAM_BASE:
                stored.append((int(size), to_signed(int(value, 16), 8 * int(size))))
    expected = [(size, to_signed(value, 8 * size)) for size, value in stores]
    if stored != expected:
        sys.exit("%s: the program has stored other values than the model" % name)

    return requests, fetches


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    assembler, runner, build_dir = sys.argv[1:]
    os.makedirs(build_dir, exist_ok=True)

    print("Requests on the bus for one unit of work (instructions in parentheses)")
    print()
    print("%-56s %14s %14s %14s %8s" % ("unit of work", "chip before", "with cache", "cache + array", "gain"))
    for name, unit, job, count in JOBS:
        results = []
        for kind, has_cache in (("old", False), ("old", True), ("new", True), ("new", False)):
            totals = []
            for amount in (count, 2 * count):
                data, old, new, stores = job(amount, random.Random(20261125))
                tag = "%s_%s_%s_%d" % (name.replace(" ", "_"), kind, "cache" if has_cache else "plain", amount)
                totals.append(run(assembler, runner, build_dir, tag, data, old if kind == "old" else new, has_cache, stores))
            results.append(((totals[1][0] - totals[0][0]) / count, (totals[1][1] - totals[0][1]) / count))
        before, cached, array, array_plain = results
        print("%-56s %7.1f (%4.0f) %7.1f (%4.0f) %7.1f (%4.0f) %7.1fx" %
              (unit, before[0], before[1], cached[0], before[1], array[0], array_plain[1], before[0] / array[0]))


if __name__ == "__main__":
    main()
