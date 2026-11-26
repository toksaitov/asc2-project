/*
    Burkut, a game for one button.

    A burkut is a golden eagle. It flies to the right between pillars of
    rock; a press of the button is a beat of its wings, and without one it
    sinks. Every pillar that it passes is a point, and a touch of a pillar
    or of the ground ends the flight. The console prints the points. The
    button held down for two seconds ends the game.

    The machine runs about thirty thousand instructions in a second on the
    bench, so a frame must be cheap (it takes 12 ms there). The program draws nothing pixel by
    pixel: the RP2040 draws every pillar as two rectangles and the burkut as
    a picture, each for one write to a register, and the program only moves
    numbers. The length of a frame comes from the clock of the RP2040, so
    the game is as fast on a quicker machine.

    Behind the pillars is a scene: the sun, two clouds, and mountains. The
    clouds and the mountains pass more slowly than the pillars, so they
    seem far away.
*/

#include "lib/machine.h"
#include "pictures.h"

#ifdef PILOT
#include "lib/show.h"
#endif

#define FRAME_US    33333 /* 30 frames in a second */
#define LEAVE_US    2000000 /* the button held for this long ends the game */

#define BURKUT_X     20
#define BURKUT_W     12
#define BURKUT_H     8
/* The picture of the burkut is larger than the burkut of the rules: its wings reach above and below, its tail and its beak to the sides */
#define PICTURE_DX  2
#define PICTURE_DY  3
#define GROUND_Y    62    /* the first row of the ground */

#define SUN_X       96    /* the left top corner of the sun, below the first row of text */
#define SUN_Y       10
#define RANGE_H     24    /* the height of the picture of the mountains */

#define PILLARS     3
#define PILLAR_W    10
#define PILLAR_STEP 48    /* from the left edge of a pillar to that of the next */
#define GAP         24    /* height of the hole in a pillar */
#define SPEED       21    /* sixteenths of a pixel that the pillars move in a frame: 40 pixels in a second */

/* The height of the burkut is counted in sixteenths of a pixel too */
#define GRAVITY     2     /* what the fall gains in a frame */
#define FLAP        (-20) /* the speed after a beat of the wings */
#define MAX_FALL    27

static int pillar_x[PILLARS];   /* left edge in sixteenths of a pixel; it may lie left of the screen */
static int pillar_gap[PILLARS]; /* first row of the hole */
static int pillar_passed[PILLARS];

static unsigned int seed = 0x2545f491;
static unsigned int score;
static unsigned int best;
static char score_text[12];
static unsigned int frames;   /* frames of the last flight */
static unsigned int work_us;  /* time that the machine has worked on them, without the wait for the end of a frame */

/* A random number; the moment of the first press makes each game its own */
static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

/* A hole that leaves 4 rows of rock above and below */
static int new_gap(void)
{
    unsigned int r = rnd();

    return 4 + (int) (r & 15) + (int) ((r >> 4) & 15);
}

