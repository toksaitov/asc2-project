/*
    Film: a day in the mountains, about a minute long.

    Six scenes, each as long as the clock says, so the film is the same
    film on a faster or a slower machine; only its pictures come more or
    less often:

    * dawn: the stars go out, the sky lights up, the sun rises behind the
      far range;
    * the burkut comes from far away, straight at the eye;
    * the burkut flies along the range: the far mountains, the near hills,
      the ground, and the clouds pass at their own speeds;
    * home: a yurt in the evening, smoke over it, the sun sets;
    * night: the stars come out, the moon rises, the door of the yurt is lit;
    * the name of the machine.

    What is a picture in memory and what is computed: the two ranges, the
    cloud, and the name are pictures, which the RP2040 draws where the
    program says. The burkut and the yurt are models in space: for every
    picture the chip turns each of their points with one instruction
    (`dotm`), finds how much light falls on each side, and fills the sides
    row by row, as `shapes.c` does. The sun, the moon, the stars, and the
    smoke are rectangles and lines that the program places.

    A press of the button goes to the next scene. The button held down for
    two seconds ends the program.
*/

#include "lib/machine.h"
#include "lib/show.h"

#include "shapes_models.h"
#include "film_pictures.h"

#define MAX_POINTS 21
#define MAX_SIDES  17

/* Two of the styles of a side; see `fill_sides` of `shapes_asm.s` */
#define STYLE_BRIGHT 6 /* every row lit */
#define STYLE_DOOR   7 /* always dark, with its edges drawn */

/* The words of the job of `fill_sides` */
#define JOB_SIDES    0
#define JOB_PLACES   1
#define JOB_STYLES   2
#define JOB_ORDER    3
#define JOB_COUNT    4
#define JOB_COVERS   5
#define JOB_PARTS    6
#define JOB_ROWS     7
#define JOB_RECT     8
#define JOB_MOVE     9
#define JOB_LIT_ROWS 10
#define JOB_WORDS    11

/* The functions of `shapes_asm.s`, `film_asm.s`, and `lib/array.s` */

void set_turn(unsigned int around, unsigned int tilt, int size, const signed char *sines, unsigned int light, unsigned int *seen);
void turn_points(const unsigned int *points, int *sums, int count);
void place_points(const int *sums, unsigned int *places, int count, const short *zoom, unsigned int middle);
void style_sides(const unsigned int *sides, unsigned int *styles, int count);
void sort_sides(const unsigned int *sides, const int *sums, int *depths, unsigned char *order, int count);
void fill_sides(const unsigned int *job);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void draw_marks_at(volatile unsigned int *rect, const unsigned int *marks, int count, unsigned int offset);
void draw_marks_wrap(volatile unsigned int *rect, const unsigned int *marks, int count, unsigned int shift);
void draw_lines_at(volatile unsigned int *move, const unsigned int *points, int count, unsigned int offset);

/*
    Where the light comes from, as the eye sees it, in 64ths: by day from
    the left (-29), from above (42), and from the front (39); by night from
    the moon at the left, half as strong, so that no side is bright.
*/
#define LIGHT 0x272ae3u
#define NIGHT_LIGHT 0x1315f2u

/* The sine of an angle in 64ths; a circle is 256 */
#define sine(angle) ((int) sines[(angle) & 255])

/* A place on the screen for `place_points`, and the offset of marks for `draw_marks_at` */
#define MIDDLE(x, y) ((unsigned int) (x) | (unsigned int) (y) << 16)
#define AT(x, y)     ((unsigned int) (x) | (unsigned int) (y) << 7)

/* The scenes and how long each one lasts, in microseconds */
#define DAWN    0
#define ARRIVAL 1
#define FLIGHT  2
#define HOME    3
#define NIGHT   4
#define NAME    5
#define SCENES  6

#define SECOND 1000000u

static const unsigned int lengths[SCENES] = {
    10 * SECOND, 8 * SECOND, 14 * SECOND, 10 * SECOND, 9 * SECOND, 8 * SECOND
};

