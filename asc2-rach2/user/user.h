/*
    What a program can call: the system calls of `syscalls.s` and the
    functions for text of `text.c`.

    The system calls have the names and the meanings of those of Linux. A
    call that fails returns a negative number: the number of an error.
*/

#ifndef _USER_H_
#define _USER_H_

#include <stddef.h>

/* The file descriptors that a program starts with */

#define STDIN  0
#define STDOUT 1
#define STDERR 2

/* Flags of `open` */

#define O_RDONLY    00       // for reading
#define O_WRONLY    01       // for writing
#define O_RDWR      02       // for both
#define O_CREAT     0100     // make the file if it is not there
#define O_TRUNC     01000    // remove the bytes that the file has
#define O_APPEND    02000    // every write goes to the end of the file
#define O_DIRECTORY 0200000  // the path must name a directory

/* Starting points of `lseek` */

#define SEEK_SET 0 // the start of the file
#define SEEK_CUR 1 // the position of the descriptor
#define SEEK_END 2 // the end of the file

/* Numbers of errors */

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

/* What `waitpid` stores about a child that has ended */

#define EXIT_CODE(status)   (((status) >> 8) & 0xff) // the value that the child gave to `exit`
#define STOP_REASON(status) ((status) & 0x7f)        // not 0 if the kernel has stopped the child for a mistake

/* The kind of a file in the mode that `stat` tells */

#define S_IFMT  0170000 // the bits of the kind
#define S_IFREG 0100000 // a file
#define S_IFDIR 0040000 // a directory

// The times of a process, counted in timer interrupts
typedef struct tms
{
    unsigned int utime;  // time that the process itself has run
    unsigned int stime;  // time that the kernel has worked for the process
    unsigned int cutime; // the two times of the children that have ended
    unsigned int cstime;
} tms_t;

// What `stat` tells about a file. The kernel fills the fields that have a comment.
typedef struct stat
{
    unsigned int dev;
    unsigned int ino;        // a number that no other file has
    unsigned short mode;     // the kind of the file and its permissions
    unsigned short nlink;    // number of names of the file: 1
    unsigned short uid;
    unsigned short gid;
    unsigned int rdev;
    unsigned int size;       // bytes of data
    unsigned int blksize;
    unsigned int blocks;
    unsigned int atime;      // the time of the last change, in seconds since the start of the machine
    unsigned int atime_nsec;
    unsigned int mtime;      // the same time
    unsigned int mtime_nsec;
    unsigned int ctime;      // the same time
    unsigned int ctime_nsec;
    unsigned int unused[2];
} stat_t;

/*
    An entry of a directory, as `getdents64` gives it. The entries follow
    each other in the buffer, and `reclen` leads from one to the next.
*/
typedef struct dirent64
{
    unsigned long long ino; // a number that no other file has
    long long off;
    unsigned short reclen;  // bytes of this entry
    unsigned char type;
    char name[];            // the name with its zero byte
} dirent64_t;

/* Processes */

// Ends the process. The parent gets the low byte of `status`. Does not return.
void exit(int status);

// Makes a child: a copy of the process. Returns the number of the child in the parent, and 0 in the child.
int fork(void);

/*
    Gives the process another program: the file `path`, with the arguments
    `argv` and the environment `envp`. Both are arrays of strings that end
    with a NULL. Returns only if it fails.
*/
int execve(const char *path, char *const argv[], char *const envp[]);

/*
    Waits for the end of a child: the one with the number `pid`, or any child
    if `pid` is -1. Stores at `status`, if it is not NULL, what `EXIT_CODE`
    and `STOP_REASON` read. The kernel has no options, so `options` is 0.
    Returns the number of the child.
*/
int waitpid(int pid, int *status, int options);

// Returns the number of the process.
int getpid(void);

// Returns the number of timer interrupts since the start of the machine. Stores the times of the process at `buf` if it is not NULL.
int times(tms_t *buf);

/* Files */

