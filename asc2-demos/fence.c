/*
    Fence: three neurons learn to fence a field.

    A field has sheep and wolves. Each of three neurons owns a line on the
    screen: on one side of its line the neuron says "in", on the other
    "out". An animal is inside the fence if all three say "in". The lines
    start anywhere. The neurons look at one animal after another, and the
    lines move until every sheep is inside and every wolf outside.

    The three neurons are the three rows of the array of the chip. An
    animal is two numbers, where it stands, and a constant; `dotm` gives
    them to all three neurons at once, which is a layer of a network. A
    sheep that a neuron shuts out moves that neuron's line toward it; a
    wolf that all three let in moves the line that is nearest to it. One
    `learn` corrects all three rows, each by its own error.

    One time in four another line than the nearest takes the lesson of a
    wolf. Without that the lines can turn in a circle for ever: the nearest
    line is not always the one that can shut the wolf out. A model of the
    rule finished 300 fields of 300, half of them in 290 looks at an animal
    and the slowest in 2084 (`../asc2-kb/models/fence.py`).

    A press of the button makes a new field; the button held down for two
    seconds ends the program.
*/

#include "lib/machine.h"
#include "lib/show.h"

#define ANIMALS  24      /* half of them sheep */
#define STEPS    2       /* animals that the neurons look at between two pictures */
#define PATIENCE 8000    /* looks at an animal after which a field is given up */
#define LESSON   16      /* how far an animal moves a line: this many 128ths of its inputs */
#define REST_US  5000000 /* how long a finished field stays */

#define FIELD_Y  8       /* the field is the screen below the first row of text */
#define FIELD_H  56
#define MIDDLE_X 64      /* the place 0, 0 of the field on the screen */
#define MIDDLE_Y 36
#define ROOM     600     /* how far from an edge of the pasture an animal stands at the least, as the edge measures it */

/* The functions of `fence_asm.s` and `lib/array.s` */

void fence_look(unsigned int inputs, int *sums);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void get_weights(unsigned int *words);
void correct_weights(unsigned int errors);

/* The picture of the animals that the RP2040 copies to the screen for every frame: its size, then 56 rows of 4 words */
static unsigned int field[1 + 4 * FIELD_H];

static signed char animal_x[ANIMALS];  /* where an animal stands: from -60 to 60, */
static signed char animal_y[ANIMALS];  /* and from -24 to 24 */
static unsigned char is_sheep[ANIMALS];
static unsigned char is_wrong[ANIMALS]; /* the fence was wrong about the animal when the neurons looked at it last */

static unsigned int seed = 0x2545f491;
static unsigned int fields;
static unsigned int steps;

static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

/* A number from -range to range - 1; `range` is a power of two */
static int rnd_around(int range)
{
    return (int) (rnd() & (unsigned int) (2 * range - 1)) - range;
}

static void set_pixel(int x, int y)
{
    field[1 + 4 * (y - FIELD_Y) + (x >> 5)] |= 1u << (x & 31);
}

/* A sheep is a full square of 4 by 4 pixels, a wolf a hollow one */
static void draw_animal(int number)
{
    int left = MIDDLE_X + animal_x[number] - 2;
    int top = MIDDLE_Y + animal_y[number] - 2;
    int i;
    int j;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            if (is_sheep[number] || i == 0 || i == 3 || j == 0 || j == 3) {
                set_pixel(left + j, top + i);
            }
        }
    }
}

/*
    Makes a field: a pasture with three corners, sheep inside it and wolves
    outside, none of them near an edge. The pasture is one triangle, turned
    around and moved a little for every field. The work takes most of a
    second, so the button is looked at meanwhile; returns what it did.
*/
static int make_field(void)
{
    static const signed char corners[3][2] = { { -40, 30 }, { 40, 34 }, { 4, -44 } };
    int corner_x[3];
    int corner_y[3];
    int flip_x = (rnd() & 1) ? -1 : 1;
    int flip_y = (rnd() & 1) ? -1 : 1;
    int move_x = rnd_around(8);
    int move_y = rnd_around(4);
    int sheep = 0;
    int wolves = 0;
    int event = SHOW_NONE;
    int i;

    for (i = 0; i < 3; ++i) {
        corner_x[i] = flip_x * corners[i][0] + move_x;
        corner_y[i] = flip_y * corners[i][1] + move_y;
    }
    for (i = 0; i < 1 + 4 * FIELD_H; ++i) {
        field[i] = 0;
    }
    field[0] = SCR_WIDTH | FIELD_H << 16;

    while (sheep + wolves < ANIMALS) {
        int x = rnd_around(64);
        int y = rnd_around(32);
        int inside = 0;
        int near = 0;
        int seen = show_button();

        if (seen != SHOW_NONE) {
            event = seen;
        }

        /* The inputs of the neurons count the height twice, so that the field is as high for them as it is wide */
        if (x < -58 || x > 58 || y < -23 || y > 23) {
            continue;
        }
        for (i = 0; i < 3; ++i) {
            int next = i == 2 ? 0 : i + 1;
            int side = (corner_x[next] - corner_x[i]) * (2 * y - corner_y[i]) -
                       (corner_y[next] - corner_y[i]) * (x - corner_x[i]);
            /* The corners go around the pasture one way, and the other way after one flip */
            if (flip_x * flip_y > 0) {
                side = -side;
            }
            if (side > 0) {
                ++inside;
            }
            if (side > -ROOM && side < ROOM) {
                near = 1;
            }
        }
        if (near || (inside == 3 ? sheep : wolves) == ANIMALS / 2) {
            continue;
        }
        i = sheep + wolves;
        animal_x[i] = (signed char) x;
        animal_y[i] = (signed char) y;
        is_sheep[i] = (unsigned char) (inside == 3);
        is_wrong[i] = 1;
        draw_animal(i);
        if (inside == 3) {
            ++sheep;
        } else {
            ++wolves;
        }
    }

    /* The lines start anywhere */
    set_weights(rnd() & 0x1f1f1f, rnd() & 0x1f1f1f, rnd() & 0x1f1f1f);
    steps = 0;
    ++fields;

    return event;
}

