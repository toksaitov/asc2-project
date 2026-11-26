/*
    A neuron that learns to tell two letters apart.

    The letters T and L are pictures of 4 by 3 pixels, and every pixel of
    them is turned around with a chance of one in eight. The neuron has an
    input for every pixel: it adds the twelve products of the inputs and its
    weights, and the sign of the sum is its answer. The answer is then
    compared with the right one, and every weight moves a little. Nobody
    tells the program how the letters look. Every `SWAP` letters the teacher
    changes the names of the two: T is called L and L is called T, the
    neuron is wrong, and it learns anew.

    The array of the chip does the sum (`dotv`) and the correction of the
    weights (`learn`); the button changes to plain C code for both and
    back, and the neuron starts from zero. The screen shows the last letter,
    the twelve weights (the size of a square is that of the weight, a frame
    is a weight below zero), the time of one step, and the mistakes: a bar
    for every four letters. From time to time both ways do the same step,
    and the program counts the numbers that differ. The button held down for
    two seconds ends the program.
*/

#include "lib/machine.h"

#define LEAVE_US 2000000 /* the button held for this long ends the program */

#define CELLS    12
#define ROWS     3
#define INPUT    16   /* the input of a lit pixel; a dark one is -INPUT */
#define GOAL     2048 /* the sum that the neuron should reach for a T, and -GOAL for an L */
#define RATE     8    /* the error is the distance to the goal in parts of 2^RATE */
#define CHUNK    4    /* letters for a bar and a picture of the screen */
#define CHECKS   8    /* bars between two steps that both ways do */
#define SWAP     64   /* letters after which the names change */
#define BARS     32
#define BAR_W    4
#define BAR_H    4    /* rows of a bar for a mistake */

/* The functions of `lib/array.s` */

void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void get_weights(unsigned int *words);
void set_inputs(const unsigned int *words);
int add_products(void);
void correct_weights(unsigned int errors);

/* The two letters, the left top pixel in bit 0, four pixels in a row */
#define LETTER_T 0x66f
#define LETTER_L 0xf11

/* A word of inputs for every four pixels of a letter */
#define B(bit, place) ((unsigned int) ((bit) ? INPUT : 0x100 - INPUT) << (place))
#define W(n) (B((n) & 1, 0) | B((n) & 2, 8) | B((n) & 4, 16) | B((n) & 8, 24))
static const unsigned int row_words[16] = {
    W(0), W(1), W(2), W(3), W(4), W(5), W(6), W(7), W(8), W(9), W(10), W(11), W(12), W(13), W(14), W(15)
};

static unsigned int inputs[ROWS];  /* the letter of this step; cell i is byte i */
static unsigned int weights[ROWS]; /* the weights of the plain C way */

static unsigned int seed = 0x2545f491;
static unsigned int letter;        /* the pixels of the letter of this step */
static int is_t;                   /* it is a T, or an L that is called a T */
static int is_swapped;
static int answer;                 /* what the neuron has said */
static int is_in_c;                /* the way: plain C, or the array */

static unsigned int compared;      /* numbers that both ways have computed */
static unsigned int differ;        /* numbers that were not the same */

static unsigned char bars[BARS];   /* mistakes of the last bars */

static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

static void make_letter(void)
{
    unsigned int r = rnd();
    unsigned int s = rnd();
    int real_t = (int) (s >> 31);

    letter = ((real_t ? LETTER_T : LETTER_L) ^ (r & (r >> 12) & s)) & 0xfff;
    is_t = real_t != is_swapped;
    inputs[0] = row_words[letter & 15];
    inputs[1] = row_words[(letter >> 4) & 15];
    inputs[2] = row_words[letter >> 8];
}

static int add_in_c(void)
{
    const signed char *weight = (const signed char *) weights;
    const signed char *input = (const signed char *) inputs;
    int sum = 0;
    int i;

    for (i = 0; i < CELLS; ++i) {
        sum += weight[i] * input[i];
    }

    return sum;
}

static void correct_in_c(int error)
{
    signed char *weight = (signed char *) weights;
    const signed char *input = (const signed char *) inputs;
    int i;

    for (i = 0; i < CELLS; ++i) {
        int value = (weight[i] * 128 + 64 + error * input[i]) >> 7;
        if (value > 127) {
            value = 127;
        }
        if (value < -128) {
            value = -128;
        }
        weight[i] = (signed char) value;
    }
}

static int get_error(int sum)
{
    int error = ((is_t ? GOAL : -GOAL) - sum) >> RATE;

    if (error > 127) {
        error = 127;
    }
    if (error < -128) {
        error = -128;
    }

    return error;
}

/* One step of the neuron: the answer to the letter, and the correction. Returns 1 for a mistake. */
static int step(void)
{
    int sum;

    if (is_in_c) {
        sum = add_in_c();
        correct_in_c(get_error(sum));
    } else {
        unsigned int error;
        set_inputs(inputs);
        sum = add_products();
        error = (unsigned int) get_error(sum) & 0xff;
        correct_weights(error | error << 8 | error << 16);
    }
    answer = sum > 0;

    return answer != is_t;
}

/* One step in both ways from the same weights; the sums and all weights must be the same */
static int step_and_compare(void)
{
    unsigned int chip[ROWS];
    unsigned int error;
    int sum_in_array;
    int sum_in_c;
    int i;

    if (is_in_c) {
        set_weights(weights[0], weights[1], weights[2]);
    } else {
        get_weights(weights);
    }

    set_inputs(inputs);
    sum_in_array = add_products();
    sum_in_c = add_in_c();
    ++compared;
    if (sum_in_array != sum_in_c) {
        ++differ;
    }

    error = (unsigned int) get_error(sum_in_array) & 0xff;
    correct_weights(error | error << 8 | error << 16);
    get_weights(chip);
    correct_in_c(get_error(sum_in_c));
    for (i = 0; i < CELLS; ++i) {
        ++compared;
        if (((signed char *) chip)[i] != ((signed char *) weights)[i]) {
            ++differ;
        }
    }
    answer = sum_in_array > 0;

    return answer != is_t;
}

