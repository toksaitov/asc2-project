/*
    Shapes: models that turn in space, as lines and with filled sides.

    Three models: a cube, a yurt, and a burkut that beats its wings. Each is
    shown in two ways, and a press of the button goes to the next one:

    * lines: the edges of the model, drawn by the RP2040;
    * solid: the sides that the eye sees, each filled as bright as the light
      falls on it. The program fills a side row by row; five shades are
      made of lit and dark rows. A side is filled a little smaller than it
      is, so that a dark seam parts it from its neighbours; a side in the
      shade gets its edges drawn.

    What the chip does for a picture: every point of the model is turned by
    a matrix of nine numbers, which is nine multiplications and six
    additions. The matrix sits in the weights of the array of the chip, and
    one instruction, `dotm`, turns a point. For a side one `dotm` gives two
    answers: how much light falls on it, which is the dot product of the
    direction that the side faces and the direction of the light, and
    whether the eye is in front of it, which is the dot product of that
    direction and the place of the eye. In 3D the array does not make the
    pictures come faster than good code of the older instructions would, by
    the measurements of `../asc2-kb/array-design.md`; it makes the code
    short.

    The burkut is not one closed body, so its sides are drawn from the far
    ones to the near ones, and a nearer side covers what is behind it.

    Nearly every instruction that the chip has to fetch costs it a request
    on the bus, and so does every word of data. The work for a point, an
    edge, and a row of a side is therefore done by short loops in assembly
    that run out of the cache of the chip (`shapes_asm.s`).

    The button held down for two seconds ends the program.
*/

#include "lib/machine.h"
#include "lib/show.h"

#include "shapes_models.h"

#define MAX_POINTS 21
#define MAX_SIDES  17

#define TOP_ROW    8 /* the picture is below the first row of text */

/* The middle of a model on the screen for `place_points`: the column 64 of the row 36 */
#define MIDDLE     (64u | 36u << 16)

#define STYLE_DOOR 7 /* the style of a side that is always dark and covers what is behind it; see `fill_sides` */

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

/* The functions of `shapes_asm.s` and `lib/array.s` */

void set_turn(unsigned int around, unsigned int tilt, int size, const signed char *sines, unsigned int light, unsigned int *seen);
void turn_points(const unsigned int *points, int *sums, int count);
void place_points(const int *sums, unsigned int *places, int count, const short *zoom, unsigned int middle);
void draw_edges(volatile unsigned int *move, const unsigned int *places, const unsigned char *edges, int count);
void style_sides(const unsigned int *sides, unsigned int *styles, int count);
void sort_sides(const unsigned int *sides, const int *sums, int *depths, unsigned char *order, int count);
void fill_sides(const unsigned int *job);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);

typedef struct model
{
    const char *name;
    unsigned int *points;
    int point_count;
    const unsigned char *edges;
    int edge_count;
    unsigned int *sides;
    int side_count;
    int dark_side;        /* a side that is always dark, or -1 */
    int is_closed;        /* one closed body: no side covers another one that faces the eye */
    int motion;
    int size;             /* how large it is drawn, in 64ths */
} model_t;

/* How a model moves */
#define TUMBLES 0 /* it turns around two axes */
#define TURNS   1 /* it stands and turns around its upright axis, seen a little from above, now more and now less */
#define FLIES   2 /* it comes towards the eye, seen from above, and swings to the left and to the right */

#define MODELS 3

static const model_t models[MODELS] = {
    { "cube", cube_points, CUBE_POINTS, cube_edges, CUBE_EDGES, cube_sides, CUBE_SIDES, -1, 1, TUMBLES, 72 },
    { "yurt", yurt_points, YURT_POINTS, yurt_edges, YURT_EDGES, yurt_sides, YURT_SIDES, YURT_DOOR, 1, TURNS, 88 },
    { "burkut", burkut_points, BURKUT_POINTS, burkut_edges, BURKUT_EDGES, burkut_sides, BURKUT_SIDES, -1, 0, FLIES, 100 }
};

/* Where the light comes from, as the eye sees it, in 64ths: from the left (-29), from above (42), and from the front (39) */
#define LIGHT 0x272ae3u

/* The sine of an angle in 64ths; a circle is 256 */
#define sine(angle) ((int) sines[(angle) & 255])

static unsigned int seen[2];            /* the light and the eye as the model sees them; see `set_turn` */
static int sums[3 * MAX_POINTS];        /* the points that have turned, in 64ths; y grows downwards */
static unsigned int places[MAX_POINTS]; /* their places on the screen: the column, and the row in the upper half */
static unsigned int styles[MAX_SIDES];  /* how every side is drawn in this picture */
static int depths[MAX_SIDES];
static unsigned char order[MAX_SIDES];  /* the numbers of the sides in the order of drawing */
static unsigned short rows[SCR_HEIGHT]; /* for `fill_sides`: the two ends of a side in every row of the screen, a byte each */
static unsigned int job[JOB_WORDS];

/* What every way of drawing has shown: pictures and microseconds, a model in lines and then solid */
static unsigned int total_pictures[2 * MODELS];
static unsigned int total_us[2 * MODELS];

