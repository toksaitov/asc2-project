/*
    The length of a turn.

    A process that waits for the disk or for the console most of the time
    should get the CPU for as long as it wants: it gives the CPU back by
    itself soon. A process that computes without a pause should get short
    turns, so that the others do not wait long behind it. The kernel does not
    know which kind a process is, so it guesses before every turn: will the
    process go to sleep before the turn ends? A process that is expected to
    sleep gets a long turn, and the other kind a short one.

    The guess comes from the smallest neural network, one neuron. Four
    numbers describe what the process did before (the inputs), and four more
    say how much each of them counts (the weights). The neuron multiplies
    each input by its weight and adds the four products. The process is
    expected to sleep if the sum is above zero. The inputs and the weights
    are signed bytes, four in a word, and the instruction `dot4` of the chip
    computes the sum in one step: this is what the dot-product unit of the
    chip is for.

    The weights are the same for all processes, so a process that has just
    started is judged by what the others have taught. The network learns
    while the kernel runs. After every turn the kernel sees how the turn has
    ended. If the guess was wrong, each weight moves a little in the
    direction that would have made the guess right: up by a part of its
    input if the process has slept, and down by the same if it has not (the
    rule of the perceptron).

    The kernel computes every sum a second time with four multiplications of
    C and stops if the two sums differ. So every run of the kernel is also a
    check of the dot-product unit.
*/

#include "sys/proc/slice.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/cpu.h"
#include "sys/kio.h"
#include "sys/panic.h"
#include "sys/pit.h"

#define INPUTS 4 // inputs of the neuron, and bytes in a word

/* The inputs, by their place in the word */

#define INPUT_ONE    0 // always `INPUT_ONE_VALUE`, so that its weight is added to every sum
#define INPUT_TICKS  1 // timer interrupts of the last turn
#define INPUT_SLEEPS 2 // how often the process has slept lately
#define INPUT_CALLS  3 // system calls of the last turn

#define INPUT_ONE_VALUE 32
#define INPUT_MAX       127 // the largest value of a signed byte
#define INPUT_MIN       (-128)

#define TICKS_SCALE 32 // what one timer interrupt adds to its input
#define CALLS_SCALE 8  // what one system call adds to its input

#define SLEEPS_UP   16 // what a turn that ends in a sleep adds to the input of the sleeps
#define SLEEPS_DOWN 4  // what a turn that the timer ends takes from it

#define WEIGHT_MAX   63
#define WEIGHT_MIN   (-64)
#define LEARN_SHIFT  3 // a weight moves by its input divided by 8

/*
    The weights that the kernel starts with, as four signed bytes: -36, 9,
    29, and -14. The network has learned them from zeros in a run of the
    program `load` on the emulator with a disk as slow as a real one
    (`make measure` in `asc2-tests/`).
*/
#ifndef FIRST_WEIGHTS
#define FIRST_WEIGHTS 0xf21d09dc
#endif

static uint32_t weights = FIRST_WEIGHTS;

/* Counters for `print_slices` */

static uint32_t sums;            // sums that `dot4` and C have both computed
static uint32_t ends[3];         // turns by their end (`turn_end_t` values)
static uint32_t right_guesses;   // turns whose end the network has guessed
static uint32_t right_repeats;   // turns that ended as the turn of the same process before them
static uint32_t wrong_long;      // long turns that the timer has ended
static uint32_t wakeups;         // turns of a process that was woken up before
static uint32_t wait_ticks;      // timer interrupts between the wakeups and those turns
static uint32_t longest_wait;    // the most timer interrupts between a wakeup and the turn

// Returns the signed byte at the place `i` of a word.
static int32_t get_byte(uint32_t word, uint32_t i)
{
    return (int8_t) (word >> (8 * i));
}

// Returns a word with the signed byte at the place `i` set to a value between `min` and `max`.
static uint32_t set_byte(uint32_t word, uint32_t i, int32_t value, int32_t min, int32_t max)
{
    if (value < min) {
        value = min;
    } else if (value > max) {
        value = max;
    }

    return (word & ~(0xffu << (8 * i))) | (((uint32_t) value & 0xff) << (8 * i));
}

void init_slice(process_t *proc)
{
    proc->inputs = set_byte(0, INPUT_ONE, INPUT_ONE_VALUE, INPUT_MIN, INPUT_MAX);
    proc->slice = SHORT_SLICE;
    proc->turn_ticks = 0;
    proc->turn_calls = 0;
    proc->is_expected_to_sleep = false;
    proc->has_slept = false;
    proc->is_woken = false;
}

