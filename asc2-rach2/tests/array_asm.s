# The part of `array.c` that is written in assembly: the compiler has no words
# for the instructions of the array of the dot products.
#
# The caller has pushed the arguments, the first one on the top of the stack,
# and a function may change `r0`-`r3`.

.global set_weights
.global set_inputs
.global get_array
.global add_products
.global learn
.global shift_inputs

.section .text

# void set_weights(unsigned int w0, unsigned int w1, unsigned int w2)
#
# Writes the weights of the three rows.
set_weights:
    lr 0(sp) r0
    mov r0 w0
    lr 4(sp) r0
    mov r0 w1
    lr 8(sp) r0
    mov r0 w2
    ret

# void set_inputs(unsigned int x0, unsigned int x1, unsigned int x2)
#
# Writes the inputs of the three rows.
set_inputs:
    lr 0(sp) r0
    mov r0 x0
    lr 4(sp) r0
    mov r0 x1
    lr 8(sp) r0
    mov r0 x2
    ret

# void get_array(unsigned int *regs)
#
# Stores the registers of the array at `regs`: `w0`-`w2`, then `x0`-`x2`.
get_array:
    lr 0(sp) r1
    mov w0 r0
    sr r0 0(r1)
    mov w1 r0
    sr r0 4(r1)
    mov w2 r0
    sr r0 8(r1)
    mov x0 r0
    sr r0 12(r1)
    mov x1 r0
    sr r0 16(r1)
    mov x2 r0
    sr r0 20(r1)
    ret

# int add_products(int sum)
#
# Returns `sum` plus the twelve products of the weights and the inputs
# (`dotv`).
add_products:
    lr 0(sp) r0
    dotv r0
    ret

# void learn(unsigned int errors)
#
# Moves every weight toward a smaller error: byte k of `errors` is the error
# of row k (`learn`).
learn:
    lr 0(sp) r0
    learn r0
    ret

# void shift_inputs(unsigned int input)
#
# Moves every input on by one cell and puts the low byte of `input` into the
# first cell (`xsh`).
shift_inputs:
    lr 0(sp) r0
    xsh r0
    ret
