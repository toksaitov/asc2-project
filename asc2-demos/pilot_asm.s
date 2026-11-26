# The neuron of `pilot.c`: four inputs in the first row of the array of the
# dot products, and its four weights beside them. The other rows stay at
# zero, as a program finds them.
#
# The caller has pushed the arguments, the first one on the top of the stack.

.global pilot_guess
.global pilot_learn

.section .text

# int pilot_guess(unsigned int inputs)
#
# Returns the sum of the products of the inputs and the weights (`dotv`).
pilot_guess:
    lr 0(sp) r0
    mov r0 x0
    li 0x0 r0
    dotv r0
    ret

# void pilot_learn(unsigned int inputs, unsigned int error)
#
# Moves every weight by its input times the error, in 128ths (`learn`).
pilot_learn:
    lr 0(sp) r0
    mov r0 x0
    lr 4(sp) r0
    learn r0
    ret