/* Where the sun stands by day and where it sets, and where the moon rises: the column of the left side of the box */
#define SUN_X     77
#define SUNSET_X  97
#define MOON_X    9

/* How far the land is moved to the right behind the yurt: a peak stands behind it, the sun sets in a valley to its right, and the moon rises in one to its left */
#define HOME_SHIFT 20
#define HIDDEN_Y  40 /* the row of the top of the sun or of the moon when the far range hides it wholly */

/* How the burkut is seen and how far the land is moved down while it flies: the eye looks up into the sky */
#define SKY_DOWN    20
#define BURKUT_SIZE  76
#define TOWARDS     192 /* the angle around the burkut from which it comes straight at the eye */
#define ALONG       32  /* the one from which it flies to the right and away */

static unsigned int seen[2];            /* the light and the eye as a model sees them; see `set_turn` */
static int sums[3 * MAX_POINTS];        /* the points that have turned, in 64ths; y grows downwards */
static unsigned int places[MAX_POINTS]; /* their places on the screen: the column, and the row in the upper half */
static unsigned int styles[MAX_SIDES];  /* how every side is drawn in this picture */
static int depths[MAX_SIDES];
static unsigned char order[MAX_SIDES];  /* the numbers of the sides in the order of drawing */
static unsigned short rows[SCR_HEIGHT]; /* for `fill_sides`: the two ends of a side in every row of the screen, a byte each */
static unsigned int job[JOB_WORDS];

/* Gets ready to draw a model: the job of `fill_sides`, and the sides in the order in which the model lists them. */
static void take_model(unsigned int *sides, int side_count)
{
    int i;

    job[JOB_SIDES] = (unsigned int) sides;
    job[JOB_PLACES] = (unsigned int) places;
    job[JOB_STYLES] = (unsigned int) styles;
    job[JOB_ORDER] = (unsigned int) order;
    job[JOB_COUNT] = (unsigned int) side_count;
    job[JOB_COVERS] = 1; /* a side covers the land behind it */
    job[JOB_PARTS] = (unsigned int) parts;
    job[JOB_ROWS] = (unsigned int) rows;
    job[JOB_RECT] = (unsigned int) &RP_SCR_RECT;
    job[JOB_MOVE] = (unsigned int) &RP_SCR_MOVE;
    job[JOB_LIT_ROWS] = (unsigned int) lit_rows;
    for (i = 0; i < MAX_SIDES; ++i) {
        order[i] = (unsigned char) i;
    }
}

/* Beats the wings of the burkut, as `shapes.c` does: a beat is about a second. */
static void beat_wings(unsigned int time)
{
    const unsigned int *beat = wing_beats + BEAT_WORDS * ((time >> 15) & (BEATS - 1));
    unsigned int *point = burkut_points + BURKUT_WINGS + 2;
    unsigned int *part = burkut_sides + SIDE_WORDS * BURKUT_WING_SIDES;

    point[0] = beat[0];
    point[1] = beat[1];
    point[2] = beat[2];
    point[5] = beat[3];
    point[6] = beat[4];
    point[7] = beat[5];
    part[0] = beat[6];
    part[SIDE_WORDS] = beat[7];
    part[2 * SIDE_WORDS] = beat[8];
    part[3 * SIDE_WORDS] = beat[9];
}

/*
    Draws the burkut over what the picture has so far. `around` and `tilt`
    are the angles from which the eye sees it, `size` is in 64ths, and
    `middle` is its place on the screen. Its row must be from 28 to 36, and
    the caller keeps the burkut between the sides of the screen.
*/
static void draw_burkut(unsigned int time, unsigned int around, unsigned int tilt, int size, unsigned int middle)
{
    beat_wings(time);
    set_turn(around, tilt, size, sines, LIGHT, seen);
    turn_points(burkut_points, sums, BURKUT_POINTS);
    place_points(sums, places, BURKUT_POINTS, zoom, middle);
    set_weights(seen[0], seen[1], 0);
    style_sides(burkut_sides, styles, BURKUT_SIDES);
    sort_sides(burkut_sides, sums, depths, order, BURKUT_SIDES);
    fill_sides(job);
}

