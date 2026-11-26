# Writes ../../asc2-demos/film_pictures.h: the pictures and the tables of `film.c`.
# Run it from this directory: python3 film_gen.py [preview.png]
#
# With a name of a file it also writes a PNG with the pictures put together as the film does, to judge
# the drawing without the machine.
#
# A picture is what `scr_blit` takes: the width and the height in the first word, then the rows, the
# leftmost pixel of a word in its bit 0. A mark is a rectangle for the register of the RP2040: the column,
# the row from bit 7, the width less one from bit 13, the height less one from bit 21, 1 for lit in bit 27.
import math, os, random, struct, sys, zlib

W, H = 128, 64
FAR_H, FAR_Y = 36, 12     # the far range: its height and the row of its top
FAR_BODY = 44             # the rows that the far range hides: it goes on down behind the near hills
MID_H, MID_Y = 24, 34     # the near hills
GROUND_Y = 58             # the first row of the ground


def grid(width, height): return [[0] * width for _ in range(height)]


def far_tops():
    # The height of the far range over its bottom row at every column. The range repeats after 128 columns.
    random.seed(3)
    tops = []
    for x in range(W):
        u = math.pi * x / W
        r = 0.55 * abs(math.sin(3 * u + 0.4)) + 0.30 * abs(math.sin(7 * u + 1.3)) + 0.15 * abs(math.sin(13 * u + 2.1))
        tops.append(9 + round(25 * r ** 1.3))
    return tops


def far_range():
    tops = far_tops()
    mask, lit = grid(W, FAR_BODY), grid(W, FAR_H)
    for x in range(W):
        top = FAR_H - tops[x]
        for y in range(top, FAR_BODY): mask[y][x] = 1
        # The line of the ridge: from this top to the one of the next column
        other = FAR_H - tops[(x + 1) % W]
        for y in range(min(top, other), max(top, other) + 1 if top != other else top + 1):
            if y >= top: lit[y][x] = 1
        lit[top][x] = 1
        # Snow on what is high: lit from the ridge down, with a ragged lower edge
        snow = max(0, tops[x] - 19) * 0.62 + ((x * 5) % 3) - 1
        for y in range(top, min(FAR_H, top + round(snow))): lit[y][x] = 1
    return mask, lit, tops


def mid_tops():
    tops = []
    for x in range(W):
        u = 2 * math.pi * x / W
        h = 11 + 6 * math.sin(u + 1.0) + 4 * math.sin(2 * u + 2.5) + 1.5 * math.sin(5 * u + 0.7)
        tops.append(max(3, min(MID_H - 6, round(h))))
    return tops


def mid_hills():
    tops = mid_tops()
    mask, lit = grid(W, MID_H), grid(W, MID_H)
    for x in range(W):
        top = MID_H - tops[x]
        for y in range(top, MID_H):
            mask[y][x] = 1
            if y == top or (y > top + 1 and (x + 2 * y) % 4 == 0): lit[y][x] = 1
        other = MID_H - tops[(x + 1) % W]
        for y in range(top, max(top, other) + 1): lit[y][x] = 1
    # Spruces on the hills: narrow and tall
    for x in (9, 14, 20, 52, 58, 97, 103, 108, 116):
        foot = MID_H - tops[x]
        for k in range(7):
            half = (k + 1) // 3
            for dx in range(-half, half + 1):
                y = foot - 7 + k
                if 0 <= y < MID_H:
                    lit[y][(x + dx) % W] = 1; mask[y][(x + dx) % W] = 1
    return mask, lit, tops


def cloud():
    width, height = 30, 9
    g = grid(width, height)
    for cx, cy, rx, ry in ((8, 6, 8, 3), (15, 4, 7, 4), (22, 6, 7, 3)):
        for y in range(height):
            for x in range(width):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1: g[y][x] = 1
    return g


FONT = {
    'A': ('.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'),
    'L': ('#....', '#....', '#....', '#....', '#....', '#....', '#####'),
    'M': ('#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'),
    'U': ('#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'),
    'R': ('####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'),
    'T': ('#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'),
    'I': ('#####', '..#..', '..#..', '..#..', '..#..', '..#..', '#####'),
    ' ': ('.....',) * 7,
}


def name(text='ALMURUT II', sx=2, sy=3):
    width, height = len(text) * (5 * sx + 2), 7 * sy
    g = grid(width, height)
    for i, c in enumerate(text):
        for r, row in enumerate(FONT[c]):
            for k, bit in enumerate(row):
                if bit == '#':
                    for dy in range(sy):
                        for dx in range(sx): g[r * sy + dy][i * (5 * sx + 2) + k * sx + dx + 1] = 1
    return g


