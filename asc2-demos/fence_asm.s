# The three neurons of `fence.c`: a row of the array of the dot products for
# each, and one instruction for all three.
#
# The caller has pushed the arguments, the first one on the top of the stack.

.global fence_look

.section .text

# void fence_look(unsigned int inputs, int *sums)
#
# Gives the same four inputs to the three neurons and stores their three
# sums (`dotm`).
fence_look:
    lr 0(sp) r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x0 r1
    li 0x0 r2
    li 0x0 r3
    dotm r1
    lr 4(sp) r0
    sr r1 0(r0)
    sr r2 4(r0)
    sr r3 8(r0)
    ret