/* Beats the wings of the burkut: moves the points of the elbows and of the tips, and sets the directions that the four parts of the wings face. */
static void beat_wings(unsigned int time)
{
    const unsigned int *beat = wing_beats + BEAT_WORDS * ((time >> 15) & (BEATS - 1)); /* a beat is about a second */
    unsigned int *point = burkut_points + BURKUT_WINGS + 2;
    unsigned int *part = burkut_sides + SIDE_WORDS * BURKUT_WING_SIDES;

    /* A wing is five points: two at the body, which stay, then the two of the elbow and the tip */
    point[0] = beat[0];
    point[1] = beat[1];
    point[2] = beat[2];
    point[5] = beat[3];
    point[6] = beat[4];
    point[7] = beat[5];

    /* The first word of a side is the direction that it faces */
    part[0] = beat[6];
    part[SIDE_WORDS] = beat[7];
    part[2 * SIDE_WORDS] = beat[8];
    part[3 * SIDE_WORDS] = beat[9];
}

/* Gets ready to draw a model: the job of `fill_sides`, and the sides in the order in which the model lists them. */
static void take_model(const model_t *model)
{
    int i;

    job[JOB_SIDES] = (unsigned int) model->sides;
    job[JOB_PLACES] = (unsigned int) places;
    job[JOB_STYLES] = (unsigned int) styles;
    job[JOB_ORDER] = (unsigned int) order;
    job[JOB_COUNT] = (unsigned int) model->side_count;
    job[JOB_COVERS] = (unsigned int) !model->is_closed;
    job[JOB_PARTS] = (unsigned int) parts;
    job[JOB_ROWS] = (unsigned int) rows;
    job[JOB_RECT] = (unsigned int) &RP_SCR_RECT;
    job[JOB_MOVE] = (unsigned int) &RP_SCR_MOVE;
    job[JOB_LIT_ROWS] = (unsigned int) lit_rows;
    for (i = 0; i < MAX_SIDES; ++i) {
        order[i] = (unsigned char) i;
    }
}

static void show_model(const model_t *model, int is_solid, unsigned int time)
{
    unsigned int around;
    unsigned int tilt;

    if (model->motion == TUMBLES) {
        around = time >> 14;
        tilt = time >> 15;
    } else if (model->motion == TURNS) {
        around = time >> 15;
        tilt = (unsigned int) (20 + (sine(time >> 16) * 10 >> 6));
    } else {
        /* The nose points at the eye at three quarters of a circle; it swings a little to each side */
        around = (unsigned int) (192 + (sine(time >> 15) * 20 >> 6));
        tilt = (unsigned int) (28 + (sine(time >> 16) * 8 >> 6));
        beat_wings(time);
    }

    set_turn(around, tilt, model->size, sines, LIGHT, seen);
    turn_points(model->points, sums, model->point_count);
    place_points(sums, places, model->point_count, zoom, MIDDLE);

    scr_rect(0, TOP_ROW, SCR_WIDTH, SCR_HEIGHT - TOP_ROW, SCR_DARK);
    if (!is_solid) {
        draw_edges(&RP_SCR_MOVE, places, model->edges, model->edge_count);
    } else {
        set_weights(seen[0], seen[1], 0);
        style_sides(model->sides, styles, model->side_count);
        if (model->dark_side >= 0 && styles[model->dark_side] != 0) {
            styles[model->dark_side] = STYLE_DOOR;
        }
        if (!model->is_closed) {
            sort_sides(model->sides, sums, depths, order, model->side_count);
        }
        fill_sides(job);
    }
    scr_show();
}

static void show_name(const model_t *model, int is_solid)
{
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_DARK);
    scr_text(0, 0, "SHAPES");
    scr_text(0, 8, model->name);
    scr_text(0, 15, is_solid ? "solid" : "lines");
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_FLIP);
}

static void report(void)
{
    int i;

    for (i = 0; i < 2 * MODELS; ++i) {
        if (total_us[i] >= 1000) {
            put_str("The ");
            put_str(models[i >> 1].name);
            put_str((i & 1) ? " with filled sides: " : " as lines: ");
            put_dec((total_pictures[i] * 1000 + total_us[i] / 2000) / (total_us[i] / 1000));
            put_str(" pictures a second.\n");
        }
    }
}

int main(int argc, char *argv[])
{
    int view = 0; /* a model twice: in lines and then solid */
    int event = SHOW_NONE;

    if (scr_open() != 0) {
        put_str("shapes: another program has the screen\n");
        return 1;
    }
    put_str("shapes: a press of the button shows the next shape; hold it for two seconds to end\n");

    if (show_title("SHAPES", "Models turn in space.\nFor every corner the\nchip does the nine\nmultiplications of\nthe turn in one step.",
                   "press:next  hold:end")) {
        event = SHOW_LEAVE;
    }
    show_tour(argc, 5, 31); /* each of the six views once */
    while (event != SHOW_LEAVE) {
        const model_t *model = &models[view >> 1];
        unsigned int start = RP_TIME;
        unsigned int pictures = 0;

        scr_clear();
        take_model(model);
        show_name(model, view & 1);
        do {
            show_model(model, view & 1, RP_TIME);
            ++pictures;
            event = show_button();
        } while (event == SHOW_NONE);
        total_pictures[view] += pictures;
        total_us[view] += RP_TIME - start;

        view = view == 2 * MODELS - 1 ? 0 : view + 1;
    }

    scr_close();
    report();

    return 0;
}