/* The points as text; a division is slow on this machine, so only when they change */
static void set_score(unsigned int value)
{
    char digits[10];
    int count = 0;
    int i = 0;

    score = value;
    do {
        digits[count++] = (char) ('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count > 0) {
        score_text[i++] = digits[--count];
    }
    score_text[i] = '\0';
}

static int is_leaving;          /* the player has held the button down: the game ends */
static int was_held;            /* the button was down at the last look */
static unsigned int held_since;

/* Looks at the button; every loop that waits calls it, so no release is missed */
static void watch_button(void)
{
#ifdef PILOT
    if (show_tour_over()) {
        is_leaving = 1;
    }
#endif
    if (RP_BTN_STAT == 0) {
        was_held = 0;
    } else if (!was_held) {
        was_held = 1;
        held_since = RP_TIME;
    } else if (RP_TIME - held_since > LEAVE_US) {
        is_leaving = 1;
    }
}

static void wait_until(unsigned int start, unsigned int us)
{
    while (RP_TIME - start < us) {
        watch_button();
    }
}

static void wait_for_press(void)
{
    unsigned int presses = RP_BTN_COUNT;

    while (RP_BTN_COUNT == presses && !is_leaving) {
        watch_button();
    }
}

static void draw_cloud(int x, int y)
{
    scr_rect(x + 4, y, 10, 2, SCR_LIGHT);
    scr_rect(x, y + 2, 20, 3, SCR_LIGHT);
}

/*
    The scene behind the pillars, a few writes to the RP2040: the sun, two
    clouds that pass a pixel in two frames, and mountains that pass a pixel
    in four. A cloud comes back after 256 pixels, the mountains repeat
    after 128.
*/
static void draw_scene(void)
{
    int range = (int) ((frames >> 2) & 127);
    int cloud = 127 - (int) ((frames >> 1) & 255);

    scr_rect(SUN_X + 2, SUN_Y, 5, 9, SCR_LIGHT);
    scr_rect(SUN_X, SUN_Y + 2, 9, 5, SCR_LIGHT);
    scr_rect(SUN_X + 1, SUN_Y + 1, 7, 7, SCR_LIGHT);
    draw_cloud(cloud, 12);
    draw_cloud(cloud < 0 ? cloud + 128 : cloud - 128, 22);
    scr_blit(mountains, -range, GROUND_Y - RANGE_H, SCR_LIGHT);
    scr_blit(mountains, 128 - range, GROUND_Y - RANGE_H, SCR_LIGHT);
}

/* The burkut with a dark edge around it, so that neither the scene nor a pillar runs into its picture */
static void draw_burkut(unsigned int *picture, int y)
{
    int left = BURKUT_X - PICTURE_DX;
    int top = y - PICTURE_DY;

    scr_blit(picture, left - 1, top, SCR_DARK);
    scr_blit(picture, left + 1, top, SCR_DARK);
    scr_blit(picture, left, top - 1, SCR_DARK);
    scr_blit(picture, left, top + 1, SCR_DARK);
    scr_blit(picture, left, top, SCR_LIGHT);
}

static void draw_pillars(void)
{
    int i;

    for (i = 0; i < PILLARS; ++i) {
        int x = pillar_x[i] >> 4;
        int bottom = pillar_gap[i] + GAP;
        if (x < SCR_WIDTH) {
            scr_rect(x, 0, PILLAR_W, pillar_gap[i], SCR_LIGHT);
            scr_rect(x, bottom, PILLAR_W, GROUND_Y - bottom, SCR_LIGHT);
        }
    }
    scr_rect(0, GROUND_Y, SCR_WIDTH, SCR_HEIGHT - GROUND_Y, SCR_LIGHT);
}

#ifndef PILOT

/* The first screen: the burkut, the name, and a line that blinks until a press */
static void show_title(void)
{
    unsigned int presses = RP_BTN_COUNT;
    unsigned int start;

    scr_clear();
    scr_blit(burkut_big, 48, 2, SCR_LIGHT);
    scr_text(4, 7, "BURKUT");
    scr_text(6, 2, "press the button");
    for (;;) {
        scr_show();
        start = RP_TIME;
        while (RP_TIME - start < 400000) {
            watch_button();
            if (RP_BTN_COUNT != presses) {
                seed ^= RP_TIME;
                return;
            }
        }
        scr_rect(0, 47, SCR_WIDTH, 10, SCR_FLIP);
    }
}

#else

/* The neuron that flies the burkut; `pilot.c` tells how it learns */

#define MEMORY 16 /* frames before a crash that the neuron learns from */
#define LESSON 4  /* how far a wrong frame moves a weight: this many 128ths of its input */
#define WATCH  3  /* a flight of the neuron runs fast until it has passed this many pillars: one frame in four is drawn, and no frame waits */

/* The functions of `pilot_asm.s` */

int pilot_guess(unsigned int inputs);
void pilot_learn(unsigned int inputs, unsigned int error);

static unsigned int seen[MEMORY];    /* what the neuron saw in the last frames, at the number of the frame modulo MEMORY */
static unsigned char chosen[MEMORY]; /* 1 if it beat the wings there */
static int is_person;                /* a person has taken the flight over */
static int is_low;                   /* the flight ended below a hole or on the ground */
static unsigned int flights;
static unsigned int lessons;         /* crashes that the neuron has learned from */

static int hold(int value)
{
    return value > 127 ? 127 : value < -128 ? -128 : value;
}

/* What the neuron sees: four signed bytes in a word */
static unsigned int look(int height, int speed)
{
    int nearest = 0x7fffffff;
    int gap = 0;
    int above;
    int i;

    /* The next pillar is the first one that the burkut has not left behind */
    for (i = 0; i < PILLARS; ++i) {
        int x = pillar_x[i] >> 4;
        if (x + PILLAR_W >= BURKUT_X && x < nearest) {
            nearest = x;
            gap = pillar_gap[i];
        }
    }
    above = (height >> 4) + BURKUT_H / 2 - (gap + GAP / 2);

    return ((unsigned int) hold(2 * above) & 0xff) | ((unsigned int) hold(4 * speed) & 0xff) << 8 | 64u << 16 |
           ((unsigned int) hold(nearest - BURKUT_X) & 0xff) << 24;
}

/* The lesson of a crash: the frames before it in which the choice was wrong move the weights */
static void learn_from_crash(unsigned int count)
{
    unsigned int first = count < MEMORY ? 0 : count - MEMORY;
    unsigned int i;

    for (i = first; i < count; ++i) {
        unsigned int place = i & (MEMORY - 1);
        if (is_low && !chosen[place]) {
            pilot_learn(seen[place], LESSON);
        } else if (!is_low && chosen[place]) {
            pilot_learn(seen[place], (unsigned int) -LESSON & 0xff);
        }
    }
    ++lessons;
}

/* The first row of the screen: the number of the flight, who flies, and the points */
static void show_flight(void)
{
    int col;

    scr_rect(0, 0, SCR_WIDTH, 8, SCR_DARK);
    col = scr_dec(0, 7, flights);
    scr_text(0, 0, "flight");
    if (is_person) {
        scr_text(0, col + 1, "you");
    } else if (score < WATCH) {
        scr_text(0, col + 1, ">>");
    }
}

#endif

/* One flight; returns when the burkut has touched something */
static void play(void)
{
    unsigned int presses = RP_BTN_COUNT;
    int height = 24 << 4;
    int speed = 0;
    int is_down = 0;
    int i;

    for (i = 0; i < PILLARS; ++i) {
        pillar_x[i] = (SCR_WIDTH + i * PILLAR_STEP) << 4;
        pillar_gap[i] = new_gap();
        pillar_passed[i] = 0;
    }
    set_score(0);
    frames = 0;
    work_us = 0;

    while (!is_down && !is_leaving) {
        unsigned int start = RP_TIME;
        unsigned int now = RP_BTN_COUNT;
        int y;

        /* The count of presses misses none, however long a frame takes */
        if (now != presses) {
            presses = now;
            speed = FLAP;
#ifdef PILOT
            is_person = 1;
#endif
        }
#ifdef PILOT
        if (!is_person) {
            unsigned int inputs = look(height, speed);
            int beats = pilot_guess(inputs) > 0;
            seen[frames & (MEMORY - 1)] = inputs;
            chosen[frames & (MEMORY - 1)] = (unsigned char) beats;
            if (beats) {
                speed = FLAP;
            }
        }
#endif
        speed += GRAVITY;
        if (speed > MAX_FALL) {
            speed = MAX_FALL;
        }
        height += speed;
        if (height < 0) {
            height = 0;
            speed = 0;
        }
        y = height >> 4;
        if (y + BURKUT_H > GROUND_Y) {
            is_down = 1;
#ifdef PILOT
            is_low = 1;
#endif
        }

        for (i = 0; i < PILLARS; ++i) {
            int x = (pillar_x[i] -= SPEED) >> 4;
            if (x < -PILLAR_W) {
                pillar_x[i] += (PILLARS * PILLAR_STEP) << 4;
                x += PILLARS * PILLAR_STEP;
                pillar_gap[i] = new_gap();
                pillar_passed[i] = 0;
            }

            if (!pillar_passed[i] && x + PILLAR_W < BURKUT_X) {
                pillar_passed[i] = 1;
                set_score(score + 1);
#ifndef PILOT
                put_str("score ");
                put_str(score_text);
                put_char('\n');
#endif
            }
            /* The burkut is judged by its body, a box of 10 by 6 pixels in the middle of its picture; the wings do not count */
            if (x < BURKUT_X + BURKUT_W - 1 && x + PILLAR_W > BURKUT_X + 1 &&
                (y + 1 < pillar_gap[i] || y + BURKUT_H - 1 > pillar_gap[i] + GAP)) {
                is_down = 1;
#ifdef PILOT
                is_low = y + 1 >= pillar_gap[i];
#endif
            }
        }

#ifdef PILOT
        if (is_person || score >= WATCH || (frames & 3) == 0) {
#endif
        scr_clear();
#ifdef PILOT
        /* The flights that run fast, for the neuron to learn, have no scene: they are to be over soon */
        if (is_person || score >= WATCH) {
            draw_scene();
        }
#else
        draw_scene();
#endif
        draw_pillars();
        draw_burkut(speed < 0 ? burkut_down : burkut_up, y);
#ifdef PILOT
        show_flight();
#endif
        scr_text(0, 18, score_text);
        scr_show();
#ifdef PILOT
        }
#endif

        ++frames;
        work_us += RP_TIME - start;
#ifdef PILOT
        if (is_person || score >= WATCH) {
            wait_until(start, FRAME_US);
        } else {
            watch_button();
        }
#else
        wait_until(start, FRAME_US);
#endif
    }
#ifdef PILOT
    if (is_down && !is_person) {
        learn_from_crash(frames);
    }
#endif
}

#ifndef PILOT

/* The end of a flight: the picture blinks, then the points and the best flight so far */
static void show_end(void)
{
    unsigned int start;
    int i;
    int col;

    for (i = 0; i < 4; ++i) {
        start = RP_TIME;
        scr_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, SCR_FLIP);
        scr_show();
        wait_until(start, 80000);
    }

    if (score > best) {
        best = score;
    }
    put_str("the flight is over: score ");
    put_dec(score);
    put_str(", best ");
    put_dec(best);
#ifndef RACH2
    /* Under the kernel the line stays the same on every machine, so that a test can compare it */
    put_str("; a frame took ");
    put_dec(work_us / frames / 1000);
    put_str(" ms of ");
    put_dec(FRAME_US / 1000);
#endif
    put_char('\n');

    scr_rect(10, 14, 108, 36, SCR_LIGHT);
    scr_rect(12, 16, 104, 32, SCR_DARK);
    scr_text(2, 6, "GAME OVER");
    col = scr_dec(4, 9, score);
    scr_text(4, 3, "score");
    col = scr_dec(5, 9, best);
    scr_text(5, 3, "best");
    scr_text(3, 4, "hold: the end");
    scr_show();

    /* A press that was meant for the flight must not start the next one */
    start = RP_TIME;
    wait_until(start, 700000);
    wait_for_press();
}

