/*
    The models of `shapes.c` and its tables. `../asc2-kb/models/shapes_gen.py` has written this file;
    change the models there.

    A point is a word of three signed bytes: x, y, and z, x in the low byte. x goes to the right, y up,
    and z towards the eye. An edge is two bytes, its two points.

    A side is three words. The first one is the direction that the side faces, three signed bytes with
    the length 127, and in the fourth byte how far the plane of the side lies from the middle of the
    model, in 127ths of 32. The other two words are eight bytes: the number of its points (3 or 4; 128
    more if both of its faces can be seen), then the points, which go around against the clock for an
    eye that looks at the side from outside, and the first point once more.
*/

#define SIDE_WORDS     3
#define SIDE_BOTH_WAYS 128

#define CUBE_POINTS 8
#define CUBE_EDGES  12
#define CUBE_SIDES  6

static unsigned int cube_points[CUBE_POINTS] = {
    0xf5f5f5, 0xf5f50b, 0xf50b0b, 0xf50bf5, 0x0bf5f5, 0x0bf50b,
    0x0b0b0b, 0x0b0bf5
};

static const unsigned char cube_edges[2 * CUBE_EDGES] = {
    2, 3, 1, 2, 0, 1, 0, 3, 4, 5, 5, 6, 6, 7, 4, 7, 1, 5, 0, 4,
    3, 7, 2, 6
};

static unsigned int cube_sides[SIDE_WORDS * CUBE_SIDES] = {
    0x2c810000, 0x01020304, 0x00000300,
    0x2c7f0000, 0x06050404, 0x00000407,
    0x2c008100, 0x05010004, 0x00000004,
    0x2c007f00, 0x07030204, 0x00000206,
    0x2c00007f, 0x06020104, 0x00000105,
    0x2c000081, 0x03070404, 0x00000400
};

#define YURT_POINTS 21
#define YURT_EDGES  36
#define YURT_SIDES  17
#define YURT_DOOR   1 /* the side that is the door: always dark */
#define YURT_APEX   16 /* the point at the top of the roof */

static unsigned int yurt_points[YURT_POINTS] = {
    0x00f511, 0x0cf50c, 0x11f500, 0x0cf5f4, 0x00f5ef, 0xf4f5f4,
    0xeff500, 0xf4f50c, 0x000011, 0x0c000c, 0x110000, 0x0c00f4,
    0x0000ef, 0xf400f4, 0xef0000, 0xf4000c, 0x000b00, 0x04f510,
    0x08f50e, 0x08fd0e, 0x04fd10
};

static const unsigned char yurt_edges[2 * YURT_EDGES] = {
    8, 9, 1, 9, 0, 1, 0, 8, 19, 20, 18, 19, 17, 18, 17, 20, 9, 10, 2, 10,
    1, 2, 10, 11, 3, 11, 2, 3, 11, 12, 4, 12, 3, 4, 12, 13, 5, 13, 4, 5,
    13, 14, 6, 14, 5, 6, 14, 15, 7, 15, 6, 7, 8, 15, 0, 7, 9, 16, 8, 16,
    10, 16, 11, 16, 12, 16, 13, 16, 14, 16, 15, 16
};

static unsigned int yurt_sides[SIDE_WORDS * YURT_SIDES] = {
    0x3e310075, 0x01090804, 0x00000800,
    0x40390072, 0x12131404, 0x00001411,
    0x3e750031, 0x020a0904, 0x00000901,
    0x3e7500cf, 0x030b0a04, 0x00000a02,
    0x3e31008b, 0x040c0b04, 0x00000b03,
    0x3ecf008b, 0x050d0c04, 0x00000c04,
    0x3e8b00cf, 0x060e0d04, 0x00000d05,
    0x3e8b0031, 0x070f0e04, 0x00000e06,
    0x3ecf0075, 0x00080f04, 0x00000f07,
    0x241c6843, 0x08091003, 0x00000010,
    0x2443681c, 0x090a1003, 0x00000010,
    0x244368e4, 0x0a0b1003, 0x00000010,
    0x241c68bd, 0x0b0c1003, 0x00000010,
    0x24e468bd, 0x0c0d1003, 0x00000010,
    0x24bd68e4, 0x0d0e1003, 0x00000010,
    0x24bd681c, 0x0e0f1003, 0x00000010,
    0x24e46843, 0x0f081003, 0x00000010
};

