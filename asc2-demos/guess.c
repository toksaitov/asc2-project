/*
    Guess: the machine reads a person's mind.

    In every beat the person presses the button or does not. Before the
    beat ends, a neuron in the chip has guessed which it will be. A round
    that the chip has guessed right is a point for the chip, every other
    round a point for the person. People are poor at being random: who
    changes too often, repeats too often, or plays a pattern is read.

    The neuron has twelve inputs, two for each of the last six rounds: what
    the person chose, and what the person chose times whether the chip was
    right. They are a line of bytes in the array of the chip: `xsh` moves
    the line on by one place for a new byte, `dotv` adds the twelve
    products of the inputs and the weights, and the sign of the sum is the
    guess. After the round `learn` moves every weight toward the answer
    that would have been right. `../asc2-kb/models/guess.py` is a model of
    it.

    The button held down for two seconds ends the program.
*/

#include "lib/machine.h"
#include "lib/show.h"

#define LEAVE_US  2000000 /* the button held for this long ends the program */
#define ANSWER_US 1200000 /* the time of a beat */
#define REVEAL_US 900000  /* how long the result of a round stays */

#define INPUT 16   /* an input is this much for a press and minus this much for a wait */
#define GOAL  2048 /* the sum that the neuron should reach for a press, and -GOAL for a wait */
#define RATE  8    /* the error is the distance to the goal in parts of 2^RATE */

#define BAR_X 48   /* where the bars of the points start */
#define BAR_W 78

/* The functions of `guess_asm.s` */

int guess_sum(int start);
void guess_learn(unsigned int errors);
void guess_push(int value);

static int bias;             /* a thirteenth weight, for an input that is always INPUT; C keeps it */
static unsigned int rounds;
static unsigned int chip_points;
static unsigned int person_points;

static int is_leaving;
static int was_down;
static unsigned int down_since;

/* The tour shows the program with nobody at the button: then the program presses by itself, in two beats of every three */
static int is_on_tour;
static int tour_beat;

/* Looks at the button; a hold of two seconds ends the program */
static void watch_button(void)
{
    if (show_tour_over()) {
        is_leaving = 1;
    }
    if (RP_BTN_STAT == 0) {
        was_down = 0;
    } else if (!was_down) {
        was_down = 1;
        down_since = RP_TIME;
    } else if (RP_TIME - down_since > LEAVE_US) {
        is_leaving = 1;
    }
}

static void show_points(void)
{
    unsigned int most = rounds < 39 ? 39 : rounds;
    int col;

    scr_rect(0, 40, SCR_WIDTH, 16, SCR_DARK);
    scr_text(5, 0, "chip");
    col = scr_dec(5, 5, chip_points);
    scr_rect(BAR_X, 41, (int) (chip_points * BAR_W / most), 6, SCR_LIGHT);
    scr_text(6, 0, "you");
    col = scr_dec(6, 5, person_points);
    scr_rect(BAR_X, 49, (int) (person_points * BAR_W / most), 6, SCR_LIGHT);
}

/* One round; the chip guesses, the person chooses, and the chip learns */
static void play_round(void)
{
    int sum = guess_sum(bias * INPUT);
    int guess = sum > 0;
    int choice = 0;
    int is_right;
    int error;
    int value;
    unsigned int presses = RP_BTN_COUNT;
    unsigned int start = RP_TIME;
    unsigned int shown = 0;
    unsigned int now;

    ++rounds;
    tour_beat = tour_beat == 2 ? 0 : tour_beat + 1;
    scr_rect(0, 8, SCR_WIDTH, 32, SCR_DARK);
    scr_text(1, 0, "round");
    scr_dec(1, 6, rounds);
    scr_text(2, 0, "you:  ?");
    scr_text(3, 0, "chip: has guessed");
    show_bar(32, 0, 1);
    scr_show();

    /* The beat: the bar fills, and a press at any time in it is the choice */
    while ((now = RP_TIME - start) < ANSWER_US && !is_leaving) {
        watch_button();
        if (!choice && (RP_BTN_COUNT != presses || (is_on_tour && tour_beat != 0 && now > ANSWER_US / 2))) {
            choice = 1;
            scr_text(2, 6, "PRESS");
            shown = 0;
        }
        if (now - shown >= 100000 || shown == 0) {
            shown = now;
            show_bar(32, now, ANSWER_US);
            scr_show();
        }
    }
    if (is_leaving) {
        return;
    }

    is_right = guess == choice;
    if (is_right) {
        ++chip_points;
    } else {
        ++person_points;
    }

    /* The lesson: toward the sum that would have been right */
    error = ((choice ? GOAL : -GOAL) - sum) >> RATE;
    error = error > 127 ? 127 : error < -128 ? -128 : error;
    guess_learn(((unsigned int) error & 0xff) * 0x010101);
    value = (bias * 128 + 64 + error * INPUT) >> 7;
    bias = value > 127 ? 127 : value < -128 ? -128 : value;

    /* The round joins the line of the inputs: the choice in the first place, and before it the choice times "the chip was right" */
    value = choice ? INPUT : -INPUT;
    guess_push(is_right ? value : -value);
    guess_push(value);

    scr_rect(0, 16, SCR_WIDTH, 24, SCR_DARK);
    scr_text(2, 0, choice ? "you:  PRESS" : "you:  WAIT");
    scr_text(3, 0, guess ? "chip: PRESS" : "chip: WAIT");
    scr_text(4, 0, is_right ? "The chip read you." : "You fooled the chip.");
    show_points();
    scr_show();

    start = RP_TIME;
    while (RP_TIME - start < REVEAL_US && !is_leaving) {
        watch_button();
    }
}

int main(int argc, char *argv[])
{
    if (scr_open() != 0) {
        put_str("guess: another program has the screen\n");
        return 1;
    }
    put_str("guess: press the button or do not in every beat; hold it for two seconds to end\n");

    is_leaving = show_title("MIND READER", "In every beat, press\nthe button or don't.\nThe chip guesses what\nyou will do, and\nlearns how you think.",
                            "hold: end");
    is_on_tour = show_tour(argc, 0, 36);
    if (!is_leaving) {
        scr_clear();
        show_header("MIND READER");
        scr_text(7, 0, is_on_tour ? "tour presses 2 of 3" : "press or wait");
        show_points();
    }
    while (!is_leaving) {
        play_round();
    }

    scr_close();
    if (rounds > 1) {
        /* The round in which the button was held to end does not count */
        --rounds;
        put_str("In ");
        put_dec(rounds);
        put_str(" rounds the chip read the person ");
        put_dec(chip_points);
        put_str(" times: ");
        put_dec(chip_points * 100 / rounds);
        put_str(" of 100. A coin would be right 50 times of 100.\n");
    }

    return 0;
}
