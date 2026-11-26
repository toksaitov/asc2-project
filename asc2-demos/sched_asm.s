# One instruction of the chip for `sched.c` that the compiler has no word
# for.
#
# The caller has pushed the argument.

.global count_ones

.section .text

# int count_ones(unsigned int word)
#
# Returns the number of bits of the word that are 1 (`popcnt`).
count_ones:
    lr 0(sp) r0
    popcnt r0 r0
    ret