#define BURKUT_POINTS 18
#define BURKUT_EDGES  27
#define BURKUT_SIDES  13
#define BURKUT_WINGS  6 /* the first point of the wings: for each wing the two points at the body, the two of the elbow, and the tip */
#define BURKUT_WING_SIDES 8 /* the first side of the wings: for each wing the inner part and the outer part */

static unsigned int burkut_points[BURKUT_POINTS] = {
    0x000010, 0x0000f6, 0x000601, 0x00fb01, 0x050001, 0xfb0001,
    0x030107, 0x0301fb, 0x0f0105, 0x0f01fd, 0x1d0100, 0xfd0107,
    0xfd01fb, 0xf10105, 0xf101fd, 0xe30100, 0x0501ed, 0xfb01ed
};

static const unsigned char burkut_edges[2 * BURKUT_EDGES] = {
    2, 4, 0, 4, 0, 2, 1, 4, 1, 2, 3, 4, 0, 3, 1, 3, 3, 5, 0, 5,
    1, 5, 2, 5, 7, 9, 8, 9, 6, 8, 6, 7, 9, 10, 8, 10, 11, 13, 13, 14,
    12, 14, 11, 12, 13, 15, 14, 15, 16, 17, 1, 16, 1, 17
};

static unsigned int burkut_sides[SIDE_WORDS * BURKUT_SIDES] = {
    0x105f4f20, 0x00040203, 0x00000002,
    0x0d5c4dd6, 0x02040103, 0x00000001,
    0x0f57a91d, 0x00030403, 0x00000004,
    0x0c55abd9, 0x04030103, 0x00000001,
    0x0fa9a91d, 0x00050303, 0x00000003,
    0x0cababd9, 0x03050103, 0x00000001,
    0x10a14f20, 0x00020503, 0x00000005,
    0x0da44dd6, 0x05020103, 0x00000001,
    0x00007f00, 0x08090784, 0x00000706,
    0x00007f00, 0x080a0983, 0x00000009,
    0x00007f00, 0x0e0d0b84, 0x00000b0c,
    0x00007f00, 0x0e0f0d83, 0x0000000d,
    0xfc007e0e, 0x01101183, 0x00000011
};

/* What a size at a depth is multiplied by, in 64ths: 90 / (90 - z) for z from -32 to 31. A point that is nearer looks larger. */
static const short zoom[64] = {
    47, 48, 48, 48, 49, 49, 50, 50, 51, 51, 51, 52, 52, 53, 53, 54,
    54, 55, 55, 56, 56, 57, 58, 58, 59, 59, 60, 61, 61, 62, 63, 63,
    64, 65, 65, 66, 67, 68, 69, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    78, 79, 80, 81, 82, 83, 85, 86, 87, 89, 90, 91, 93, 94, 96, 98
};

/* 65536 divided by a number of rows from 1 to 63: the CPU has no instruction that divides */
static const int parts[64] = {
    0, 65536, 32768, 21845, 16384, 13107, 10922, 9362, 8192, 7281, 6553, 5957,
    5461, 5041, 4681, 4369, 4096, 3855, 3640, 3449, 3276, 3120, 2978, 2849,
    2730, 2621, 2520, 2427, 2340, 2259, 2184, 2114, 2048, 1985, 1927, 1872,
    1820, 1771, 1724, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1424, 1394,
    1365, 1337, 1310, 1285, 1260, 1236, 1213, 1191, 1170, 1149, 1129, 1110,
    1092, 1074, 1057, 1040
};