def picture(g):
    height, width = len(g), len(g[0])
    words = [width | height << 16]
    for row in g:
        for start in range(0, width, 32):
            value = 0
            for k, bit in enumerate(row[start:start + 32]): value |= bit << k
            words.append(value)
    return words


def rect(x, y, width, height, lit): return x | y << 7 | (width - 1) << 13 | (height - 1) << 21 | (1 if lit else 0) << 27


def disc(radius, lit=True, dx=0, dy=0):
    # The rows of a disc as marks; its box has its corner at the column 0 of the row 0
    marks = []
    for y in range(-radius, radius + 1):
        half = int(math.sqrt(radius * radius - y * y) + 0.5)
        marks.append(rect(radius - half + dx, y + radius + dy, 2 * half + 1, 1, lit))
    return marks


SUN_R = 6
RAY_BIAS = 24             # the points of the rays are kept with this added, so that none is negative
RAY_PHASES = 8


def rays():
    out = []
    for phase in range(RAY_PHASES):
        for k in range(8):
            a = 2 * math.pi * (k + phase / RAY_PHASES) / 8
            for r in (SUN_R + 4, SUN_R + (9 if k % 2 == 0 else 6)):
                x, y = round(math.cos(a) * r) + RAY_BIAS, round(math.sin(a) * r) + RAY_BIAS
                out.append(x | y << 16)
    return out


def stars(tops):
    random.seed(11)
    marks = []
    while len(marks) < 28:
        x, y = random.randrange(1, 126), random.randrange(0, 14)
        if y >= FAR_Y + FAR_H - max(tops) - 2: continue       # behind the range, wherever the film moves it
        if any(abs((m & 127) - x) < 5 and abs((m >> 7 & 63) - y) < 2 for m in marks): continue
        marks.append(rect(x, y, 2 if len(marks) % 5 == 0 else 1, 1, True))
    marks.sort(key=lambda m: -(m >> 7 & 63))                  # the lowest first: they go out first at dawn
    return marks


GLOW_ROWS = (45, 43, 41, 38, 35, 32, 28, 24, 19)


def tufts():
    random.seed(5)
    marks = []
    for k in range(10):
        x = (k * 13 + random.randrange(0, 6)) % W
        y = GROUND_Y + 1 + random.randrange(0, 5)
        marks.append(rect(x, y, 2 + k % 3, 1, True))
    return marks


def emit(out, kind, cname, words, per=8, note=None):
    if note: out.append('/* %s */' % note)
    out.append('static %s %s[%d] = {' % (kind, cname, len(words)))
    for i in range(0, len(words), per): out.append('    ' + ', '.join('0x%08x' % w for w in words[i:i + per]) + ',')
    out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')


far_mask, far_lit, far_top = far_range()
mid_mask, mid_lit, mid_top = mid_hills()
cloud_g, name_g = cloud(), name()
star_marks = stars(far_top)
moon = disc(6) + disc(6, False, 4, -2)[2:]

out = ['/*',
       '    The pictures and the tables of `film.c`.',
       '    `../asc2-kb/models/film_gen.py` has written this file; change the drawing there.',
       '*/', '',
       '#define FAR_Y      %d /* the row of the top of the far range */' % FAR_Y,
       '#define MID_Y      %d /* the row of the top of the near hills */' % MID_Y,
       '#define GROUND_Y   %d /* the first row of the ground */' % GROUND_Y,
       '#define SUN_SIDE   %d /* the side of the box of the sun */' % (2 * SUN_R + 1),
       '#define RAY_BIAS   %d /* what is added to the points of the rays, so that none is negative */' % RAY_BIAS,
       '#define RAY_PHASES %d /* the rays turn in this many steps and are then as at the start */' % RAY_PHASES,
       '#define RAY_WORDS  16 /* words of a step: eight rays of two points */',
       '#define STARS      %d' % len(star_marks),
       '#define GLOW_ROWS  %d' % len(GLOW_ROWS),
       '#define SUN_MARKS  %d' % len(disc(SUN_R)),
       '#define MOON_MARKS %d' % len(moon),
       '#define TUFTS      %d' % len(tufts()),
       '#define NAME_WIDTH %d' % len(name_g[0]),
       '#define NAME_HEIGHT %d' % len(name_g), '']
emit(out, 'unsigned int', 'far_mask', picture(far_mask), 8,
     'The far range, 128 columns that repeat: all that it hides, and then what of it is lit (the ridge and the snow)')
