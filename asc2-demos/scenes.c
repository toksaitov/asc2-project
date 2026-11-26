/*
    Three scenes for the screen: text, a picture, and a short film. The
    program draws the same pixels every time and ends, so a test can compare
    what the screen shows at the end (`scenes.frame`) and what the console
    has printed (`scenes.expect`).

    Under the kernel the console takes the screen back when the program
    ends, so there the program stays on every scene for a while: `REST_US`
    microseconds, or until the button is pressed.
*/

#include "lib/machine.h"
#include "pictures.h"

#ifdef RACH2
#define REST_US 3000000
#else
#define REST_US 0
#endif

static void rest(void)
{
    unsigned int start = RP_TIME;
    unsigned int presses = RP_BTN_COUNT;

    while (REST_US != 0 && RP_TIME - start < REST_US && RP_BTN_COUNT == presses) {
    }
}

static void scene_text(void)
{
    scr_clear();
    scr_text(0, 0, "Almurut II");
    scr_text(1, 0, "a 32-bit computer");
    scr_text(2, 0, "with its own");
    scr_text(3, 0, "instruction set");
    scr_text(5, 0, "!\"#$%&'()*+,-./:;<=>?");
    scr_text(6, 0, "0123456789 ABCDEFGHIJ");
    scr_text(7, 0, "abcdefghij [\\]^_`{|}~");
    scr_show();
}

/* The mountains with a sky; the film flies the small burkut through it */
static void draw_land(void)
{
    scr_clear();
    scr_blit(mountains, 0, 38, SCR_LIGHT);
    scr_rect(0, 62, SCR_WIDTH, 2, SCR_LIGHT);
}

static void scene_picture(void)
{
    draw_land();
    scr_blit(burkut_big, 48, 6, SCR_LIGHT);
    scr_text(0, 0, "Burkut");
    scr_show();
}

static void scene_film(void)
{
    int x;
    int y = 20;
    int turn = 0;

    for (x = -16; x < SCR_WIDTH; x += 4) {
        draw_land();
        /* The wings go down in every second frame, and the burkut sinks and rises a little */
        scr_blit((turn & 1) ? burkut_down : burkut_up, x, y + ((turn & 2) ? 1 : -1), SCR_LIGHT);
        scr_show();
        ++turn;
    }
}

int main(void)
{
    if (scr_open() != 0) {
        put_str("scenes: another program has the screen\n");
        return 1;
    }
    put_str("scenes of the screen\n");

    scene_text();
    put_str("text is shown\n");
    rest();

    scene_picture();
    put_str("a picture is shown\n");
    rest();

    scene_film();
    put_str("a film is shown\n");

    /* The last frame: the picture again, in a frame that turns its own pixels over */
    draw_land();
    scr_blit(burkut_big, 48, 6, SCR_LIGHT);
    scr_text(0, 0, "Almurut II");
    scr_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, SCR_FLIP);
    scr_rect(2, 2, SCR_WIDTH - 4, SCR_HEIGHT - 4, SCR_FLIP);
    scr_show();

    put_str("frames: ");
    put_dec(RP_SCR_FRAMES);
    put_str("\n");
    rest();
    scr_close();

    return 0;
}