#else

int main(int argc, char *argv[])
{
    unsigned int start;
    int i;

    if (scr_open() != 0) {
        put_str("pilot: another program has the screen\n");
        return 1;
    }
    put_str("pilot: the burkut flies by itself; a press lets you fly, and a hold of two seconds ends the program\n");

    is_leaving = show_title("PILOT", "The burkut flies by\nitself. A neuron in\nthe chip starts with\nnothing and learns\nfrom every crash.",
                            "press:fly   hold:end");
    show_tour(argc, 0, 45); /* time to learn, and a few flights after that */
    seed ^= RP_TIME;
    while (!is_leaving) {
        ++flights;
        is_person = 0;
        is_low = 0;
        play();
        /* A press that is still held may be the one that ends the program */
        while (RP_BTN_STAT != 0 && !is_leaving) {
            watch_button();
        }
        if (is_leaving) {
            break;
        }
        if (is_person || score > best) {
            put_str("flight ");
            put_dec(flights);
            put_str(": ");
            put_dec(score);
            put_str(score == 1 ? " pillar" : " pillars");
            put_str(is_person ? ", the end of it flown by a person\n" : ", the best of the neuron so far\n");
        }
        if (!is_person && score > best) {
            best = score;
        }
        /* The end of a flight that was worth watching blinks */
        for (i = 0; i < 2 && (is_person || score >= WATCH); ++i) {
            start = RP_TIME;
            scr_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, SCR_FLIP);
            scr_show();
            wait_until(start, 60000);
        }
    }

    scr_close();
    put_str("The burkut flew ");
    put_dec(flights);
    put_str(flights == 1 ? " time, and the neuron learned from " : " times, and the neuron learned from ");
    put_dec(lessons);
    put_str(lessons == 1 ? " crash. Its best flight passed " : " crashes. Its best flight passed ");
    put_dec(best);
    put_str(best == 1 ? " pillar.\n" : " pillars.\n");

    return 0;
}

#endif

#ifndef PILOT

int main(void)
{
    if (scr_open() != 0) {
        put_str("burkut: another program has the screen\n");
        return 1;
    }
    put_str("Burkut: a press of the button is a beat of the wings; hold it for two seconds to end the game\n");

    show_title();
    while (!is_leaving) {
        put_str("the flight begins\n");
        play();
        if (!is_leaving) {
            show_end();
        }
    }

    scr_close();
    put_str("the game is over: best ");
    put_dec(best);
    put_char('\n');

    return 0;
}

#endif
