/*
    The array of the dot products of the chip, shared by the processes.
*/

#ifndef _ARRAY_H_
#define _ARRAY_H_

#include "sys/proc/process.h"

// Sets the function that handles an instruction for the array of a process that does not have the array.
void init_array(void);

// Gives a process zeros as its registers of the array, as a program has them at its start.
void reset_array(process_t *proc);

// Takes the array from a process that ends.
void release_array(process_t *proc);

// Gives a child the registers of the array of its parent.
void copy_array(process_t *child, process_t *parent);

// Lets the process that gets the CPU use the array without the kernel if the chip holds its values, and not otherwise.
void switch_array(const process_t *proc);

#endif
