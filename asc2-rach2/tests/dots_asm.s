# The part of `dots.c` that is written in assembly: the compiler has no word
# for the instruction `dot4`.

.global dot4

.section .text

# int dot4(unsigned int a, unsigned int b, int sum)
#
# Takes `a` and `b` as four signed bytes each, multiplies the bytes in the
# same places, and returns `sum` with the four products added. The chip
# computes the products in its dot-product unit.
#
# The caller has pushed the arguments, the first one on the top of the stack.
dot4:
    lr 0(sp) r0
    lr 4(sp) r1
    lr 8(sp) r2
    dot4 r0 r1 r2
    mov r2 r0
    ret
