/*
    The length of a turn. A small network guesses whether a process will go
    to sleep before its turn ends, and the scheduler gives the process a long
    turn or a short one from the guess.
*/

#ifndef _SLICE_H_
#define _SLICE_H_

#include "sys/proc/process.h"

#define LONG_SLICE  4 // timer interrupts in a long turn
#define SHORT_SLICE 1 // timer interrupts in a short turn

/* How the scheduler chooses the length of a turn (`SLICE_POLICY`) */

#define SLICE_POLICY_LONG    0 // every turn is long
#define SLICE_POLICY_SHORT   1 // every turn is short
#define SLICE_POLICY_NETWORK 2 // the network chooses

#ifndef SLICE_POLICY
#define SLICE_POLICY SLICE_POLICY_NETWORK
#endif

/* How a turn has ended */
typedef enum turn_end
{
    TURN_END_SLEEP, // the process waits for something
    TURN_END_TIMER, // the turn was used up
    TURN_END_EXIT   // the process has ended
} turn_end_t;

// Prepares what the network knows about a process that is new.
void init_slice(process_t *proc);

// Guesses what the process will do, and sets the number of timer interrupts of the turn that starts.
void start_turn(process_t *proc);

// Tells the network how the turn of the process has ended. The network learns from a wrong guess.
void end_turn(process_t *proc, turn_end_t end);

// Notes that the process was woken up now, to measure how long it waits for the CPU.
void note_wakeup(process_t *proc);

// Prints what the network has done since the start.
void print_slices(void);

/* What `get_slices` stores, by the place of the word */

#define SLICES_WEIGHTS 0 // the four weights of the network, as signed bytes: the constant, the timer interrupts, the sleeps, the system calls
#define SLICES_TURNS   1 // turns that have ended in a sleep or by the timer: the guesses that could be judged
#define SLICES_RIGHT   2 // those of them that the network has guessed
#define SLICES_SLEEPS  3 // those of them that have ended in a sleep
#define SLICES_WORDS   4

// Stores what a program may know about the network: its weights and how often it was right.
void get_slices(uint32_t words[SLICES_WORDS]);

// Makes the network forget what it has learned: the weights become zeros, and it learns anew.
void forget_slices(void);

#endif
