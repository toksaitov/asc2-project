# The jobs of `race.c`, each written twice by hand: with the instructions of
# the array of the dot products, and with the instructions that the chip had
# before them (`dot4`, `mul`, and the rest), as well as they can be used. The
# loops are those of the measurement of `../asc2-tests/gain/gain.py`.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global learn_fast
.global learn_plain
.global think_fast
.global think_plain

.section .text

# int learn_fast(const unsigned int *examples, int count)
#
# A neuron of twelve inputs learns from `count` examples. An example is four
# words: the inputs of the three rows, and the answer that is right. The
# weights are in the array. A step is the guess (`dotv`), the error, and the
# correction of all twelve weights (`learn`). Returns the sum of the guesses.
learn_fast:
    push r4
    push r5
    push r6
    push fp
    lr 16(sp) r4
    lr 20(sp) r6
    li 0x010101 fp
    li 0x0 r5
_learn_fast_next:
    lr 0(r4) r1
    mov r1 x0
    lr 4(r4) r1
    mov r1 x1
    lr 8(r4) r1
    mov r1 x2
    li 0x0 r0
    dotv r0
    add r0 r5
    sar 0x7 r0
    lr 12(r4) r3
    sub r0 r3
    sar 0x2 r3
    ssat r3
    and 0xff r3
    mul fp r3
    learn r3
    add 0x10 r4
    sub 0x1 r6
    jne _learn_fast_next
    mov r5 r0
    pop fp
    pop r6
    pop r5
    pop r4
    ret

# int learn_plain(const unsigned int *examples, int count, signed char *weights)
#
# The same steps without the array: the twelve weights are bytes in memory,
# three `dot4` make the guess, and a loop corrects the weights one by one.
learn_plain:
    push r4
    push r5
    push r6
    push r7
    push fp
    push lr
    lr 24(sp) r4
    lr 28(sp) r6
    lr 32(sp) fp
    li 0xffffff80 lr
    li 0x0 r5
_learn_plain_next:
    li 0x0 r0
    lr 0(r4) r1
    lr 0(fp) r7
    dot4 r7 r1 r0
    lr 4(r4) r1
    lr 4(fp) r7
    dot4 r7 r1 r0
    lr 8(r4) r1
    lr 8(fp) r7
    dot4 r7 r1 r0
    add r0 r5
    sar 0x7 r0
    lr 12(r4) r3
    sub r0 r3
    sar 0x2 r3
    cmp lr r3
    jge _learn_plain_error_low
    mov lr r3
_learn_plain_error_low:
    cmp 0x7f r3
    jle _learn_plain_error_high
    li 0x7f r3
_learn_plain_error_high:
    li 0x0 r2
_learn_plain_weight:
    lrb 0(r4) r1
    mul r3 r1
    add 0x40 r1
    sar 0x7 r1
    lrb 0(fp) r7
    add r7 r1
    cmp lr r1
    jge _learn_plain_weight_low
    mov lr r1
_learn_plain_weight_low:
    cmp 0x7f r1
    jle _learn_plain_weight_high
    li 0x7f r1
_learn_plain_weight_high:
    srb r1 0(fp)
    add 0x1 r4
    add 0x1 fp
    add 0x1 r2
    cmp 0xc r2
    jne _learn_plain_weight
    sub 0xc fp
    add 0x4 r4
    sub 0x1 r6
    jne _learn_plain_next
    mov r5 r0
    pop lr
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

# int think_fast(const unsigned int *samples, int count, unsigned int output)
#
# A small network answers `count` questions. A question is a word of four
# inputs. Three neurons look at it at once (`dotm`, their weights are in the
# array), and one more neuron (`output` has its weights) makes the answer of
# what the three say. Returns the sum of the answers.
think_fast:
    push r4
    push r5
    push r6
    push r7
    push fp
    lr 20(sp) r4
    lr 24(sp) r6
    lr 28(sp) fp
    li 0x0 r5
_think_fast_next:
    lr 0(r4) r7
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
    add r0 r5
    add 0x4 r4
    sub 0x1 r6
    jne _think_fast_next
    mov r5 r0
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

# int think_plain(const unsigned int *samples, int count, unsigned int output, const unsigned int *hidden)
#
# The same network without the array: the weights of the three neurons stay
# in registers, and each neuron is a `dot4`, a shift, and two comparisons.
think_plain:
    push r4
    push r5
    push r6
    push r7
    push fp
    push lr
    lr 24(sp) r4
    lr 28(sp) r6
    lr 32(sp) fp
    lr 36(sp) r3
    lr 0(r3) r1
    lr 4(r3) r2
    lr 8(r3) r3
    li 0x0 r5
_think_plain_next:
    lr 0(r4) r7
    li 0x0 r0
    dot4 r1 r7 r0
    sar 0x5 r0
    cmp 0x0 r0
    jge _think_plain_first_low
    li 0x0 r0
_think_plain_first_low:
    cmp 0x7f r0
    jle _think_plain_first_high
    li 0x7f r0
_think_plain_first_high:
    mov r0 lr
    li 0x0 r0
    dot4 r2 r7 r0
    sar 0x5 r0
    cmp 0x0 r0
    jge _think_plain_second_low
    li 0x0 r0
_think_plain_second_low:
    cmp 0x7f r0
    jle _think_plain_second_high
    li 0x7f r0
_think_plain_second_high:
    shl 0x8 r0
    or r0 lr
    li 0x0 r0
    dot4 r3 r7 r0
    sar 0x5 r0
    cmp 0x0 r0
    jge _think_plain_third_low
    li 0x0 r0
_think_plain_third_low:
    cmp 0x7f r0
    jle _think_plain_third_high
    li 0x7f r0
_think_plain_third_high:
    shl 0x10 r0
    or r0 lr
    li 0x0 r0
    dot4 fp lr r0
    add r0 r5
    add 0x4 r4
    sub 0x1 r6
    jne _think_plain_next
    mov r5 r0
    pop lr
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret
