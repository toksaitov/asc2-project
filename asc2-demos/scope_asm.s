# The two loops of `scope.c`: the filter of twelve numbers in the array of
# the dot products, and the trace of a signal on the screen.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global scope_filter
.global scope_trace

.section .text

# int scope_filter(int sample)
#
# Takes a new sample into the filter and returns what the filter makes of
# the last twelve. `xsh` moves the samples in the inputs of the array on by
# one place and puts the new one into the first place; `dotv` multiplies
# the twelve samples by the twelve numbers of the filter, which are the
# weights, and adds the products. The sum is in 128ths and is brought to
# the range of a sample.
scope_filter:
    lr 0(sp) r0
    xsh r0
    li 0x0 r0
    dotv r0
    sar 0x7 r0
    ssat r0
    ret

# void scope_trace(volatile unsigned int *line, const signed char *ring, unsigned int start, int middle)
#
# Draws the 128 samples of a ring as one trace across the screen, the oldest
# sample, the one at `start`, in the column 0. A sample of 0 is in the row
# `middle`, and 8 units of a sample are a row. `line` is the register of
# the RP2040 that draws a line from the last point to a new one; the caller
# has set the first point.
scope_trace:
    push r4
    push r5
    push r6
    lr 12(sp) r3
    lr 16(sp) r4
    lr 20(sp) r5
    lr 24(sp) r6
    li 0x0 r2
_scope_trace_next:
    mov r5 r0
    add r2 r0
    and 0x7f r0
    add r4 r0
    lrb 0(r0) r0
    sar 0x3 r0
    mov r6 r1
    sub r0 r1
    shl 0x10 r1
    or r2 r1
    sr r1 0(r3)
    add 0x1 r2
    cmp 0x80 r2
    jl _scope_trace_next
    pop r6
    pop r5
    pop r4
    ret