/* The sine of an angle in 64ths; a circle is 256 */
static const signed char sines[256] = {
    0, 2, 3, 5, 6, 8, 9, 11, 12, 14, 16, 17, 19, 20, 22, 23,
    24, 26, 27, 29, 30, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59,
    59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60,
    59, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46,
    45, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 30, 29, 27, 26,
    24, 23, 22, 20, 19, 17, 16, 14, 12, 11, 9, 8, 6, 5, 3, 2,
    0, -2, -3, -5, -6, -8, -9, -11, -12, -14, -16, -17, -19, -20, -22, -23,
    -24, -26, -27, -29, -30, -32, -33, -34, -36, -37, -38, -39, -41, -42, -43, -44,
    -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -55, -56, -56, -57, -58, -59,
    -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -64, -64, -64, -64, -64,
    -64, -64, -64, -64, -64, -64, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60,
    -59, -59, -58, -57, -56, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46,
    -45, -44, -43, -42, -41, -39, -38, -37, -36, -34, -33, -32, -30, -29, -27, -26,
    -24, -23, -22, -20, -19, -17, -16, -14, -12, -11, -9, -8, -6, -5, -3, -2
};

/*
    The beat of the wings of the burkut in 32 steps, ten words each: for the left wing and then for the
    right one the two points of the elbow and the tip, and then for each wing the directions that
    its inner part and its outer part face.
*/
#define BEATS      32
#define BEAT_WORDS 10