/* The neurons look at an animal and learn from it; returns 1 if the fence was wrong about it */
static int look_at(int number, int with_lesson)
{
    unsigned int inputs = ((unsigned int) animal_x[number] & 0xff) | ((unsigned int) (2 * animal_y[number]) & 0xff) << 8 | 64u << 16;
    unsigned int errors = 0;
    int sums[3];
    int i;

    fence_look(inputs, sums);
    if (is_sheep[number]) {
        /* Every neuron that shuts a sheep out moves toward it */
        for (i = 0; i < 3; ++i) {
            if (sums[i] <= 0) {
                errors |= (unsigned int) LESSON << (8 * i);
            }
        }
    } else if (sums[0] > 0 && sums[1] > 0 && sums[2] > 0) {
        /* A wolf is inside: the neuron whose line is nearest to it moves to shut it out */
        int nearest = sums[1] < sums[0] ? 1 : 0;
        unsigned int chance = rnd() & 7;
        if (sums[2] < sums[nearest]) {
            nearest = 2;
        }
        if (chance < 2) {
            /* One time in four another line takes the lesson */
            nearest += 1 + (int) chance;
            if (nearest > 2) {
                nearest -= 3;
            }
        }
        errors = ((unsigned int) -LESSON & 0xff) << (8 * nearest);
    }
    if (errors != 0 && with_lesson) {
        correct_weights(errors);
    }

    return errors != 0;
}

/*
    Where a line and its three strokes are on the screen, as points for the
    RP2040: the two ends of the line, then the two ends of each stroke. A
    stroke leads from the line to its inner side, where the sum of the
    neuron is above zero. The points are computed anew only when the
    weights of the line change: the CPU has no instruction that divides.
*/
#define LINE_POINTS 8

static unsigned int line_points[3][LINE_POINTS];
static unsigned int line_weights[3]; /* the weights that the points were computed from */
static int shown_wrong = -1;         /* what the first row of the screen says: the number of wrong animals, or 0 for "all are right" */

static int limit(int value)
{
    return value > 400 ? 400 : value < -400 ? -400 : value;
}

#define FIELD_TOP (FIELD_Y - MIDDLE_Y) /* the top row of the field, counted from its middle */
#define NOWHERE   SCR_POINT(-100, -100) /* a point outside the screen */

