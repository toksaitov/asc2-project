/*
    A cube that turns in space.

    In every picture the program turns the eight corners of the cube (a
    matrix times each of them), makes the far ones smaller, and draws the
    twelve edges. The button goes through four ways to do it:

    * the array of the chip turns the corners, or plain C code does;
    * the RP2040 draws the edges, or the program draws every pixel.

    The screen shows the pictures in a second and the time that the eight
    corners take. In every sixteenth picture the corners are turned in both
    ways, and the program counts the numbers that differ. The button held
    down for two seconds ends the program, and the console gets a line for
    every way that has run.
*/

#include "lib/machine.h"

#define LEAVE_US  2000000 /* the button held for this long ends the program */
#define REPORT_US 1000000 /* the numbers on the screen are those of this time */

#define CORNERS   8
#define SIDE      15 /* half of an edge */
#define CENTER_X  92
#define CENTER_Y  32
#define AREA_X    62 /* the columns from here on belong to the cube, the others to the text */
#define DISTANCE  64 /* from the eye to the middle of the cube; a point at the depth z looks DISTANCE / (DISTANCE - z) times as large */
#define DEPTHS    64

#define WAYS      4  /* bit 0: the program draws the pixels; bit 1: plain C turns the corners */

/* The functions of `lib/array.s` and `cube_asm.s` */

void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void turn_points(const unsigned int *points, int *sums, int count);
void line_pixels(volatile unsigned int *reg, unsigned int pixel, unsigned int last, int dx, int dy, int step_x, int step_y);

/* The sine of a quarter of a circle in 64 steps, in 64ths */
static const signed char sines[65] = {
    0, 2, 3, 5, 6, 8, 9, 11, 12, 14, 16, 17, 19, 20, 22, 23, 24, 26, 27, 29, 30, 32,
    33, 34, 36, 37, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
    56, 57, 58, 59, 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64, 64
};

/* A corner as a word: x, y, z, and 0, each a signed byte */
#define CORNER(x, y, z) (((unsigned int) (x) & 0xff) | ((unsigned int) (y) & 0xff) << 8 | ((unsigned int) (z) & 0xff) << 16)

static const unsigned int corners[CORNERS] = {
    CORNER(-SIDE, -SIDE, -SIDE), CORNER(SIDE, -SIDE, -SIDE), CORNER(SIDE, SIDE, -SIDE), CORNER(-SIDE, SIDE, -SIDE),
    CORNER(-SIDE, -SIDE, SIDE), CORNER(SIDE, -SIDE, SIDE), CORNER(SIDE, SIDE, SIDE), CORNER(-SIDE, SIDE, SIDE)
};

/* The edges as a walk from corner to corner; 8 more starts a new walk */
static const unsigned char walk[] = { 8 + 0, 1, 2, 3, 0, 4, 5, 6, 7, 4, 8 + 1, 5, 8 + 2, 6, 8 + 3, 7 };

#define WALK_STEPS ((int) sizeof(walk))

static int zoom[DEPTHS];            /* what a size at a depth is multiplied by, in 64ths */
static signed char matrix[3][4];
static int sums[3 * CORNERS];       /* the corners that have turned, in 64ths */
static int other_sums[3 * CORNERS];
static int corner_x[CORNERS];
static int corner_y[CORNERS];

static int way;
static unsigned int compared;       /* numbers that both ways have computed */
static unsigned int differ;         /* numbers that were not the same */

/* What a way has shown while the program ran */
static unsigned int total_pictures[WAYS];
static unsigned int total_us[WAYS];
static unsigned int total_turns[WAYS];
static unsigned int total_turn_us[WAYS];

static int sine(unsigned int angle)
{
    unsigned int part = angle & 63;
    int value = (angle & 64) ? sines[64 - part] : sines[part];

    return (angle & 128) ? -value : value;
}

/* The matrix of a turn around the x axis and then around the y axis, in 64ths */
static void set_matrix(unsigned int a, unsigned int b)
{
    int sa = sine(a);
    int ca = sine(a + 64);
    int sb = sine(b);
    int cb = sine(b + 64);

    matrix[0][0] = (signed char) cb;
    matrix[0][1] = (signed char) (sb * sa >> 6);
    matrix[0][2] = (signed char) (sb * ca >> 6);
    matrix[1][0] = 0;
    matrix[1][1] = (signed char) ca;
    matrix[1][2] = (signed char) -sa;
    matrix[2][0] = (signed char) -sb;
    matrix[2][1] = (signed char) (cb * sa >> 6);
    matrix[2][2] = (signed char) (cb * ca >> 6);
}

static void turn_in_array(int *out)
{
    unsigned int *rows = (unsigned int *) matrix;

    set_weights(rows[0], rows[1], rows[2]);
    turn_points(corners, out, CORNERS);
}

static void turn_in_c(int *out)
{
    const signed char *point = (const signed char *) corners;
    int i;

    for (i = 0; i < CORNERS; ++i) {
        out[0] = matrix[0][0] * point[0] + matrix[0][1] * point[1] + matrix[0][2] * point[2];
        out[1] = matrix[1][0] * point[0] + matrix[1][1] * point[1] + matrix[1][2] * point[2];
        out[2] = matrix[2][0] * point[0] + matrix[2][1] * point[1] + matrix[2][2] * point[2];
        out += 3;
        point += 4;
    }
}

