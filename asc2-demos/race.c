/*
    Races between the accelerator of the chip and plain code.

    A race gives the same job to two pieces of code, both written by hand
    (`race_asm.s`): one uses the instructions of the array of the dot
    products, the other the instructions that the chip had before them. The
    two work in turns, each for the same short time, and a bar for each
    shows the work that is done. When the bar of the accelerator is full,
    the race ends, and the screen says how many times faster it was. Before
    a race both do the same piece of the job, and their answers are
    compared.

    * Learning: a neuron of twelve inputs learns a rule from examples. A
      step is a guess and the correction of all twelve weights.
    * Thinking: a small network of four inputs, three neurons, and one
      output answers questions.

    A press of the button goes on to the next picture; the button held down
    for two seconds ends the program, and the console gets the results.
*/

#include "lib/machine.h"
#include "lib/show.h"

#define RACES    2
#define UNITS    32      /* steps or answers in one call of a job */
#define SLICE_US 120000  /* how long a side works before the other one has its turn */
#define ROUNDS   25      /* turns of the accelerator in a race */
#define REST_US  6000000 /* how long a result stays */

/* The functions of `race_asm.s` and `lib/array.s` */

int learn_fast(const unsigned int *examples, int count);
int learn_plain(const unsigned int *examples, int count, signed char *weights);
int think_fast(const unsigned int *samples, int count, unsigned int output);
int think_plain(const unsigned int *samples, int count, unsigned int output, const unsigned int *hidden);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void get_weights(unsigned int *words);

static const char *const names[RACES] = { "RACE 1: LEARNING", "RACE 2: THINKING" };
static const char *const whats[RACES] = { "steps of learning", "answers of a network" };
static const char *const words[RACES] = { "learning", "thinking" };

static unsigned int examples[4 * UNITS]; /* for learning: the twelve inputs in three words, and the right answer */
static unsigned int samples[UNITS];      /* for thinking: the four inputs of a question */
static unsigned int hidden[3];           /* the weights of the three neurons of the network */
static unsigned int output;              /* and of its output */
static unsigned int plain_weights[3];    /* the twelve weights of the neuron that learns in plain code */

static unsigned int done_fast[RACES];    /* what the last race of a kind has shown */
static unsigned int done_plain[RACES];
static unsigned int tenths[RACES];       /* how many times faster the accelerator was, times ten; 0 before the race */
static int is_same[RACES];               /* the answers of both sides were the same */

static unsigned int seed = 0x2545f491;

static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

/* A word of four signed bytes between -63 and 63 */
static unsigned int rnd_word(void)
{
    unsigned int word = 0;
    int i;

    for (i = 0; i < 4; ++i) {
        unsigned int r = rnd();
        word |= (((r & 63) + ((r >> 6) & 63) - 63) & 0xff) << (8 * i);
    }

    return word;
}

/* The examples follow a rule that the neuron does not know: twelve weights, and the answer is the sum of the products, in 128ths */
static void make_data(void)
{
    unsigned int rule[3];
    int i;
    int j;

    for (i = 0; i < 3; ++i) {
        rule[i] = rnd_word();
        hidden[i] = rnd_word();
    }
    output = rnd_word() & 0xffffff;
    for (i = 0; i < UNITS; ++i) {
        int sum = 0;
        for (j = 0; j < 3; ++j) {
            examples[4 * i + j] = rnd_word();
        }
        for (j = 0; j < 12; ++j) {
            sum += ((signed char *) rule)[j] * ((signed char *) &examples[4 * i])[j];
        }
        examples[4 * i + 3] = (unsigned int) (sum >> 7);
        samples[i] = rnd_word();
    }
}

static void get_ready(int race)
{
    if (race == 0) {
        plain_weights[0] = plain_weights[1] = plain_weights[2] = 0;
        set_weights(0, 0, 0);
    } else {
        set_weights(hidden[0], hidden[1], hidden[2]);
    }
}

static int work_fast(int race)
{
    return race == 0 ? learn_fast(examples, UNITS) : think_fast(samples, UNITS, output);
}

static int work_plain(int race)
{
    return race == 0 ? learn_plain(examples, UNITS, (signed char *) plain_weights)
                     : think_plain(samples, UNITS, output, hidden);
}

/* Both sides do the same work twice from the same start; returns 1 if every answer and every weight is the same */
static int have_same_answers(int race)
{
    unsigned int chip[3];
    int same = 1;
    int i;

    get_ready(race);
    for (i = 0; i < 2; ++i) {
        if (work_fast(race) != work_plain(race)) {
            same = 0;
        }
    }
    if (race == 0) {
        get_weights(chip);
        for (i = 0; i < 3; ++i) {
            if (chip[i] != plain_weights[i]) {
                same = 0;
            }
        }
    }

    return same;
}

