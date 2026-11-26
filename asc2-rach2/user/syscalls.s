# The system calls as C functions.
#
# A system call is `int 0x80` with the number of the call in `r0` and the
# arguments in `r1`, `r2`, and `r3`. The kernel leaves the result in `r0` and
# keeps every other register. A negative result is the number of an error.
# The numbers of the calls are those of Linux for the i386.
#
# The C compiler passes arguments on the stack, the first one on the top, and
# a function may change `r0`-`r3`. So a function here puts its number into
# `r0` and jumps to the lines at the end of the file. They copy as many
# arguments into registers as the call has and enter the kernel.
#
# `user.h` says what each call does.

.global exit
.global fork
.global read
.global write
.global open
.global close
.global waitpid
.global creat
.global unlink
.global execve
.global chdir
.global lseek
.global getpid
.global rename
.global times
.global dup2
.global stat
.global getdents64
.global utimensat
.global screen
.global slices
.global nap

.section .text

exit:
    li 1 r0
    jmp _enter_with_1

fork:
    li 2 r0
    jmp _enter

read:
    li 3 r0
    jmp _enter_with_3

write:
    li 4 r0
    jmp _enter_with_3

open:
    li 5 r0
    jmp _enter_with_3

close:
    li 6 r0
    jmp _enter_with_1

waitpid:
    li 7 r0
    jmp _enter_with_3

creat:
    li 8 r0
    jmp _enter_with_2

unlink:
    li 10 r0
    jmp _enter_with_1

execve:
    li 11 r0
    jmp _enter_with_3

chdir:
    li 12 r0
    jmp _enter_with_1

lseek:
    li 19 r0
    jmp _enter_with_3

getpid:
    li 20 r0
    jmp _enter

rename:
    li 38 r0
    jmp _enter_with_2

times:
    li 43 r0
    jmp _enter_with_1

screen:
    li 1000 r0
    jmp _enter_with_3

slices:
    li 1001 r0
    jmp _enter_with_2

nap:
    li 1002 r0
    jmp _enter_with_1

dup2:
    li 63 r0
    jmp _enter_with_2

stat:
    li 106 r0
    jmp _enter_with_2

getdents64:
    li 220 r0
    jmp _enter_with_3

# The kernel does not look at the fourth argument, the flags
utimensat:
    li 320 r0
    jmp _enter_with_3

# Each line takes one argument, and the lines below it take the arguments
# before that one
_enter_with_3:
    lr 8(sp) r3
_enter_with_2:
    lr 4(sp) r2
_enter_with_1:
    lr 0(sp) r1
_enter:
    int 0x80
    ret