emit(out, 'unsigned int', 'far_lit', picture(far_lit))
emit(out, 'unsigned int', 'mid_mask', picture(mid_mask), 8, 'The near hills with their spruces, in the same two pictures')
emit(out, 'unsigned int', 'mid_lit', picture(mid_lit))
emit(out, 'unsigned int', 'cloud', picture(cloud_g), 8, 'A cloud')
emit(out, 'unsigned int', 'name', picture(name_g), 8, 'The name of the machine in large letters')
emit(out, 'const unsigned int', 'star_marks', star_marks, 8, 'The stars as marks, the lowest first: those go out first at dawn')
emit(out, 'const unsigned int', 'glow_marks', [rect(0, y, W, 1, True) for y in GLOW_ROWS], 8,
     'The rows of the sky that the dawn lights, from the horizon up')
emit(out, 'const unsigned int', 'sun_marks', disc(SUN_R), 8, 'The sun: the rows of a disc, from the corner of its box')
emit(out, 'const unsigned int', 'moon_marks', moon, 8, 'The moon: a lit disc and a dark one over a part of it')
emit(out, 'const unsigned int', 'ray_points', rays(), 8,
     'The rays of the sun in RAY_PHASES steps of a turn: for each ray its inner and its outer point, from the middle of the sun, with RAY_BIAS added')
emit(out, 'const unsigned int', 'tuft_marks', tufts(), 8, 'Tufts of grass on the ground')

here = os.path.dirname(os.path.abspath(__file__))
open(os.path.join(here, '..', '..', 'asc2-demos', 'film_pictures.h'), 'w').write('\n'.join(out))
print('film_pictures.h: %d words of pictures' % sum(len(picture(g)) for g in (far_mask, far_lit, mid_mask, mid_lit, cloud_g, name_g)))


# The preview: the scenes without the models, each scaled by 3
def blit(screen, g, x0, y0, mode):
    for y, row in enumerate(g):
        for x, bit in enumerate(row):
            if bit and 0 <= x0 + x < W and 0 <= y0 + y < H: screen[y0 + y][x0 + x] = mode if mode < 2 else 1 - screen[y0 + y][x0 + x]

def mark(screen, word, off_x=0, off_y=0):
    x, y, w, h, lit = (word & 127) + off_x, (word >> 7 & 63) + off_y, (word >> 13 & 127) + 1, (word >> 21 & 63) + 1, word >> 27 & 3
    for yy in range(y, y + h):
        for xx in range(x, min(W, x + w)):
            if 0 <= yy < H: screen[yy][xx] = lit

def land(screen, far_x, mid_x, with_mask=True):
    for dx in (0, W):
        if with_mask: blit(screen, far_mask, far_x + dx - W, FAR_Y, 0)
        blit(screen, far_lit, far_x + dx - W, FAR_Y, 1)
    for dx in (0, W):
        blit(screen, mid_mask, mid_x + dx - W, MID_Y, 0)
        blit(screen, mid_lit, mid_x + dx - W, MID_Y, 1)

def scene_dawn(sun_y, glow, stars_left):
    s = grid(W, H)
    for m in star_marks[len(star_marks) - stars_left:]: mark(s, m)
    for y in GLOW_ROWS[:glow]: mark(s, rect(0, y, W, 1, True))
    for m in disc(SUN_R): mark(s, m, 84, sun_y)
    land(s, 0, 0)
    return s

def scene_day(shift):
    s = grid(W, H)
    for m in disc(SUN_R): mark(s, m, 84, 1)
    blit(s, cloud_g, 20 - shift // 2, 1, 2); blit(s, cloud_g, 100 - shift // 2, 4, 2)
    land(s, (-shift // 4) % W, (-shift) % W, False)
    for m in tufts(): mark(s, m)
    return s

def scene_name():
    s = grid(W, H)
    blit(s, name_g, (W - len(name_g[0])) // 2, 6, 1)
    return s

if len(sys.argv) > 1:
    shots = [scene_dawn(40, 0, 28), scene_dawn(27, 5, 14), scene_dawn(1, 9, 0), scene_day(0), scene_day(40), scene_name()]
    scale, gap = 3, 6
    width, height = W * scale * 2 + gap * 3, (H * scale + gap) * 3 + gap
    image = [[60] * width for _ in range(height)]
    for i, s in enumerate(shots):
        x0, y0 = gap + (i % 2) * (W * scale + gap), gap + (i // 2) * (H * scale + gap)
        for y in range(H * scale):
            for x in range(W * scale): image[y0 + y][x0 + x] = 255 if s[y // scale][x // scale] == 1 else 0
    raw = b''.join(b'\0' + bytes(row) for row in image)
    def chunk(tag, data): return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    open(sys.argv[1], 'wb').write(png)