/*
    Opens a file, or the directory of the disk, which is `/`. `flags` has one
    of `O_RDONLY`, `O_WRONLY`, and `O_RDWR`, and any of the other flags. A
    file that `O_CREAT` makes gets the permissions `mode`. Returns the new
    file descriptor.
*/
int open(const char *path, int flags, int mode);

// Opens a file for writing, empty: a new one, or one that loses its bytes. Returns the new file descriptor.
int creat(const char *path, int mode);

// Closes a file descriptor.
int close(int fd);

// Reads at most `count` bytes of an open file, or a typed line of the console. Returns the number of bytes, 0 at the end.
int read(int fd, void *buf, unsigned int count);

// Writes `count` bytes to an open file, or prints them on the console. Returns the number of bytes written.
int write(int fd, const void *buf, unsigned int count);

// Moves the position of an open file to `offset` from one of the `SEEK_` points. Returns the new position.
int lseek(int fd, int offset, int whence);

// Makes `new_fd` stand for the open file of `old_fd`. Returns `new_fd`.
int dup2(int old_fd, int new_fd);

// Tells the kind, the size, and the time of a file.
int stat(const char *path, stat_t *buf);

// Reads entries of an open directory into `buf`. Returns the number of bytes, 0 at the end.
int getdents64(int fd, void *buf, unsigned int count);

// Removes a file.
int unlink(const char *path);

// Gives a file a new name. A file that has the new name is removed.
int rename(const char *old_path, const char *new_path);

// Sets the time of the open file `dirfd` to now. The kernel knows this one form: `path` and `times` are NULL.
int utimensat(int dirfd, const char *path, const void *times, int flags);

// Makes a directory the place where files are looked for. The disk has one directory, `/`.
int chdir(const char *path);

/* The screen and the push button */

#define SCREEN_OPEN    0
#define SCREEN_CLOSE   1
#define SCREEN_PICTURE 2

/*
    Takes the screen of the machine from the console for this program
    (`SCREEN_OPEN`; the result is the address of the page with the frame
    memory, the registers of the screen, the button, and the clock), gives it
    back (`SCREEN_CLOSE`), or names a picture that the program will draw
    (`SCREEN_PICTURE`; `a` is its number, from 0 to 3, and `b` its address).
    One program has the screen at a time; the console gets it back when the
    program ends. `screen.h` has functions that draw.
*/
int screen(int request, unsigned int a, unsigned int b);

/* The network of the scheduler */

/* Requests of `slices` */

#define SLICES_READ   0 // tell the weights of the network and how often it was right
#define SLICES_FORGET 1 // make the weights zeros first, so that the network learns anew

/* What `slices` stores, by the place of the word */

#define SLICES_WEIGHTS 0 // the four weights, as signed bytes, the first in the low byte: the constant, the timer interrupts of the last turn, the sleeps lately, the system calls of the last turn
#define SLICES_TURNS   1 // turns that the network has guessed about and that have ended in a sleep or by the timer
#define SLICES_RIGHT   2 // those of them that the network has guessed
#define SLICES_SLEEPS  3 // those of them that have ended in a sleep
#define SLICES_WORDS   4

/*
    The kernel guesses before every turn of a process whether the process
    will go to sleep before the turn ends, with a neuron of four inputs
    that learns from its wrong guesses. This call stores at `words` what
    the neuron has learned and how often it was right, since the start of
    the machine (`SLICES_WORDS` words). With `SLICES_FORGET` the neuron
    forgets first what it has learned, and learns anew. Not a call of Linux.
*/
int slices(int request, unsigned int *words);

// Lets the process sleep until `ticks` more timer interrupts have come; other processes run meanwhile. Returns 0. Not a call of Linux.
int nap(unsigned int ticks);

/* Text */

// Returns the number of bytes of a string, without its zero byte.
size_t strlen(const char *str);

// Returns 0 if two strings are equal.
int strcmp(const char *a, const char *b);

// Writes a string to a file descriptor.
void print(int fd, const char *str);

// Writes a number to a file descriptor, in decimal digits.
void print_number(int fd, unsigned int number);

#endif
