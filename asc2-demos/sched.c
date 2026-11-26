/*
    Sched: the neuron in the scheduler of the kernel, seen at work.

    The kernel gives the CPU to the programs in turns. Before every turn it
    guesses: will the program stop by itself to wait for something (the
    disk, say), or will it run on until the timer takes the CPU away? A
    program that will wait gets a long turn, and one that runs on a short
    one, so that the others do not wait long behind it. The guess comes
    from a neuron of four inputs, and the neuron learns from every wrong
    guess (`sys/proc/slice.c` of the kernel).

    This program gives the kernel something to guess about: three children
    of three kinds. One computes without a pause. One works a little and
    then sleeps, again and again, as a program does that waits for a key or
    for the disk. One computes between two sleeps, for a short time or for
    several turns as a coin falls, so that nobody can guess it every time:
    a guess that is right nine times of ten is a good one here. How much
    work a turn holds is measured first, so that
    the children behave the same on a fast machine and on a slow one. Then
    the program shows what the
    kernel tells about its neuron: how many of the last hundred guesses
    were right, each guess as a column (a gap is a wrong one), and the four
    weights that the neuron has learned.

    A press of the button makes the neuron forget everything, and it learns
    anew in front of the eyes. The button held down for two seconds ends
    the program.

    The program runs under the kernel `asc2-rach2` only.
*/

#include "user.h"

#include "lib/machine.h"
#include "lib/show.h"

#define WORKERS    3
#define STOP_FILE  "sched.stop" /* the children end when this file is there */
#define TRY_SUM    400          /* numbers that are added between two looks at the clock while a period of the timer is measured */
#define LONG_TICKS 6            /* periods of the timer that the mixed child computes when it computes for long */
#define LAST_TICKS 20000        /* a child ends by itself after this many timer interrupts, whatever happens to its parent */

#define RECENT     100          /* guesses that the picture shows */
#define HIDDEN     28           /* guesses that the picture keeps and does not show: 128 less those */
#define BAR_X      88           /* the column where a weight of zero ends */
#define TOLD_FRAMES 12          /* pictures that say that the neuron has forgotten */

/* The function of `sched_asm.s` */

int count_ones(unsigned int word);

/* The last 128 guesses, a bit for each, 1 for a right one; the newest is the highest bit of the last word */
static unsigned int guesses[4];
static unsigned int kept; /* how many of the last hundred places hold a guess: fewer than a hundred at the start */

/* The picture of the guesses: 6 rows of 128 pixels, each the bits above */
static unsigned int strip[1 + 4 * 6];

static int is_stopped(void)
{
    stat_t info;

    return stat(STOP_FILE, &info) == 0;
}

/* Adds the numbers from 1 to `count`; the sum itself is not used. */
static void add_numbers(unsigned int count)
{
    volatile unsigned int sum = 0;
    unsigned int i;

    for (i = 1; i <= count; ++i) {
        sum += i;
    }
}

/* Returns about how many numbers the machine adds in a period of the timer. */
static unsigned int measure_tick(void)
{
    unsigned int count = 0;
    int start = times(NULL);

    while (times(NULL) == start) {
    }
    start = times(NULL);
    while (times(NULL) == start) {
        add_numbers(TRY_SUM);
        count += TRY_SUM;
    }

    return count;
}

/*
    The work of a child; it ends when the stop file appears. A look for the
    file takes the kernel longer than a short piece of work, so a child
    looks only now and then. `tick_sum` is how many numbers the machine
    adds in a period of the timer.
*/
static void work(int kind, unsigned int tick_sum)
{
    int start = times(NULL);
    unsigned int coin = 0x1234567 + (unsigned int) start;
    unsigned int passes = 0;
    int i;

    for (;;) {
        ++passes;
        if (kind == 0) {
            /* Computes without a pause, and looks for the file after two periods of the timer */
            add_numbers(tick_sum >> 2);
            if ((passes & 7) != 0) {
                continue;
            }
        } else if (kind == 1) {
            /* Works a little and sleeps, and looks for the file after four sleeps */
            add_numbers(tick_sum >> 4);
            nap(1);
            if ((passes & 3) != 0) {
                continue;
            }
        } else {
            coin ^= coin << 13;
            coin ^= coin >> 17;
            coin ^= coin << 5;
            if ((coin & 0x100) != 0) {
                /* The same as the child before, */
                add_numbers(tick_sum >> 4);
            } else {
                /* or computes for several turns before the sleep, as the coin falls */
                for (i = 0; i < 4 * LONG_TICKS; ++i) {
                    add_numbers(tick_sum >> 2);
                }
            }
            nap(1);
            if ((passes & 1) != 0) {
                continue;
            }
        }
        if (times(NULL) - start > LAST_TICKS || is_stopped()) {
            break;
        }
    }
}

