# void switch_context(uint32_t *old_sp, uint32_t new_sp)
#
# Leaves the code that runs on one kernel stack and continues the code that
# waits on another.
#
# To the caller this is an ordinary function that returns later. A function
# must keep `r4`-`r7`, `fp`, and its return address, and the caller keeps the
# rest by itself, so these six words are all of the state. They are pushed on
# the current stack, the stack pointer is stored at `old_sp`, and the stack
# pointer becomes `new_sp`. The other stack has the same six words on its top,
# pushed when its code called this function. They are popped, and `ret`
# returns into that code.

.global switch_context

.section .text
switch_context:
    lr 0(sp) r0
    lr 4(sp) r1

    push lr
    push fp
    push r7
    push r6
    push r5
    push r4

    sr sp 0(r0)
    mov r1 sp

    pop r4
    pop r5
    pop r6
    pop r7
    pop fp
    pop lr
    ret
