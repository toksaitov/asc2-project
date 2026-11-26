/*
    System calls: the requests of a program to the kernel.
*/

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#define SYSCALL_INT 0x80 // a program enters the kernel with `int 0x80`

/* Numbers of the system calls, those of Linux for the i386 */

#define SYS_EXIT         1
#define SYS_FORK         2
#define SYS_READ         3
#define SYS_WRITE        4
#define SYS_OPEN         5
#define SYS_CLOSE        6
#define SYS_WAITPID      7
#define SYS_CREAT        8
#define SYS_UNLINK      10
#define SYS_EXECVE      11
#define SYS_CHDIR       12
#define SYS_LSEEK       19
#define SYS_GETPID      20
#define SYS_RENAME      38
#define SYS_TIMES       43
#define SYS_DUP2        63
#define SYS_STAT       106
#define SYS_GETDENTS64 220
#define SYS_UTIMENSAT  320
#define SYS_SCREEN    1000 // not a call of Linux: the screen and the button of the RP2040 (`sys/screen.h`)
#define SYS_SLICES    1001 // not a call of Linux: the network of the scheduler (`sys/proc/slice.h`)
#define SYS_NAP       1002 // not a call of Linux: sleep for a number of timer interrupts

/* Requests of `SYS_SLICES` */

#define SLICES_READ   0 // tell the weights of the network and how often it was right
#define SLICES_FORGET 1 // make the weights zeros first, so that the network learns anew

/* Numbers of errors, those of Linux. A system call returns one as a negative number. */

#define ENOENT        2 // no file has this name
#define EIO           5 // the disk has failed, or the file system is damaged
#define E2BIG         7 // the arguments of a program are too many or too long
#define ENOEXEC       8 // the file is not a program for this machine
#define EBADF         9 // the file descriptor is not open, or not open for this use
#define ECHILD       10 // the process has no such child
#define EAGAIN       11 // the table of processes is full
#define ENOMEM       12 // the free pages have run out
#define EBUSY        16 // another process has the screen
#define EACCES       13 // the directory cannot be run as a program
#define EFAULT       14 // an address is outside the memory of the program
#define ENOTDIR      20 // the name is of a file, and a directory is asked for
#define EISDIR       21 // the name is of the directory, and a file is asked for
#define EINVAL       22 // an argument has a value that the call does not know
#define EMFILE       24 // the process has no free file descriptor
#define ENOSPC       28 // the disk is full
#define ESPIPE       29 // the console has no positions
#define ENAMETOOLONG 36 // a name is longer than a name may be
#define ENOSYS       38 // no system call has this number

// Sets the handler of the `int` instruction.
void init_syscall(void);

#endif