static void show_letter(void)
{
    int i;
    unsigned int bits = letter;

    scr_rect(0, 0, 34, 26, SCR_LIGHT);
    scr_rect(1, 1, 32, 24, SCR_DARK);
    for (i = 0; i < CELLS; ++i) {
        if (bits & 1) {
            scr_rect(1 + (i & 3) * 8, 1 + (i >> 2) * 8, 8, 8, SCR_LIGHT);
        }
        bits >>= 1;
    }
}

static void show_weights(void)
{
    signed char *weight = (signed char *) weights;
    int i;

    if (!is_in_c) {
        get_weights(weights);
    }
    scr_rect(41, 1, 32, 24, SCR_DARK);
    for (i = 0; i < CELLS; ++i) {
        int size = weight[i] < 0 ? -weight[i] : weight[i];
        size = size >= 32 ? 8 : (size + 3) >> 2;
        if (size != 0) {
            int x = 41 + (i & 3) * 8 + ((8 - size) >> 1);
            int y = 1 + (i >> 2) * 8 + ((8 - size) >> 1);
            scr_rect(x, y, size, size, SCR_LIGHT);
            if (weight[i] < 0) {
                scr_rect(x + 1, y + 1, size - 2, size - 2, SCR_DARK);
            }
        }
    }
}

static void show_bars(void)
{
    int i;

    scr_rect(0, 46, SCR_WIDTH, 18, SCR_DARK);
    scr_rect(0, 63, SCR_WIDTH, 1, SCR_LIGHT);
    for (i = 0; i < BARS; ++i) {
        int height = bars[i] * BAR_H;
        if (height != 0) {
            scr_rect(i * BAR_W, 63 - height, BAR_W - 1, height, SCR_LIGHT);
        }
    }
}

static void add_bar(int mistakes)
{
    int i;

    for (i = 1; i < BARS; ++i) {
        bars[i - 1] = bars[i];
    }
    bars[BARS - 1] = (unsigned char) mistakes;
}

static int is_held(unsigned int *since, int *was_down)
{
    if (RP_BTN_STAT == 0) {
        *was_down = 0;
    } else if (!*was_down) {
        *was_down = 1;
        *since = RP_TIME;
    } else if (RP_TIME - *since > LEAVE_US) {
        return 1;
    }

    return 0;
}

static unsigned int total_steps[2]; /* what the two ways have done: the array, then plain C */
static unsigned int total_us[2];
static unsigned int total_letters[2];
static unsigned int total_mistakes[2];

static void report(void)
{
    int i;

    for (i = 0; i < 2; ++i) {
        if (total_steps[i] != 0) {
            put_str(i != 0 ? "plain C: " : "array: ");
            put_dec(total_letters[i]);
            put_str(" letters, ");
            put_dec(total_mistakes[i]);
            put_str(" mistakes, a step in ");
            put_dec(total_us[i] / total_steps[i]);
            put_str(" us\n");
        }
    }
    put_str("array against C: ");
    put_dec(compared);
    put_str(" numbers compared, ");
    put_dec(differ);
    put_str(" different\n");
}

int main(void)
{
    unsigned int presses;
    unsigned int since = 0;
    int was_down = 0;
    int is_leaving = 0;
    int i;

    if (scr_open() != 0) {
        put_str("learn: another program has the screen\n");
        return 1;
    }
    put_str("learn: a press of the button changes the way; hold it for two seconds to end\n");

    presses = RP_BTN_COUNT;
    while (!is_leaving) {
        unsigned int chunks = 0;
        unsigned int letters = 0;
        unsigned int last_us = 0;

        weights[0] = weights[1] = weights[2] = 0;
        set_weights(0, 0, 0);
        is_swapped = 0;
        for (i = 0; i < BARS; ++i) {
            bars[i] = 0;
        }

        scr_clear();
        scr_text(4, 0, "letter  weights");
        scr_text(0, 14, is_in_c ? "plain C" : "array");
        while (!is_leaving && RP_BTN_COUNT == presses) {
            int mistakes = 0;

            for (i = 0; i < CHUNK; ++i) {
                make_letter();
                if (i == 0 && chunks % CHECKS == CHECKS - 1) {
                    mistakes += step_and_compare();
                } else {
                    unsigned int start = RP_TIME;
                    mistakes += step();
                    total_us[is_in_c] += last_us = RP_TIME - start;
                    ++total_steps[is_in_c];
                }
                ++letters;
                if (letters % SWAP == 0) {
                    is_swapped = !is_swapped;
                }
            }
            total_letters[is_in_c] += CHUNK;
            total_mistakes[is_in_c] += (unsigned int) mistakes;
            ++chunks;

            add_bar(mistakes);
            show_letter();
            show_weights();
            show_bars();
            scr_rect(84, 8, 44, 32, SCR_DARK);
            i = scr_dec(1, 14, last_us);
            scr_text(1, i, "us");
            scr_text(2, 14, is_swapped ? "swapped" : "");
            scr_text(3, 14, answer ? "says T" : "says L");
            scr_text(4, 14 + 1, differ != 0 ? "DIFFER" : "same");
            scr_show();

            is_leaving = is_held(&since, &was_down);
        }
        presses = RP_BTN_COUNT;
        is_in_c = !is_in_c;
    }

    scr_close();
    report();

    return differ == 0 ? 0 : 1;
}
