#include "show.h"

#include "machine.h"

#define HOLD_US  2000000 /* the button held for this long ends a program */
#define TITLE_US 6000000 /* a title card stays for this long */

static int is_watching;         /* the presses before the first look do not count */
static unsigned int presses;    /* presses that have been seen */
static int is_down;             /* a press has begun and has not ended */
static unsigned int down_since;
static unsigned int looked_at;  /* when the button was looked at last and no press had begun */

/* The tour: the program shows itself; see `show_tour` */
static unsigned int tour_next_us;  /* a press is made up this often; 0 for never */
static unsigned int tour_leave_us; /* the program ends after this time; 0 without the tour */
static unsigned int tour_since;
static unsigned int tour_pressed;  /* when a press was made up last */

int show_tour(int argc, unsigned int next, unsigned int leave)
{
#ifdef RACH2
    if (argc > 1) {
        tour_next_us = next * 1000000;
        tour_leave_us = leave * 1000000;
        tour_since = RP_TIME;
        tour_pressed = tour_since;
        return 1;
    }
#else
    /* The bare machine starts a program without arguments, and the tour is a program of the kernel */
    (void) argc;
    (void) next;
    (void) leave;
#endif

    return 0;
}

int show_tour_over(void)
{
    return tour_leave_us != 0 && RP_TIME - tour_since > tour_leave_us;
}

void show_forget(void)
{
    is_watching = 0;
    is_down = 0;
}

int show_button(void)
{
    unsigned int now = RP_BTN_COUNT;
    unsigned int time = RP_TIME;

    if (tour_leave_us != 0) {
        if (time - tour_since > tour_leave_us) {
            return SHOW_LEAVE;
        }
        if (tour_next_us != 0 && time - tour_pressed > tour_next_us) {
            tour_pressed = time;
            return SHOW_NEXT;
        }
    }

    if (!is_watching) {
        is_watching = 1;
        presses = now;
        looked_at = time;
    }
    if (!is_down) {
        if (now == presses) {
            looked_at = time;
            return SHOW_NONE;
        }
        presses = now;
        is_down = 1;

        /*
            The press has begun at some time since the last look. It counts
            from that look, so that a program that looks seldom, because a
            picture takes it long, does not take a hold for a press.
        */
        down_since = looked_at;
    }
    if (RP_BTN_STAT == 0) {
        is_down = 0;
        return SHOW_NEXT;
    }
    if (RP_TIME - down_since > HOLD_US) {
        is_down = 0;
        presses = RP_BTN_COUNT;
        return SHOW_LEAVE;
    }

    return SHOW_NONE;
}

int show_wait(unsigned int us)
{
    unsigned int start = RP_TIME;
    int event;

    do {
        event = show_button();
    } while (event == SHOW_NONE && RP_TIME - start < us);

    return event;
}

void show_header(const char *name)
{
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_DARK);
    scr_text(0, 1, name);
    scr_rect(0, 0, SCR_WIDTH, 8, SCR_FLIP);
}

void show_lines(int row, const char *text)
{
    int col = 0;

    while (*text != '\0') {
        if (*text == '\n') {
            ++row;
            col = 0;
        } else {
            RP_SCR_CHAR = (unsigned int) row << 16 | (unsigned int) col << 8 | (unsigned char) *text;
            ++col;
        }
        ++text;
    }
}

int show_title(const char *name, const char *text, const char *hint)
{
    scr_clear();
    show_header(name);
    show_lines(2, text);
    scr_text(7, 0, hint);
    scr_show();

    return show_wait(TITLE_US) == SHOW_LEAVE;
}

void show_bar(int y, unsigned int done, unsigned int goal)
{
    unsigned int room = SCR_WIDTH - 4;
    unsigned int width = goal == 0 ? 0 : done >= goal ? room : done * room / goal;

    scr_rect(0, y, SCR_WIDTH, 7, SCR_LIGHT);
    scr_rect(1, y + 1, SCR_WIDTH - 2, 5, SCR_DARK);
    scr_rect(2, y + 2, (int) width, 3, SCR_LIGHT);
}

/* The cells of the digits, 3 by 5, a row in 3 bits, the top row and the left cell first */
static const unsigned short digits[10] = {
    075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717
};

int show_big(int x, int y, const char *text, int scale)
{
    while (*text != '\0') {
        unsigned int cells = 0;
        int width = 3;
        int row;

        if (*text >= '0' && *text <= '9') {
            cells = digits[*text - '0'];
        } else if (*text == 'x') {
            cells = 000525;
        } else if (*text == '.') {
            cells = 000004;
            width = 1;
        }
        for (row = 0; row < 5; ++row) {
            unsigned int bits = (cells >> (3 * (4 - row))) & 7;
            int col;
            for (col = 0; col < width; ++col) {
                if (bits & (4u >> col)) {
                    scr_rect(x + col * scale, y + row * scale, scale, scale, SCR_LIGHT);
                }
            }
        }
        x += (width + 1) * scale;
        ++text;
    }

    return x;
}
