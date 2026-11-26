/*
    The start of a program: its file is read from the disk into new memory.
*/

#ifndef _EXEC_H_
#define _EXEC_H_

#include "sys/lib/kint.h"

#define MAX_ARGS 32 // arguments of a program
#define MAX_ENVS 16 // strings of the environment of a program

/*
    The arguments and the environment that a program starts with. The strings
    are in the kernel, each with its zero byte, one right after the other: the
    arguments first.
*/
typedef struct exec_args
{
    const char *strs;
    uint32_t size; // bytes of all strings
    uint32_t argc; // number of arguments
    uint32_t envc; // number of strings of the environment
} exec_args_t;

// A program that is ready to run
typedef struct program
{
    uint32_t *page_dir; // top page table of its memory
    uint32_t entry;     // address of its first instruction
    uint32_t sp;        // stack pointer at the start
} program_t;

/*
    Reads a program from an ELF file of the disk into new page tables, and
    puts the arguments and the environment on its stack. Returns 0, or a
    negative number of an error: `-ENOENT` if the file is not there,
    `-ENOEXEC` if it is not a program for this machine, `-E2BIG` if the
    arguments do not fit, `-ENOMEM` if the free pages have run out, and
    `-EIO` if the disk has failed.
*/
int32_t load_program(const char *path, const exec_args_t *args, program_t *program);

#endif
