# The mistakes of `crash.c` that C has no words for.

.global turn_interrupts_off
.global enter_kernel_wrongly

.section .text

# void turn_interrupts_off(void)
#
# `cli` is an instruction for the kernel only: a program that could turn
# interrupts off would keep the CPU for as long as it liked.
turn_interrupts_off:
    cli
    ret

# void enter_kernel_wrongly(void)
#
# The kernel takes system calls through `int 0x80` only.
enter_kernel_wrongly:
    int 0x81
    ret