/* Writes "11x" or "4.9x" for a number of tenths */
static void write_times(char *text, unsigned int value)
{
    if (value >= 100) {
        value = (value + 5) / 10;
        if (value >= 100) {
            *text++ = (char) ('0' + value / 100);
        }
        *text++ = (char) ('0' + value / 10 % 10);
        *text++ = (char) ('0' + value % 10);
    } else {
        *text++ = (char) ('0' + value / 10);
        *text++ = '.';
        *text++ = (char) ('0' + value % 10);
    }
    *text++ = 'x';
    *text = '\0';
}

/* One race; returns what the button did, or `SHOW_NONE` if the race and its result were left alone */
static int run_race(int race)
{
    unsigned int fast = 0;
    unsigned int plain = 0;
    unsigned int fast_us = 0;
    unsigned int plain_us = 0;
    unsigned int goal = 0;
    int event = SHOW_NONE;
    char text[8];

    scr_clear();
    show_header(names[race]);
    scr_text(1, 0, whats[race]);
    scr_text(2, 0, "accelerator");
    scr_text(4, 0, "plain code");
    show_bar(24, 0, 1);
    show_bar(40, 0, 1);
    scr_show();

    is_same[race] = have_same_answers(race);
    get_ready(race);

    while (event == SHOW_NONE && (goal == 0 || fast < goal)) {
        unsigned int start = RP_TIME;
        do {
            work_fast(race);
            fast += UNITS;
        } while (RP_TIME - start < SLICE_US);
        fast_us += RP_TIME - start;

        start = RP_TIME;
        do {
            work_plain(race);
            plain += UNITS;
        } while (RP_TIME - start < SLICE_US);
        plain_us += RP_TIME - start;

        if (goal == 0) {
            goal = fast * ROUNDS;
        }
        scr_dec(2, 14, fast);
        scr_dec(4, 14, plain);
        show_bar(24, fast, goal);
        show_bar(40, plain, goal);
        scr_show();
        event = show_button();
    }
    if (event != SHOW_NONE) {
        return event;
    }

    /* The work in the same time, so the two times count too */
    done_fast[race] = fast;
    done_plain[race] = plain;
    tenths[race] = fast * (plain_us / 1000) / (fast_us / 1000) * 10 / plain;

    write_times(text, tenths[race]);
    show_big(0, 49, text, 3);
    scr_text(6, 8, "times faster");
    scr_text(7, 8, is_same[race] ? "same answers" : "ANSWERS DIFFER");
    scr_show();

    return show_wait(REST_US);
}

static int show_result(void)
{
    char text[8];
    int race;

    scr_clear();
    show_header("THE ACCELERATOR IS");
    for (race = 0; race < RACES; ++race) {
        if (tenths[race] != 0) {
            write_times(text, tenths[race]);
            show_big(0, 12 + 18 * race, text, 3);
            scr_text(2 + 2 * race, 8, "faster at");
            scr_text(3 + 2 * race, 8, words[race]);
        }
    }
    scr_text(6, 0, "both written by hand");
    scr_text(7, 0, "press:again hold:end");
    scr_show();

    return show_wait(2 * REST_US);
}

static void report(void)
{
    int race;

    for (race = 0; race < RACES; ++race) {
        if (tenths[race] != 0) {
            put_str(race == 0 ? "Learning: the accelerator did " : "Thinking: the accelerator gave ");
            put_dec(done_fast[race]);
            put_str(race == 0 ? " steps while plain code did " : " answers while plain code gave ");
            put_dec(done_plain[race]);
            put_str(". That is ");
            put_dec(tenths[race] / 10);
            put_char('.');
            put_dec(tenths[race] % 10);
            put_str(is_same[race] ? " times faster, with the same answers.\n" : " times faster, but THE ANSWERS DIFFER.\n");
        }
    }
    put_str("Both sides are written by hand; plain code uses the chip without its array of dot products.\n");
}

int main(int argc, char *argv[])
{
    int is_leaving;

    if (scr_open() != 0) {
        put_str("race: another program has the screen\n");
        return 1;
    }
    put_str("race: a press of the button goes on; hold it for two seconds to end\n");

    make_data();
    is_leaving = show_title("RACE", "The same job, done\ntwice: with the AI\naccelerator and with\nplain code.",
                            "press:next  hold:end");
    show_tour(argc, 0, 38); /* both races and the result once */
    while (!is_leaving) {
        int race;
        for (race = 0; race < RACES && !is_leaving; ++race) {
            is_leaving = run_race(race) == SHOW_LEAVE;
        }
        if (!is_leaving) {
            is_leaving = show_result() == SHOW_LEAVE;
        }
    }

    scr_close();
    report();

    return is_same[0] && is_same[1] ? 0 : 1;
}