static void compare_ways(void)
{
    int i;

    turn_in_array(sums);
    turn_in_c(other_sums);
    for (i = 0; i < 3 * CORNERS; ++i) {
        ++compared;
        if (sums[i] != other_sums[i]) {
            ++differ;
        }
    }
}

/* The places of the corners on the screen; the far ones move to the middle */
static void place_corners(void)
{
    int i;
    int *sum = sums;

    for (i = 0; i < CORNERS; ++i) {
        int size = zoom[(sum[2] >> 6) + DEPTHS / 2];
        corner_x[i] = CENTER_X + (sum[0] * size >> 12);
        corner_y[i] = CENTER_Y + (sum[1] * size >> 12);
        sum += 3;
    }
}

static void draw_edges_by_rp(void)
{
    int i;

    for (i = 0; i < WALK_STEPS; ++i) {
        int corner = walk[i] & 7;
        unsigned int point = SCR_POINT(corner_x[corner], corner_y[corner]);
        if (walk[i] & 8) {
            RP_SCR_MOVE = point;
        } else {
            RP_SCR_LINE = point;
        }
    }
}

static void draw_edges_by_pixels(void)
{
    int from = 0;
    int i;

    for (i = 0; i < WALK_STEPS; ++i) {
        int to = walk[i] & 7;
        if (!(walk[i] & 8)) {
            int dx = corner_x[to] - corner_x[from];
            int dy = corner_y[to] - corner_y[from];
            line_pixels(&RP_SCR_RECT, SCR_PIXEL(corner_x[from], corner_y[from]), SCR_PIXEL(corner_x[to], corner_y[to]),
                        dx > 0 ? dx : -dx, dy > 0 ? -dy : dy,
                        dx > 0 ? SCR_PIXEL_X : -SCR_PIXEL_X, dy > 0 ? SCR_PIXEL_Y : -SCR_PIXEL_Y);
        }
        from = to;
    }
}

static void show_way(void)
{
    scr_rect(0, 0, AREA_X, SCR_HEIGHT, SCR_DARK);
    scr_text(0, 0, (way & 2) ? "plain C" : "array");
    scr_text(1, 0, (way & 1) ? "own pixels" : "Pico lines");
    scr_text(3, 0, "pictures/s");
    scr_text(5, 0, "corners us");
}

static void show_numbers(unsigned int pictures, unsigned int us, unsigned int turns, unsigned int turn_us)
{
    scr_rect(0, 32, AREA_X, 8, SCR_DARK);
    scr_dec(4, 0, (pictures * 1000 + us / 2000) / (us / 1000));
    scr_rect(0, 48, AREA_X, 16, SCR_DARK);
    scr_dec(6, 0, turns != 0 ? turn_us / turns : 0);
    if (differ != 0) {
        scr_text(7, 0, "DIFFERENT");
    } else {
        scr_text(7, 0, "same as C");
    }
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

static void report(void)
{
    int i;

    for (i = 0; i < WAYS; ++i) {
        if (total_pictures[i] != 0 && total_turns[i] != 0) {
            put_str((i & 2) ? "plain C, " : "array, ");
            put_str((i & 1) ? "own pixels: " : "Pico lines: ");
            put_dec((total_pictures[i] * 1000 + total_us[i] / 2000) / (total_us[i] / 1000));
            put_str(" pictures a second, the 8 corners in ");
            put_dec(total_turn_us[i] / total_turns[i]);
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
        put_str("cube: another program has the screen\n");
        return 1;
    }
    put_str("cube: a press of the button changes the way; hold it for two seconds to end\n");

    for (i = 0; i < DEPTHS; ++i) {
        zoom[i] = DISTANCE * 64 / (DISTANCE - (i - DEPTHS / 2));
    }

    scr_clear();
    presses = RP_BTN_COUNT;
    while (!is_leaving) {
        unsigned int pictures = 0;
        unsigned int turns = 0;
        unsigned int turn_us = 0;
        unsigned int report_start = RP_TIME;
        unsigned int count = 0;

        show_way();
        while (!is_leaving && RP_BTN_COUNT == presses) {
            unsigned int start = RP_TIME;

            set_matrix(start >> 14, start >> 15);
            if ((count & 15) == 15) {
                compare_ways();
            } else {
                unsigned int turn_start = RP_TIME;
                if (way & 2) {
                    turn_in_c(sums);
                } else {
                    turn_in_array(sums);
                }
                turn_us += RP_TIME - turn_start;
                ++turns;
            }
            ++count;
            place_corners();

            scr_rect(AREA_X, 0, SCR_WIDTH - AREA_X, SCR_HEIGHT, SCR_DARK);
            if (way & 1) {
                draw_edges_by_pixels();
            } else {
                draw_edges_by_rp();
            }
            scr_show();

            ++pictures;
            start = RP_TIME;
            if (start - report_start >= REPORT_US) {
                total_pictures[way] += pictures;
                total_us[way] += start - report_start;
                total_turns[way] += turns;
                total_turn_us[way] += turn_us;
                show_numbers(pictures, start - report_start, turns, turn_us);
                pictures = 0;
                turns = 0;
                turn_us = 0;
                report_start = RP_TIME;
            }
            is_leaving = is_held(&since, &was_down);
        }
        presses = RP_BTN_COUNT;
        way = (way + 1) & (WAYS - 1);
    }

    scr_close();
    report();

    return differ == 0 ? 0 : 1;
}