void start_turn(process_t *proc)
{
    int32_t sum = dot4(proc->inputs, weights, 0);

    int32_t sum_in_c = 0;
    for (uint32_t i = 0; i < INPUTS; ++i) {
        sum_in_c += get_byte(proc->inputs, i) * get_byte(weights, i);
    }
    if (sum != sum_in_c) {
        k_printf("dot4 of %p and %p: %d from the chip, %d from C\n", proc->inputs, weights, sum, sum_in_c);
        k_panic("the dot-product unit and C disagree");
    }
    ++sums;

    proc->is_expected_to_sleep = sum > 0;

    if (SLICE_POLICY == SLICE_POLICY_LONG) {
        proc->slice = LONG_SLICE;
    } else if (SLICE_POLICY == SLICE_POLICY_SHORT) {
        proc->slice = SHORT_SLICE;
    } else {
        proc->slice = proc->is_expected_to_sleep ? LONG_SLICE : SHORT_SLICE;
    }
    proc->turn_ticks = 0;
    proc->turn_calls = 0;

    if (proc->is_woken) {
        uint32_t wait = get_ticks() - proc->woken_at;
        ++wakeups;
        wait_ticks += wait;
        if (wait > longest_wait) {
            longest_wait = wait;
        }
        proc->is_woken = false;
    }
}

void end_turn(process_t *proc, turn_end_t end)
{
    ++ends[end];

    // The end of a process is not an answer to the question of the network
    if (end == TURN_END_EXIT) {
        return;
    }

    bool has_slept = end == TURN_END_SLEEP;
    if (has_slept == proc->has_slept) {
        ++right_repeats;
    }
    if (!has_slept && proc->turn_ticks == LONG_SLICE) {
        ++wrong_long;
    }

    if (has_slept == proc->is_expected_to_sleep) {
        ++right_guesses;
    } else {
        // The rule of the perceptron: every weight moves by a part of its input, towards the right answer
        int32_t answer = has_slept ? 1 : -1;
        for (uint32_t i = 0; i < INPUTS; ++i) {
            int32_t step = (answer * get_byte(proc->inputs, i)) >> LEARN_SHIFT;
            weights = set_byte(weights, i, get_byte(weights, i) + step, WEIGHT_MIN, WEIGHT_MAX);
        }
    }

    // What the network will know about the process at its next turn
    int32_t sleeps = get_byte(proc->inputs, INPUT_SLEEPS) + (has_slept ? SLEEPS_UP : -SLEEPS_DOWN);
    uint32_t inputs = 0;
    inputs = set_byte(inputs, INPUT_ONE, INPUT_ONE_VALUE, INPUT_MIN, INPUT_MAX);
    inputs = set_byte(inputs, INPUT_TICKS, (int32_t) proc->turn_ticks * TICKS_SCALE, 0, INPUT_MAX);
    inputs = set_byte(inputs, INPUT_SLEEPS, sleeps, INPUT_MIN, INPUT_MAX);
    inputs = set_byte(inputs, INPUT_CALLS, proc->turn_calls > INPUT_MAX ? INPUT_MAX : (int32_t) proc->turn_calls * CALLS_SCALE, 0, INPUT_MAX);
    proc->inputs = inputs;
    proc->has_slept = has_slept;
}

void note_wakeup(process_t *proc)
{
    proc->is_woken = true;
    proc->woken_at = get_ticks();
}

void get_slices(uint32_t words[SLICES_WORDS])
{
    words[SLICES_WEIGHTS] = weights;
    words[SLICES_TURNS] = ends[TURN_END_SLEEP] + ends[TURN_END_TIMER];
    words[SLICES_RIGHT] = right_guesses;
    words[SLICES_SLEEPS] = ends[TURN_END_SLEEP];
}

void forget_slices(void)
{
    weights = 0;
}

void print_slices(void)
{
    /*
        The numbers depend on when the interrupts arrive, which differs
        between the emulator, the chip, and the board, so the kernel prints
        them only when it is built to measure (`SLICE_STATS`).
    */
#ifdef SLICE_STATS
    uint32_t judged = ends[TURN_END_SLEEP] + ends[TURN_END_TIMER];
    k_printf("Turns: %u, ended by a sleep %u, by the timer %u, by the end of the process %u\n",
             judged + ends[TURN_END_EXIT], ends[TURN_END_SLEEP], ends[TURN_END_TIMER], ends[TURN_END_EXIT]);
    k_printf("Right guesses: %u of %u by the network, %u by the last turn, %u by \"never sleeps\"\n",
             right_guesses, judged, right_repeats, ends[TURN_END_TIMER]);
    k_printf("Long turns that the timer has ended: %u\n", wrong_long);
    k_printf("Waits for the CPU after a wakeup: %u, %u timer interrupts in all, %u at the most\n",
             wakeups, wait_ticks, longest_wait);
    k_printf("Weights at the end: %d %d %d %d (%p), timer interrupts: %u\n",
             get_byte(weights, 0), get_byte(weights, 1), get_byte(weights, 2), get_byte(weights, 3), weights, get_ticks());
#endif
    k_printf("The network of the scheduler: dot4 and C gave the same sum %s\n", sums > 0 ? "every time" : "0 times");
}
