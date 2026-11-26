/*
    A program that checks the array of the dot products from the side of a
    process.

    The array of the chip has three rows of four cells. A cell multiplies a
    weight by an input, both signed bytes. The weights are in the registers
    `w0`-`w2` and the inputs in `x0`-`x2`, a word for a row, with the first
    cell in the low byte. `dotv` adds the twelve products to a number, `learn`
    changes the weights, and `xsh` moves the inputs on by one cell.

    The registers keep their values from one instruction to the next, and
    the chip has one set of them. The kernel gives every process its own
    values: it stores and loads the registers when another process asks for
    the array (`sys/proc/array.c` of the kernel). So a program starts with
    zeros, a child has the values of its parent, and the timer may interrupt
    a process at any instruction and let another process use the array in
    between.

    The program makes a child, so that two processes use the array in turns.
    Both run the same steps on the chip and on a model in C, with numbers
    that follow from a start value, and compare.
*/

#include "user.h"

#include "check.h"

#define ROWS  3
#define CELLS (4 * ROWS)

#define ROUNDS_PER_CHECK 8
#define ARRAY_TICKS 16 // timer interrupts during which a process computes

/* The functions of `array_asm.s` */

void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void set_inputs(unsigned int x0, unsigned int x1, unsigned int x2);
void get_array(unsigned int *regs);
int add_products(int sum);
void learn(unsigned int errors);
void shift_inputs(unsigned int input);

/* The model: the weights and the inputs of the cells, the first cell of the first row first */

static signed char weights[CELLS];
static signed char inputs[CELLS];

// Sets the four cells of every row from the bytes of a word, the first cell from the low byte.
static void set_cells(signed char *cells, const unsigned int *words)
{
    for (int i = 0; i < CELLS; ++i) {
        cells[i] = (signed char) (words[i / 4] >> (8 * (i % 4)));
    }
}

// Returns the word of a row: its four cells as bytes, the first cell in the low byte.
static unsigned int get_word(const signed char *cells, int row)
{
    unsigned int word = 0;
    for (int i = 0; i < 4; ++i) {
        word |= (unsigned int) (unsigned char) cells[4 * row + i] << (8 * i);
    }

    return word;
}

// What `dotv` does: returns `sum` plus the twelve products.
static int add_model_products(int sum)
{
    for (int i = 0; i < CELLS; ++i) {
        sum += weights[i] * inputs[i];
    }

    return sum;
}

/*
    What `learn` does. Byte k of `errors` is the error of row k, in parts of
    128. Every weight of the row gets the product of the error and its input,
    divided by 128 and rounded, and stays within a signed byte.
*/
static void learn_in_model(unsigned int errors)
{
    for (int i = 0; i < CELLS; ++i) {
        int error = (signed char) (errors >> (8 * (i / 4)));
        int weight = (weights[i] * 128 + 64 + error * inputs[i]) >> 7;
        weights[i] = (signed char) (weight < -128 ? -128 : weight > 127 ? 127 : weight);
    }
}

// What `xsh` does: every input moves on by one cell, the last one is dropped, and the first cell gets the low byte of `input`.
static void shift_model_inputs(unsigned int input)
{
    for (int i = CELLS - 1; i > 0; --i) {
        inputs[i] = inputs[i - 1];
    }
    inputs[0] = (signed char) input;
}

// Returns 1 if the registers of the chip have the values of the model.
static int is_array_as_model(void)
{
    unsigned int regs[2 * ROWS];
    get_array(regs);

    for (int row = 0; row < ROWS; ++row) {
        if (regs[row] != get_word(weights, row) || regs[ROWS + row] != get_word(inputs, row)) {
            return 0;
        }
    }

    return 1;
}

/*
    Returns the number that follows `number` in a series that looks random
    and never reaches 0 (the xorshift generator of Marsaglia).
*/
static unsigned int next_number(unsigned int number)
{
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    return number;
}

/*
    Uses the array and the model in the same way until the timer has
    interrupted the machine `ticks` times: new inputs come in, a sum of
    products is added to the sum before it, and the weights learn from the
    sum. Returns 0 if the sums and the registers were equal all the time.
*/
static int compute(unsigned int number, int ticks)
{
    int start = times(NULL);

    unsigned int words[2 * ROWS];
    for (int i = 0; i < 2 * ROWS; ++i) {
        words[i] = number = next_number(number);
    }
    set_cells(weights, words);
    set_cells(inputs, words + ROWS);
    set_weights(words[0], words[1], words[2]);
    set_inputs(words[3], words[4], words[5]);

    int sum = 0;
    int model_sum = 0;
    do {
        for (int i = 0; i < ROUNDS_PER_CHECK; ++i) {
            number = next_number(number);

            shift_inputs(number);
            shift_model_inputs(number);

            sum = add_products(sum);
            model_sum = add_model_products(model_sum);
            if (sum != model_sum) {
                return 1;
            }

            // The errors are small in most rounds, so that the weights do not stay at their ends
            unsigned int errors = number >> 8;
            if (number & 0x80) {
                errors &= 0x070707;
            }
            learn(errors);
            learn_in_model(errors);
        }
        if (!is_array_as_model()) {
            return 1;
        }
    } while (times(NULL) - start < ticks);

    return 0;
}