/* Draws the yurt in the same way; `door` is the style of its door, and `light` is where the light comes from. */
static void draw_yurt(unsigned int around, unsigned int tilt, int size, unsigned int middle, unsigned int door, unsigned int light)
{
    set_turn(around, tilt, size, sines, light, seen);
    turn_points(yurt_points, sums, YURT_POINTS);
    place_points(sums, places, YURT_POINTS, zoom, middle);
    set_weights(seen[0], seen[1], 0);
    style_sides(yurt_sides, styles, YURT_SIDES);
    if (styles[YURT_DOOR] != 0) {
        styles[YURT_DOOR] = door;
    }
    fill_sides(job);
}

/*
    Draws the far range and the near hills, each moved to the right by a
    number of columns, and both moved down by `down` rows: the eye looks up
    into the sky. Both repeat after 128 columns, so each is drawn twice.
    With `hides` the far range covers what is behind it: the sun, the moon,
    the lit rows of the dawn.
*/
static void draw_land(unsigned int far_shift, unsigned int mid_shift, int down, int hides)
{
    int far_x = (int) (far_shift & 127);
    int mid_x = (int) (mid_shift & 127);

    if (hides) {
        scr_blit(far_mask, far_x - 128, FAR_Y + down, SCR_DARK);
        scr_blit(far_mask, far_x, FAR_Y + down, SCR_DARK);
    }
    scr_blit(far_lit, far_x - 128, FAR_Y + down, SCR_LIGHT);
    scr_blit(far_lit, far_x, FAR_Y + down, SCR_LIGHT);
    scr_blit(mid_mask, mid_x - 128, MID_Y + down, SCR_DARK);
    scr_blit(mid_mask, mid_x, MID_Y + down, SCR_DARK);
    scr_blit(mid_lit, mid_x - 128, MID_Y + down, SCR_LIGHT);
    scr_blit(mid_lit, mid_x, MID_Y + down, SCR_LIGHT);
}

/* Draws the sun with the top of its box in a row, and its rays, which turn with the time, if `has_rays`. */
static void draw_sun(int x, int y, unsigned int time, int has_rays)
{
    draw_marks_at(&RP_SCR_RECT, sun_marks, SUN_MARKS, AT(x, y));
    if (has_rays) {
        draw_lines_at(&RP_SCR_MOVE, ray_points + RAY_WORDS * ((time >> 16) & (RAY_PHASES - 1)), 8,
                      SCR_POINT(x + SUN_SIDE / 2 - RAY_BIAS, y + SUN_SIDE / 2 - RAY_BIAS));
    }
}

/* Draws the first stars of the list that are out, `count` of them, and lets one of them twinkle. */
static void draw_stars(int count, unsigned int time)
{
    int dark = (int) ((time >> 17) & 31);

    if (count > STARS) {
        count = STARS;
    }
    if (count <= 0) {
        return;
    }
    draw_marks_at(&RP_SCR_RECT, star_marks + STARS - count, count, 0);
    if (dark < count) {
        RP_SCR_RECT = star_marks[STARS - 1 - dark] & ~((unsigned int) SCR_LIGHT << 27);
    }
}

/* Draws the smoke of the yurt: puffs that rise from a place, grow, and drift with the wind. */
static void draw_smoke(unsigned int from, unsigned int time)
{
    int x = (int) (from & 0xffff);
    int y = (int) (from >> 16);
    int puff;

    for (puff = 0; puff < 6; ++puff) {
        int age = (int) (((time >> 14) + (unsigned int) puff * 43) & 255); /* a puff lives for four seconds */
        int row = y - 2 - (age >> 3);
        int col = x + (age >> 4) + (sine(age + puff * 40) * (age >> 5) >> 6);

        if (row >= 0 && col >= 0 && col < SCR_WIDTH - 4) {
            int side = 1 + (age >> 7);
            scr_rect(col, row, side + (age >> 6 & 1), side, SCR_LIGHT);
        }
    }
}

