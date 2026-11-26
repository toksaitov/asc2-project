# The first instructions of a program.
#
# The kernel starts a program at `_start` in user mode. The stack pointer is
# at a word with the number of arguments. The addresses of the argument
# strings follow that word, then a zero, the addresses of the environment
# strings, and another zero. The code calls `main(argc, argv, envp)` and ends
# the process with the value that `main` has returned.

.global _start

.section .text
_start:
    # argc is the word at the top, and argv is the address of the word after it
    lr 0(sp) r0
    mov sp r1
    add 0x4 r1

    # envp is behind the `argc` words of argv and their zero
    mov r0 r2
    add 0x1 r2
    shl 0x2 r2
    add r1 r2

    # The C compiler takes arguments from the stack, the first one on the top
    push r2
    push r1
    push r0
    call main

    # exit(the value in r0)
    mov r0 r1
    li 1 r0
    int 0x80