// Returns 1 if every register of the array is 0.
static int is_array_empty(void)
{
    unsigned int regs[2 * ROWS];
    get_array(regs);

    for (int i = 0; i < 2 * ROWS; ++i) {
        if (regs[i] != 0) {
            return 0;
        }
    }

    return 1;
}

/*
    Ends a child with 0 if `is_right`, and with 1 if not. Returns the number
    of the child in the parent.
*/
static int fork_to_report(int is_right_in_child)
{
    int pid = fork();
    if (pid == 0) {
        exit(is_right_in_child ? 0 : 1);
    }

    return pid;
}

// Returns 1 if the child `pid` has ended with 0.
static int has_ended_well(int pid)
{
    int status = -1;

    return pid > 0 && waitpid(pid, &status, 0) == pid && status == 0;
}

int main(int argc, char *argv[], char *envp[])
{
    // The program was started again by itself: it tells if its registers are those of a new program
    if (argc > 1) {
        return is_array_empty() ? 0 : 1;
    }

    check(is_array_empty(), "a program starts with zeros in the array");

    // The squares of 1 to 12 give 650
    static const unsigned int COUNT[2 * ROWS] = { 0x04030201, 0x08070605, 0x0c0b0a09, 0x04030201, 0x08070605, 0x0c0b0a09 };
    set_cells(weights, COUNT);
    set_cells(inputs, COUNT + ROWS);
    set_weights(COUNT[0], COUNT[1], COUNT[2]);
    set_inputs(COUNT[3], COUNT[4], COUNT[5]);
    check(add_products(100) == 750 && is_array_as_model(), "dotv adds twelve products to a number");

    // A new input of -2 pushes the 12 out, and every weight meets the input that was one cell before it: 650 - 78 - 2 = 570
    shift_inputs(0xfe);
    shift_model_inputs(0xfe);
    check(add_products(0) == 570 && add_model_products(0) == 570 && is_array_as_model(), "xsh moves the inputs on by one cell");

    // An error of 64 is a half: the first weight gets a half of its input, -1, and the last one a half of 11, rounded up
    learn(0x404040);
    learn_in_model(0x404040);
    check(weights[0] == 0 && weights[CELLS - 1] == 18 && is_array_as_model(), "learn moves the weights by the error times the input");

    // The chip holds the values of this process when the child is made
    int pid = fork();
    if (pid == 0) {
        int is_right = is_array_as_model();
        set_weights(0x11111111, 0x22222222, 0x33333333);
        set_inputs(0, 0, 0);
        exit(is_right ? 0 : 1);
    }
    check(has_ended_well(pid), "a child has the values of its parent");

    // The chip holds the values of nobody now: the child has written its own and has ended
    pid = fork();
    if (pid == 0) {
        exit(is_array_as_model() ? 0 : 1);
    }
    int has_ended = has_ended_well(pid);
    check(has_ended && is_array_as_model(), "the next child has them too, and the parent keeps them: what a child writes is its own");

    // A program that starts gets no value of the program before it, also when the chip holds those values
    pid = fork();
    if (pid == 0) {
        set_weights(0x55555555, 0x66666666, 0x77777777);
        char *args[] = { argv[0], "again", NULL };
        execve(argv[0], args, envp);
        exit(1);
    }
    check(has_ended_well(pid) && is_array_as_model(), "a program that a process starts has zeros, and the process before it keeps its values");

    tms_t before;
    int start = times(&before);

    pid = fork();
    if (pid == 0) {
        exit(compute(0x2545f491, ARRAY_TICKS));
    }

    int result = compute(0x9e3779b9, ARRAY_TICKS);

    // Each of the two has run for less time than has passed, so the other one ran in between
    tms_t after;
    int passed = times(&after) - start;

    int status = -1;
    int waited_pid = waitpid(pid, &status, 0);

    check(result == 0, "the timer has interrupted the work with the array, and the sums and the weights are right");
    check(pid > 0 && waited_pid == pid && status == 0, "a child has used the array for its own sums at the same time");
    check(after.utime > before.utime && (int) (after.utime - before.utime) < passed, "the two processes took turns in the middle of their work");

    return get_result();
}
