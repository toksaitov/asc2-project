The Network in the Scheduler (2026-11-23)
=========================================

This note is the outline of the small network that the kernel `asc2-rach2` runs in its scheduler with the instruction `dot4`, written before the code, and at its end what the code then measured. The plan names it in "The Dot-Product Unit": a classifier predicts whether a process will block before its time slice ends, and the scheduler picks one of two time slices from the prediction. It runs now (2026-11-23) as the proof that the dot-product unit works in real use.

## What the Scheduler Does Today

The processes get the CPU in the order of the process table, each for one period of the timer. A turn ends when the timer interrupts, when the process sleeps (it waits for a line of the console, for the disk, for the file system, or for a child), or when it exits.

## The Two Slices

The timer ticks four times as often as before, and the handler of the timer counts the ticks that the running process has left. The timer itself is never set anew (a write of the period starts the period again, and counting keeps one tick as the unit everywhere).

* The long slice is four ticks, as long as a turn was before the change.
* The short slice is one tick.

A process that is predicted to block gets the long slice: it gives the CPU back by itself, and a burst of work a little longer than a tick is not cut in the middle. A process that is predicted to run on gets the short slice, so that the others wait a quarter of the time behind it. The order of the processes does not change, so no process can starve, and the longest wait behind a process is what it was before the change.

The other way round (the short slice for a process that will block) was looked at and dropped. A process that blocks does not use its slice, so its length does not matter there, and the long slice for a process that runs on saves only the switches, which are a few hundred instructions against a slice of thousands. Under it the network could do nothing that a fixed long slice does not do.

## The Network

One neuron with four inputs, because one `dot4` multiplies four pairs of bytes. The inputs describe the process, and the four weights are the same for all processes, so a process that has just started is judged by what the others have taught.

The inputs are signed bytes, held between their limits and never wrapped:

1. The constant 32. Its weight is the bias.
2. The length of the last turn in ticks, times 32, at most 127.
3. How often the process has blocked lately: a number per process that goes up by 16 when a turn ends in a sleep and down by 4 when the timer ends it, between -128 and 127.
4. The system calls of the last turn, times 8, at most 127.

The score is `dot4` of the inputs and the weights, added to zero. A score above zero predicts that the process will block before its slice ends.

What the process then does is the label: the turn ends in a sleep, or the timer ends it. A turn that ends with the exit of the process teaches nothing. The label is known for the slice that was given and for no other: a process that got the short slice and ran through it may or may not have blocked within the long one. The third input repairs this over a few turns: when such a process does block, the number goes up four times as fast as it goes down.

Learning is the rule of the perceptron in whole numbers. After a wrong prediction each weight moves by the label (+1 or -1) times its input, shifted right by three bits, and stays between -64 and 63. The shift divides by 8 and rounds down, so a product of -4 moves the weight by -1 and a product of 4 does not move it. The product is a `mul` in C, as the plan says. The weights that the kernel starts with are those that it has learned in a training run on the emulator, and it goes on learning.

## The Check of `dot4` at Every Prediction

The kernel computes every score a second time in C, with four multiplications, and stops with a message if the two differ. The check stays in the kernel for good: it costs four multiplications per turn, and it makes every session of the kernel a test of `dot4` on live values, on the emulator, on the simulated chip, and on the board. The kernel prints at its end that the two sums were the same every time.

## How It Is Measured

One kernel, three policies chosen at build time: every process the long slice (the scheduler as it was), every process the short slice, and the network. The fixed short slice is the control that matters: without it a network that hands out short slices to everybody would look clever.

The load is one program, `load`, that runs as five processes of three kinds: two that compute without a pause, two that write a file again and again (each write sleeps until the disk is done), and one that computes for a while between two writes. The machine has memory for no more. The emulator runs it, where a tick is a fixed number of instructions and the same input gives the same run. The kernel counts and prints at its end: the turns and how each ended, the predictions and how many were right, and for the processes that slept the time from the wakeup to the next turn, in total and at the worst. The length of the whole run is counted in ticks.

The numbers are reported as they come out, also if the network does not beat a fixed slice. The measure of the network alone is how many predictions are right, against the rule that always says "runs on" and the rule that repeats the last turn.

## What Runs Where

* The emulator and the simulated chip run the kernel's sessions with the network on and the check of every score.
* The board runs the same sessions, and a user program that computes dot products with `dot4` in two processes while the timer switches between them, which checks the instruction in user mode with paging on.

## What Came Out (2026-11-23)

The code is [asc2-rach2/sys/proc/slice.c](../asc2-rach2/sys/proc/slice.c), and `make measure` in [asc2-tests/](../asc2-tests/) makes the numbers below. Two things differ from the outline:

* The kernel prints its counts only when it is built to measure. They depend on when the interrupts arrive, which differs between the emulator, the chip, and the board, and the sessions of the kernel must print the same lines on all three.
* The disk of the emulator ends a command after 16 instructions, so no process ever waits for it. The measurement builds the emulator with a disk that takes 2500 instructions, about a third of a tick, which is what a write to the flash of the Pico takes on the board.

The run of `load`, counted in ticks (a tick is 8192 instructions on the emulator):

* Every turn long: the whole run takes 675 ticks. The 157 waits for the CPU after a wakeup take 486 ticks in all, 8 at the most.
* Every turn short: 741 ticks. The 154 waits take 96 ticks in all, 2 at the most.
* The network chooses: 735 ticks. The 154 waits take 87 ticks in all, 2 at the most.

So the network gives a woken process the CPU as quickly as short turns for everybody do, a little quicker in all, and the run ends six ticks sooner than with short turns. Why it ends sooner was not measured: of the 154 turns that ended in a sleep, 152 slept before their first tick, so the long turn itself was seldom used. All three kernels compute, check, and teach the network at every turn, also the two that do not use its guess, so the fixed long turn here is the old length of a turn with the new work of the scheduler. The fixed long turn ends the run soonest, by 8 percent, and lets a woken process wait five times as long. A scheduler that only cared for the waits could take the short turn for everybody: on this load the gain of the network over it is small.

How often the network is right about "will sleep before the turn ends", of 845 turns: 821 (97 percent). The rule that repeats the last turn of the process is right 813 times, and the rule that always says "runs on" 691 times.

The weights that the kernel starts with are -36, 9, 29, and -14, for the constant, the ticks, the sleeps, and the system calls. They are what the network had learned from zeros at the end of one run of `load` (`make measure FIRST_WEIGHTS=0`). The weight of the sleeps carries the guess, as expected; the constant leans to "runs on". The check of every sum against C has never failed: on the emulator, on the simulated chip, and on the board.
