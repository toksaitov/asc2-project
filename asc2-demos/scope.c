/*
    Scope: a filter of twelve numbers cleans a noisy signal.

    A signal comes in sample by sample: a level that jumps between high and
    low now and then, with noise on it. The upper trace shows it as it comes. The lower trace
    shows what a filter makes of it. A filter is twelve numbers: each new
    output is the last twelve samples multiplied by them and added up.

    The samples sit in the inputs of the array of the chip and the numbers
    of the filter in its weights. For every sample `xsh` moves the samples
    on by one place, and `dotv` does the twelve multiplications and the sum
    in one instruction.

    A press of the button changes the filter; the button held down for two
    seconds ends the program.

    * smooth: every number is the same, so the output is the average of
      the last twelve samples, and the noise goes.
    * jumps: the six newer numbers are positive and the six older ones
      negative, so the output is zero while the level stays and shows a
      peak where it jumps.
    * none: one number passes the newest sample through as it is.
*/

#include "lib/machine.h"
#include "lib/show.h"

#define RING       128 /* samples on the screen, a power of two */
#define STEP       1   /* new samples between two pictures */
#define LIMIT      104 /* the largest sample that is drawn: 13 rows from the middle */
#define TOP_MIDDLE 22  /* the row of a sample of 0 in the upper trace, */
#define LOW_MIDDLE 50  /* and in the lower one */
#define FILTERS    3

/* The functions of `scope_asm.s` and `lib/array.s` */

int scope_filter(int sample);
void scope_trace(volatile unsigned int *line, const signed char *ring, unsigned int start, int middle);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);

/* The numbers of a filter, four in a word, those for the newest samples first */
static const unsigned int filters[FILTERS][3] = {
    { 0x0b0b0b0b, 0x0b0b0b0b, 0x0b0b0b0b }, /* smooth: 12 times 11 of 128, the average */
    { 0x15151515, 0xebeb1515, 0xebebebeb }, /* jumps: 6 times 21, then 6 times -21 */
    { 0x0000007f, 0x00000000, 0x00000000 }  /* none: the newest sample alone */
};

static const char *const names[FILTERS] = {
    "SCOPE  filter: smooth",
    "SCOPE  filter: jumps",
    "SCOPE  filter: none"
};

static signed char incoming[RING]; /* the samples as they came, */
static signed char filtered[RING]; /* and after the filter */
static unsigned int oldest;        /* the place of the oldest sample in both */

static unsigned int seed = 0x9e3779b9;
static int level;                  /* the level of the signal now, */
static int stays;                  /* and for how many more samples */
static unsigned int samples;

static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

/* The next sample: a level that jumps between high and low after 24 to 55 samples, and noise of up to 24 on it */
static int next_sample(void)
{
    if (stays == 0) {
        level = level > 0 ? -56 : 56;
        stays = 24 + (int) (rnd() & 31);
    }
    --stays;

    return level + ((((int) (rnd() & 63) - 32) * 3) >> 2);
}

static int limit(int value)
{
    return value > LIMIT ? LIMIT : value < -LIMIT ? -LIMIT : value;
}

static void show_traces(int filter)
{
    scr_rect(0, 8, SCR_WIDTH, SCR_HEIGHT - 8, SCR_DARK);

    RP_SCR_MOVE = SCR_POINT(0, TOP_MIDDLE - (incoming[oldest] >> 3));
    scope_trace(&RP_SCR_LINE, incoming, oldest, TOP_MIDDLE);
    RP_SCR_MOVE = SCR_POINT(0, LOW_MIDDLE - (filtered[oldest] >> 3));
    scope_trace(&RP_SCR_LINE, filtered, oldest, LOW_MIDDLE);

    scr_text(1, 0, "in");
    scr_text(7, 0, "out");

    scr_rect(0, 0, SCR_WIDTH, 8, SCR_DARK);
    scr_text(0, 0, names[filter]);
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_FLIP);
    scr_show();
}

int main(int argc, char *argv[])
{
    int filter = 0;
    int event = SHOW_NONE;
    int i;

    if (scr_open() != 0) {
        put_str("scope: another program has the screen\n");
        return 1;
    }
    put_str("scope: a press of the button changes the filter; hold it for two seconds to end\n");

    if (show_title("SCOPE", "A noisy signal comes\nin (top). 12 numbers\nfilter it (bottom) in\none step of the chip\nfor every sample.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }
    show_tour(argc, 8, 25); /* each of the three filters once */
    seed ^= RP_TIME;
    set_weights(filters[0][0], filters[0][1], filters[0][2]);
    while (event != SHOW_LEAVE) {
        for (i = 0; i < STEP; ++i) {
            int sample = next_sample();
            incoming[oldest] = (signed char) limit(sample);
            filtered[oldest] = (signed char) limit(scope_filter(sample));
            oldest = (oldest + 1) & (RING - 1);
            ++samples;
        }
        show_traces(filter);

        event = show_button();
        if (event == SHOW_NEXT) {
            filter = filter == FILTERS - 1 ? 0 : filter + 1;
            set_weights(filters[filter][0], filters[filter][1], filters[filter][2]);
        }
    }

    scr_close();
    put_str("The filter took ");
    put_dec(samples);
    put_str(" samples. For each one the chip multiplied the last twelve samples by the twelve numbers of the filter and added the products, all in one instruction.\n");

    return 0;
}