/*
    Computes the points of the line of a neuron: the places where its sum is
    zero. Nothing of it may lie above the field: the first row of text is
    there, and it is not drawn anew with every picture.
*/
static void place_line(int number, const signed char *weights)
{
    static const short level_parts[3] = { 55, 128, 201 };   /* the columns -40, 0, and 40 on the way from -70 to 70, in parts of 256 */
    static const short upright_parts[3] = { 93, 261, 428 }; /* the rows -18, 0, and 18 on the way from the top row to the bottom one, in parts of 512 */
    unsigned int *points = line_points[number];
    int a = weights[0];
    int b = 2 * weights[1];
    int c = 64 * weights[2];
    int size_a = a < 0 ? -a : a;
    int size_b = b < 0 ? -b : b;
    int stroke_x;
    int stroke_y;
    int from;
    int to;
    int i;

    for (i = 0; i < LINE_POINTS; ++i) {
        points[i] = NOWHERE;
    }
    if (a == 0 && b == 0) {
        return;
    }

    if (size_b >= size_a) {
        /* Nearer to level than to upright: from the left edge to the right one, and the strokes go up or down */
        int left = -70;
        int right = 70;

        from = limit(-(c - 70 * a) / b);
        to = limit(-(c + 70 * a) / b);
        stroke_x = 4 * a / size_b;
        stroke_y = b > 0 ? 4 : -4;
        for (i = 0; i < 3; ++i) {
            int x = 40 * (i - 1);
            int y = from + (((to - from) * level_parts[i]) >> 8);
            if (y >= FIELD_TOP && y + stroke_y >= FIELD_TOP) {
                points[2 + 2 * i] = SCR_POINT(MIDDLE_X + x, MIDDLE_Y + y);
                points[3 + 2 * i] = SCR_POINT(MIDDLE_X + x + stroke_x, MIDDLE_Y + y + stroke_y);
            }
        }

        /* The part of the line above the field is cut off */
        if (from < FIELD_TOP && to < FIELD_TOP) {
            return;
        }
        if (from < FIELD_TOP) {
            left = -70 + 140 * (FIELD_TOP - from) / (to - from);
            from = FIELD_TOP;
        } else if (to < FIELD_TOP) {
            right = 70 - 140 * (FIELD_TOP - to) / (from - to);
            to = FIELD_TOP;
        }
        points[0] = SCR_POINT(MIDDLE_X + left, MIDDLE_Y + from);
        points[1] = SCR_POINT(MIDDLE_X + right, MIDDLE_Y + to);
    } else {
        /* From the top row of the field to the bottom one, and the strokes go left or right */
        from = limit(-(c + FIELD_TOP * b) / a);
        to = limit(-(c + (FIELD_TOP + FIELD_H - 1) * b) / a);
        stroke_x = a > 0 ? 4 : -4;
        stroke_y = 4 * b / size_a;
        points[0] = SCR_POINT(MIDDLE_X + from, FIELD_Y);
        points[1] = SCR_POINT(MIDDLE_X + to, FIELD_Y + FIELD_H - 1);
        for (i = 0; i < 3; ++i) {
            int y = 18 * (i - 1);
            int x = from + (((to - from) * upright_parts[i]) >> 9);
            points[2 + 2 * i] = SCR_POINT(MIDDLE_X + x, MIDDLE_Y + y);
            points[3 + 2 * i] = SCR_POINT(MIDDLE_X + x + stroke_x, MIDDLE_Y + y + stroke_y);
        }
    }
}

static void show_field(int wrong, int is_done)
{
    unsigned int weights[3];
    int i;
    int j;

    get_weights(weights);
    scr_blit(field, 0, FIELD_Y, SCR_COPY);
    for (i = 0; i < 3; ++i) {
        if (weights[i] != line_weights[i] || shown_wrong < 0) {
            line_weights[i] = weights[i];
            place_line(i, (const signed char *) &weights[i]);
        }
        for (j = 0; j < LINE_POINTS; j += 2) {
            RP_SCR_MOVE = line_points[i][j];
            RP_SCR_LINE = line_points[i][j + 1];
        }
    }

    /* The first row changes only when the number of wrong animals does */
    if (is_done) {
        wrong = 0;
    }
    if (wrong != shown_wrong) {
        shown_wrong = wrong;
        scr_rect(0, 0, SCR_WIDTH, 8, SCR_DARK);
        if (is_done) {
            scr_text(0, 1, "FENCE  all are right");
        } else {
            scr_text(0, 1, "FENCE  wrong:");
            scr_dec(0, 15, (unsigned int) wrong);
        }
        scr_rect(0, 0, SCR_WIDTH, 8, SCR_FLIP);
    }
    scr_show();
}

int main(int argc, char *argv[])
{
    int event = SHOW_NONE;
    int i;

    if (scr_open() != 0) {
        put_str("fence: another program has the screen\n");
        return 1;
    }
    put_str("fence: a press of the button makes a new field; hold it for two seconds to end\n");

    if (show_title("FENCE", "Three neurons, each\nwith a line, learn to\nkeep the sheep (full)\ninside and the wolves\n(hollow) outside.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }
    show_tour(argc, 0, 32);
    seed ^= RP_TIME;
    while (event != SHOW_LEAVE) {
        int wrong = ANIMALS;
        int is_done = 0;

        event = make_field();
        shown_wrong = -1;
        while (event == SHOW_NONE && !is_done && steps < PATIENCE) {
            for (i = 0; i < STEPS; ++i) {
                unsigned int number = rnd() & 31;
                if (number < ANIMALS) {
                    int was_wrong = is_wrong[number];
                    is_wrong[number] = (unsigned char) look_at((int) number, 1);
                    wrong += is_wrong[number] - was_wrong;
                    ++steps;
                }
            }
            if (wrong == 0) {
                /* Maybe all are right: look at every animal once, with no lesson */
                for (i = 0; i < ANIMALS; ++i) {
                    is_wrong[i] = (unsigned char) look_at(i, 0);
                    wrong += is_wrong[i];
                }
                is_done = wrong == 0;
            }
            show_field(wrong, is_done);
            event = show_button();
        }
        if (is_done) {
            put_str("Field ");
            put_dec(fields);
            put_str(": every sheep is inside the three lines and every wolf outside, after ");
            put_dec(steps);
            put_str(" looks at an animal.\n");
            event = show_wait(REST_US);
        } else if (event == SHOW_NONE) {
            put_str("Field ");
            put_dec(fields);
            put_str(": the neurons did not finish it; they get a new field.\n");
        }
    }

    scr_close();

    return 0;
}
