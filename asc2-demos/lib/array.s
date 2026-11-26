# The instructions of the array of the dot products as C functions; the
# compiler has no words for them.
#
# The array has three rows of four cells. A cell multiplies a weight by an
# input, both signed bytes. `w0`-`w2` hold the weights and `x0`-`x2` the
# inputs, a word for a row, the first cell in the low byte.
#
# The caller has pushed the arguments, the first one on the top of the stack,
# and a function may change `r0`-`r3`.

.global set_weights
.global get_weights
.global set_inputs
.global add_products
.global correct_weights

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

# void get_weights(unsigned int *words)
#
# Stores the weights of the three rows at `words`.
get_weights:
    lr 0(sp) r1
    mov w0 r0
    sr r0 0(r1)
    mov w1 r0
    sr r0 4(r1)
    mov w2 r0
    sr r0 8(r1)
    ret

# void set_inputs(const unsigned int *words)
#
# Writes the inputs of the three rows from `words`.
set_inputs:
    lr 0(sp) r1
    lr 0(r1) r0
    mov r0 x0
    lr 4(r1) r0
    mov r0 x1
    lr 8(r1) r0
    mov r0 x2
    ret

# int add_products(void)
#
# Returns the sum of the twelve products of the weights and the inputs
# (`dotv`): a neuron of twelve inputs.
add_products:
    li 0x0 r0
    dotv r0
    ret

# void correct_weights(unsigned int errors)
#
# Moves every weight toward a smaller error: byte k of `errors` is the error
# of row k, in 128ths (`learn`).
correct_weights:
    lr 0(sp) r0
    learn r0
    ret