/* What a value has grown to, a step for every 2^`shift` microseconds, from `from` up to `to` at the most */
static int grown(unsigned int time, int shift, int from, int to)
{
    int value = from + (int) (time >> shift);

    return value > to ? to : value;
}

/* The same, falling from `from` down to `to` at the least */
static int fallen(unsigned int time, int shift, int from, int to)
{
    int value = from - (int) (time >> shift);

    return value < to ? to : value;
}

/* Time that has passed since a moment of a scene, or 0 before it */
static unsigned int since(unsigned int time, unsigned int moment)
{
    return time > moment ? time - moment : 0;
}

/* Dawn: the stars go out, rows of the sky light up from the horizon, and the sun rises behind the far range. */
static void show_dawn(unsigned int time)
{
    int glow = time < 7 * SECOND ? grown(since(time, SECOND), 19, 0, GLOW_ROWS)
                                 : fallen(time - 7 * SECOND, 18, GLOW_ROWS, 0);
    int sun_y = fallen(since(time, 5 * SECOND / 2), 17, HIDDEN_Y, 1);

    draw_stars(fallen(since(time, 3 * SECOND / 2), 17, STARS, 0), time);
    draw_marks_at(&RP_SCR_RECT, glow_marks, glow, 0);
    draw_sun(SUN_X, sun_y, time, sun_y < 8);
    draw_land(0, 0, 0, 1);
}

/* The burkut comes: the eye looks up, and a point over the range grows into a bird that flies straight at it. */
static void show_arrival(unsigned int time)
{
    int size = grown(since(time, SECOND), 16, 6, BURKUT_SIZE);
    int sway = sine(time >> 16) * 10 >> 6;

    draw_sun(SUN_X, 1, time, 1);
    draw_land(0, 0, grown(time, 17, 0, SKY_DOWN), 1);
    if (time > SECOND) {
        draw_burkut(time, TOWARDS + (unsigned int) sway, 30, size, MIDDLE(64 + sway, 30));
    }
}

/*
    The flight: the burkut flies to the right, seen from above, and the land
    passes: the far range slowly, the near hills fast, the clouds between
    them. At the end the burkut flies away.
*/
static void show_flight(unsigned int time)
{
    unsigned int end = since(time, 11 * SECOND);
    int size = fallen(end, 15, BURKUT_SIZE, 6);
    int cloud_x = 150 - (int) (time >> 16);
    int bob = sine(time >> 14) * 2 >> 6;

    draw_sun(SUN_X, 1, time, 1);
    scr_blit(cloud, cloud_x, 1, SCR_FLIP);
    scr_blit(cloud, cloud_x + 96, 5, SCR_FLIP);
    draw_land(0u - (time >> 17), 0u - (time >> 15), SKY_DOWN, 0);
    draw_burkut(time, ALONG + (unsigned int) (sine(time >> 17) * 5 >> 6), (unsigned int) (40 + (sine(time >> 16) * 5 >> 6)), size,
               MIDDLE(58 + (int) (end >> 16), 31 + bob - (int) (end >> 19)));
}

/* The yurt of the scenes `HOME` and `NIGHT`; it turns slowly, as if the eye walked around it. */
static void show_yurt(unsigned int time, unsigned int door, unsigned int light)
{
    draw_marks_at(&RP_SCR_RECT, tuft_marks, TUFTS, 0);
    draw_yurt(time >> 16, 20, 76, MIDDLE(64, 36), door, light);
    draw_smoke(places[YURT_APEX], time);
}

/* Home: a yurt in the evening. Smoke rises, and the sun sets behind the far range. */
static void show_home(unsigned int time)
{
    int sun_y = grown(since(time, 2 * SECOND), 17, 1, HIDDEN_Y);

    draw_sun(SUNSET_X, sun_y, time, sun_y < 8);
    draw_land(HOME_SHIFT, HOME_SHIFT, 0, 1);
    show_yurt(time, STYLE_DOOR, LIGHT);
}