/* Tells the children to end, and waits for them. */
static void stop_workers(const int *pids)
{
    int fd = creat(STOP_FILE, 0644);
    int i;

    if (fd >= 0) {
        close(fd);
    }
    for (i = 0; i < WORKERS; ++i) {
        if (pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
    }
    unlink(STOP_FILE);
}

/* Takes `count` new guesses, all right or all wrong, behind the last ones. */
static void note_guesses(unsigned int count, int is_right)
{
    kept = kept + count > RECENT ? RECENT : kept + count;
    while (count > 0) {
        unsigned int some = count > 16 ? 16 : count;

        guesses[0] = guesses[0] >> some | guesses[1] << (32 - some);
        guesses[1] = guesses[1] >> some | guesses[2] << (32 - some);
        guesses[2] = guesses[2] >> some | guesses[3] << (32 - some);
        guesses[3] = guesses[3] >> some | (is_right ? 0xffffffffu << (32 - some) : 0);
        count -= some;
    }
}

/* Draws a weight in its row: a bar from the middle, to the right if the weight is above zero. */
static void show_weight(int row, int weight)
{
    int length = weight * 5 / 8; /* 64 is 40 pixels */

    if (length > 0) {
        scr_rect(BAR_X, 8 * row + 1, length, 5, SCR_LIGHT);
    } else if (length < 0) {
        scr_rect(BAR_X + length, 8 * row + 1, -length, 5, SCR_LIGHT);
    }
}

/* Draws what does not change from a picture to the next. */
static void show_names(void)
{
    scr_clear();
    scr_text(0, 0, "SCHED  guesses right:");
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_FLIP);
    scr_text(3, 0, "always");
    scr_text(4, 0, "ran long");
    scr_text(5, 0, "waited");
    scr_text(6, 0, "calls");
    scr_text(7, 0, "press:forget hold:end");
}

/* Draws what changes: the guesses, how many of them are right, and the weights. */
static void show_neuron(unsigned int weights, int is_told)
{
    int right = count_ones(guesses[0] >> HIDDEN) + count_ones(guesses[1]) + count_ones(guesses[2]) + count_ones(guesses[3]);
    int col;
    int i;

    /* The last hundred guesses, the newest at the right: a lit column for a right one, a gap for a wrong one */
    strip[0] = SCR_WIDTH | 6u << 16;
    for (i = 0; i < 4 * 6; ++i) {
        strip[1 + i] = guesses[i & 3];
    }
    scr_blit(strip, -HIDDEN, 9, SCR_COPY);

    /* Beside them: how many of a hundred are right */
    scr_text(1, 17, "    ");
    if (kept > 0) {
        col = scr_dec(1, 17, kept == RECENT ? (unsigned int) right : (unsigned int) right * 100 / kept);
        scr_text(1, col, "%");
    }

    scr_text(2, 0, is_told ? "it forgot; it learns:" : "what counts for it:  ");
    scr_rect(48, 24, SCR_WIDTH - 48, 32, SCR_DARK);
    show_weight(3, (signed char) weights);
    show_weight(4, (signed char) (weights >> 8));
    show_weight(5, (signed char) (weights >> 16));
    show_weight(6, (signed char) (weights >> 24));
    scr_rect(BAR_X, 24, 1, 32, SCR_FLIP);
    scr_show();
}

int main(int argc, char *argv[])
{
    unsigned int words[SLICES_WORDS];
    unsigned int first_turns;
    unsigned int first_right;
    unsigned int turns;
    unsigned int right;
    unsigned int tick_sum;
    int pids[WORKERS];
    int event = SHOW_NONE;
    int told = 0;
    int i;

    if (slices(SLICES_READ, words) != 0) {
        put_str("sched: this kernel does not tell about its scheduler\n");
        return 1;
    }

    /* The children start before the program takes the screen: the screen is its own */
    unlink(STOP_FILE);
    tick_sum = measure_tick();
    for (i = 0; i < WORKERS; ++i) {
        pids[i] = 0;
    }
    for (i = 0; i < WORKERS; ++i) {
        pids[i] = fork();
        if (pids[i] == 0) {
            work(i, tick_sum);
            exit(0);
        }
        if (pids[i] < 0) {
            put_str("sched: the machine has too little free memory for the three programs\n");
            stop_workers(pids);
            return 1;
        }
    }

    if (scr_open() != 0) {
        put_str("sched: another program has the screen\n");
        stop_workers(pids);
        return 1;
    }
    put_str("sched: a press of the button makes the neuron forget; hold it for two seconds to end\n");

    if (show_title("SCHED", "Before each turn of a\nprogram the kernel\nguesses: will it stop\nto wait? A neuron in\nit guesses and learns.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }
    show_tour(argc, 0, 26);

    slices(SLICES_READ, words);
    first_turns = turns = words[SLICES_TURNS];
    first_right = right = words[SLICES_RIGHT];
    show_names();
    while (event != SHOW_LEAVE) {
        unsigned int new_turns;
        unsigned int new_right;

        slices(SLICES_READ, words);
        new_turns = words[SLICES_TURNS] - turns;
        new_right = words[SLICES_RIGHT] - right;
        turns = words[SLICES_TURNS];
        right = words[SLICES_RIGHT];
        note_guesses(new_turns - new_right, 0);
        note_guesses(new_right, 1);

        show_neuron(words[SLICES_WEIGHTS], told > 0);
        if (told > 0) {
            --told;
        }

        event = show_button();
        if (event == SHOW_NEXT) {
            slices(SLICES_FORGET, words);
            told = TOLD_FRAMES;
        }
    }

    scr_clear();
    show_header("SCHED");
    scr_text(3, 0, "the three children");
    scr_text(4, 0, "are ending...");
    scr_show();
    stop_workers(pids);
    scr_close();

    turns -= first_turns;
    right -= first_right;
    put_str("The kernel guessed about ");
    put_dec(turns);
    put_str(" turns of the programs while sched ran, and ");
    put_dec(right);
    put_str(" guesses were right");
    if (turns > 0) {
        put_str(": ");
        put_dec(right * 100 / turns);
        put_str(" of 100");
    }
    put_str(". Its neuron learns from every wrong guess.\n");

    return 0;
}