static const unsigned int wing_beats[BEATS * BEAT_WORDS] = {
    0x0f0105, 0x0f01fd, 0x1cfc00, 0xf10105, 0xf101fd, 0xe4fc00, 0x007f00, 0x267900, 0x007f00, 0xda7900,
    0x0f0105, 0x0f01fd, 0x1cfe00, 0xf10105, 0xf101fd, 0xe4fe00, 0xf77f00, 0x167d00, 0x097f00, 0xea7d00,
    0x0e0205, 0x0e02fd, 0x1c0100, 0xf20205, 0xf202fd, 0xe40100, 0xef7d00, 0x067f00, 0x117d00, 0xfa7f00,
    0x0e0405, 0x0e04fd, 0x1c0500, 0xf20405, 0xf204fd, 0xe40500, 0xe17b00, 0xf57f00, 0x1f7b00, 0x0b7f00,
    0x0e0405, 0x0e04fd, 0x1b0700, 0xf20405, 0xf204fd, 0xe50700, 0xdb7900, 0xe57b00, 0x257900, 0x1b7b00,
    0x0e0505, 0x0e05fd, 0x1b0900, 0xf20505, 0xf205fd, 0xe50900, 0xd57700, 0xd57700, 0x2b7700, 0x2b7700,
    0x0e0505, 0x0e05fd, 0x1a0a00, 0xf20505, 0xf205fd, 0xe60a00, 0xd17500, 0xcb7300, 0x2f7500, 0x357300,
    0x0e0505, 0x0e05fd, 0x1a0c00, 0xf20505, 0xf205fd, 0xe60c00, 0xcd7500, 0xbf6d00, 0x337500, 0x416d00,
    0x0d0605, 0x0d06fd, 0x180e00, 0xf30605, 0xf306fd, 0xe80e00, 0xcb7300, 0xb76700, 0x357300, 0x496700,
    0x0e0505, 0x0e05fd, 0x190d00, 0xf20505, 0xf205fd, 0xe70d00, 0xcd7500, 0xb56500, 0x337500, 0x4b6500,
    0x0e0505, 0x0e05fd, 0x180d00, 0xf20505, 0xf205fd, 0xe80d00, 0xd17500, 0xb36300, 0x2f7500, 0x4d6300,
    0x0e0505, 0x0e05fd, 0x190d00, 0xf20505, 0xf205fd, 0xe70d00, 0xd57700, 0xb56500, 0x2b7700, 0x4b6500,
    0x0e0405, 0x0e04fd, 0x190c00, 0xf20405, 0xf204fd, 0xe70c00, 0xdb7900, 0xb76700, 0x257900, 0x496700,
    0x0e0405, 0x0e04fd, 0x190b00, 0xf20405, 0xf204fd, 0xe70b00, 0xe17b00, 0xb96900, 0x1f7b00, 0x476900,
    0x0e0205, 0x0e02fd, 0x1a0800, 0xf20205, 0xf202fd, 0xe60800, 0xef7d00, 0xc77100, 0x117d00, 0x397100,
    0x0f0105, 0x0f01fd, 0x1c0600, 0xf10105, 0xf101fd, 0xe40600, 0xf77f00, 0xd37700, 0x097f00, 0x2d7700,
    0x0f0105, 0x0f01fd, 0x1c0400, 0xf10105, 0xf101fd, 0xe40400, 0x007f00, 0xdf7b00, 0x007f00, 0x217b00,
    0x0fff05, 0x0ffffd, 0x1d0000, 0xf1ff05, 0xf1fffd, 0xe30000, 0x0c7f00, 0xf17f00, 0xf47f00, 0x0f7f00,
    0x0efe05, 0x0efefd, 0x1cfe00, 0xf2fe05, 0xf2fefd, 0xe4fe00, 0x167d00, 0x007f00, 0xea7d00, 0x007f00,
    0x0efd05, 0x0efdfd, 0x1bfb00, 0xf2fd05, 0xf2fdfd, 0xe5fb00, 0x227b00, 0x127d00, 0xde7b00, 0xee7d00,
    0x0efd05, 0x0efdfd, 0x1bf900, 0xf2fd05, 0xf2fdfd, 0xe5f900, 0x287900, 0x227b00, 0xd87900, 0xde7b00,
    0x0efc05, 0x0efcfd, 0x1bf600, 0xf2fc05, 0xf2fcfd, 0xe5f600, 0x2e7700, 0x2e7700, 0xd27700, 0xd27700,
    0x0efc05, 0x0efcfd, 0x1af500, 0xf2fc05, 0xf2fcfd, 0xe6f500, 0x347500, 0x3c6f00, 0xcc7500, 0xc46f00,
    0x0dfb05, 0x0dfbfd, 0x18f300, 0xf3fb05, 0xf3fbfd, 0xe8f300, 0x367300, 0x486900, 0xca7300, 0xb86900,
    0x0dfb05, 0x0dfbfd, 0x18f200, 0xf3fb05, 0xf3fbfd, 0xe8f200, 0x367300, 0x4c6500, 0xca7300, 0xb46500,
    0x0dfb05, 0x0dfbfd, 0x17f200, 0xf3fb05, 0xf3fbfd, 0xe9f200, 0x367300, 0x526100, 0xca7300, 0xae6100,
    0x0efc05, 0x0efcfd, 0x18f200, 0xf2fc05, 0xf2fcfd, 0xe8f200, 0x347500, 0x545f00, 0xcc7500, 0xac5f00,
    0x0efc05, 0x0efcfd, 0x18f300, 0xf2fc05, 0xf2fcfd, 0xe8f300, 0x2e7700, 0x526100, 0xd27700, 0xae6100,
    0x0efd05, 0x0efdfd, 0x18f400, 0xf2fd05, 0xf2fdfd, 0xe8f400, 0x287900, 0x4e6300, 0xd87900, 0xb26300,
    0x0efd05, 0x0efdfd, 0x19f400, 0xf2fd05, 0xf2fdfd, 0xe7f400, 0x227b00, 0x4a6700, 0xde7b00, 0xb66700,
    0x0efe05, 0x0efefd, 0x1af700, 0xf2fe05, 0xf2fefd, 0xe6f700, 0x167d00, 0x406f00, 0xea7d00, 0xc06f00,
    0x0fff05, 0x0ffffd, 0x1bf900, 0xf1ff05, 0xf1fffd, 0xe5f900, 0x0c7f00, 0x347500, 0xf47f00, 0xcc7500
};

/*
    The rows of a side that are lit, for each way of drawing a side (see `fill_sides`) and for each of
    the four rows that the first row of the side can be: the bit 31 is the first row, the bit 30 the
    next one, and so on. Of every four rows of the screen none, one, two, three, or all four are lit.
*/
static const unsigned int lit_rows[8 * 4] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x88888888, 0x11111111, 0x22222222, 0x44444444,
    0xaaaaaaaa, 0x55555555, 0xaaaaaaaa, 0x55555555,
    0xeeeeeeee, 0xdddddddd, 0xbbbbbbbb, 0x77777777,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};