/* Night: the stars come out, the moon rises, a star falls, and the door of the yurt is lit. */
static void show_night(unsigned int time)
{
    unsigned int fall = since(time, 5 * SECOND) >> 13;

    draw_stars(grown(time, 17, 0, STARS), time);
    draw_marks_at(&RP_SCR_RECT, moon_marks, MOON_MARKS, AT(MOON_X, fallen(since(time, SECOND), 17, HIDDEN_Y, 2)));
    if (fall > 0 && fall < 110) {
        RP_SCR_MOVE = SCR_POINT(4 + fall, fall >> 2);
        RP_SCR_LINE = SCR_POINT(14 + fall, 2 + (fall >> 2));
    }
    draw_land(HOME_SHIFT, HOME_SHIFT, 0, 1);
    show_yurt(time + lengths[HOME], STYLE_BRIGHT, NIGHT_LIGHT);
}

/* The name of the machine: it is uncovered from the left, the lines of what it is follow, and it flashes twice. */
static void show_name(unsigned int time)
{
    int open = (int) (time >> 13);
    int left = (SCR_WIDTH - NAME_WIDTH) / 2;
    unsigned int flash = since(time, 11 * SECOND / 2) >> 17;

    scr_blit(name, left, 6, SCR_LIGHT);
    if (open < NAME_WIDTH) {
        scr_rect(left + open, 6, NAME_WIDTH - open, NAME_HEIGHT, SCR_DARK);
    }
    if (time > 2 * SECOND) {
        scr_text(4, 2, "a 32-bit computer");
    }
    if (time > 11 * SECOND / 4) {
        scr_text(5, 4, "with its own");
    }
    if (time > 7 * SECOND / 2) {
        scr_text(6, 3, "AI accelerator");
    }
    if (time > 17 * SECOND / 4) {
        scr_text(7, 1, "and instruction set");
    }
    if (flash == 1 || flash == 3) {
        scr_rect(0, 3, SCR_WIDTH, NAME_HEIGHT + 6, SCR_FLIP);
    }
}

int main(void)
{
    int scene = DAWN;
    int event = SHOW_NONE;
    unsigned int pictures = 0;
    unsigned int start;
    unsigned int scene_start;
    unsigned int shown;

    if (scr_open() != 0) {
        put_str("film: another program has the screen\n");
        return 1;
    }
    put_str("film: a press of the button shows the next scene; hold it for two seconds to end\n");

    if (show_title("FILM", "A day in the high\nmountains, a minute\nlong. The chip draws\nevery picture of it\nwhile you watch.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }

    take_model(burkut_sides, BURKUT_SIDES);
    start = RP_TIME;
    scene_start = start;
    while (event != SHOW_LEAVE && scene < SCENES) {
        unsigned int time = RP_TIME - scene_start;

        if (event == SHOW_NEXT || time >= lengths[scene]) {
            ++scene;
            scene_start += time;
            if (scene == HOME) {
                take_model(yurt_sides, YURT_SIDES);
            }
            event = SHOW_NONE;
            continue;
        }

        scr_clear();
        if (scene == DAWN) {
            show_dawn(time);
        } else if (scene == ARRIVAL) {
            show_arrival(time);
        } else if (scene == FLIGHT) {
            show_flight(time);
        } else if (scene == HOME) {
            show_home(time);
        } else if (scene == NIGHT) {
            show_night(time);
        } else {
            show_name(time);
        }
        scr_show();
        ++pictures;

        event = show_button();
    }
    shown = RP_TIME - start;

    scr_close();
    if (shown >= SECOND) {
        put_str("The film ran for ");
        put_dec(shown / SECOND);
        put_str(" seconds and showed ");
        put_dec(pictures);
        put_str(" pictures: ");
        put_dec(pictures / (shown / SECOND));
        put_str(" a second. The land is pictures that the program moves; the burkut and the yurt are models that the chip turned in space for every picture.\n");
    }

    return 0;
}
