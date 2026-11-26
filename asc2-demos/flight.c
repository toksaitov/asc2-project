/*
    Flight: a flight by night through a valley between mountains, all of
    lines, under the stars and the moon.

    The land is a grid of heights. For every picture the chip finds the
    place of each point of the eight rows ahead on the screen, about 150
    points, and the RP2040 draws the lines between them. A press of the
    button changes how the land is drawn:

    * solid: the land hides what is behind it. The rows are drawn from the
      farthest to the nearest, and the screen is made dark below each row
      before its lines are drawn.
    * wire: only the lines; the land behind and the stars shine through.

    Nearly every instruction that the chip has to fetch costs it a request
    on the bus, and so does every word of data. The work for a point is
    therefore done by short loops in assembly that run out of the cache of
    the chip (`flight_asm.s`): two multiplications for a point, and a word
    written for a line.

    The button held down for two seconds ends the program.
*/

#include "lib/machine.h"
#include "lib/show.h"

#include "flight_land.h"

/* The words of the job of `fly` */
#define JOB_LAND   0
#define JOB_SCALES 1
#define JOB_WIDE   2
#define JOB_PLACES 3
#define JOB_FLOWN  4
#define JOB_ASIDE  5
#define JOB_SOLID  6
#define JOB_RECT   7
#define JOB_MOVE   8
#define JOB_WORDS  9

#define WAYS       2       /* the land is drawn solid or as wire */
#define NAME_US    2000000 /* how long the name of the way of drawing stays on the screen */

/* The functions of `flight_asm.s` */

void draw_marks(volatile unsigned int *rect, const unsigned int *marks, int count);
void fly(const unsigned int *job);

/* The sine of an angle in 64ths; a circle is 256 */
#define sine(angle) ((int) sines[(angle) & 255])

static unsigned int places[2 * LAND_COLS]; /* the places of the points of two rows on the screen */
static unsigned int job[JOB_WORDS];

/* What each way of drawing has shown: pictures and microseconds */
static unsigned int total_pictures[WAYS];
static unsigned int total_us[WAYS];

static void report(void)
{
    int i;

    for (i = 0; i < WAYS; ++i) {
        if (total_us[i] >= 1000) {
            put_str(i == 0 ? "The flight over solid land: " : "The flight over a land of wire: ");
            put_dec((total_pictures[i] * 1000 + total_us[i] / 2000) / (total_us[i] / 1000));
            put_str(" pictures a second.\n");
        }
    }
}

int main(int argc, char *argv[])
{
    int way = 0;
    int event = SHOW_NONE;

    if (scr_open() != 0) {
        put_str("flight: another program has the screen\n");
        return 1;
    }
    put_str("flight: a press of the button changes how the land is drawn; hold it for two seconds to end\n");

    job[JOB_LAND] = (unsigned int) land;
    job[JOB_SCALES] = (unsigned int) scales;
    job[JOB_WIDE] = (unsigned int) wide;
    job[JOB_PLACES] = (unsigned int) places;
    job[JOB_RECT] = (unsigned int) &RP_SCR_RECT;
    job[JOB_MOVE] = (unsigned int) &RP_SCR_MOVE;

    if (show_title("FLIGHT", "A flight by night\nthrough a valley.\nThe chip places 150\npoints of the land\nfor every picture.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }
    show_tour(argc, 10, 21); /* both ways of drawing once */
    while (event != SHOW_LEAVE) {
        unsigned int start = RP_TIME;
        unsigned int pictures = 0;

        job[JOB_SOLID] = (unsigned int) (way == 0);
        do {
            unsigned int time = RP_TIME;

            /* Two squares in a second; the valley bends once in its 32 rows, and the eye follows it */
            job[JOB_FLOWN] = time >> 11;
            job[JOB_ASIDE] = (unsigned int) (sine(time >> 16) * 8);

            scr_clear();
            draw_marks(&RP_SCR_RECT, sky, SKY_MARKS);
            fly(job);
            if (time - start < NAME_US) {
                scr_rect(0, 0, 6 * 13, 8, SCR_DARK);
                scr_text(0, 0, way == 0 ? "FLIGHT  solid" : "FLIGHT  wire");
            }
            scr_show();
            ++pictures;
            event = show_button();
        } while (event == SHOW_NONE);
        total_pictures[way] += pictures;
        total_us[way] += RP_TIME - start;

        way = way == WAYS - 1 ? 0 : way + 1;
    }

    scr_close();
    report();

    return 0;
}
