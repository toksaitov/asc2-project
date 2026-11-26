# The neuron of `guess.c`: twelve inputs that are a line of bytes in the
# array of the dot products, and twelve weights beside them.
#
# The caller has pushed the arguments, the first one on the top of the stack.

.global guess_sum
.global guess_learn
.global guess_push

.section .text

# int guess_sum(int start)
#
# Returns `start` plus the twelve products of the inputs and the weights
# (`dotv`).
guess_sum:
    lr 0(sp) r0
    dotv r0
    ret

# void guess_learn(unsigned int errors)
#
# Moves every weight by its input times the error of its row, in 128ths
# (`learn`).
guess_learn:
    lr 0(sp) r0
    learn r0
    ret

# void guess_push(int value)
#
# Moves every input on by one place and puts `value` into the first place
# (`xsh`); the oldest input is dropped.
guess_push:
    lr 0(sp) r0
    xsh r0
    ret
