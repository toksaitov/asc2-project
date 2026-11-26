# Writes ../../asc2-demos/flight_land.h: the land of `flight.c`, the tables of its view, and its sky.
# Run it from this directory: python3 flight_gen.py
#
# The land is a valley between mountains, 64 columns wide and 32 rows long; the rows repeat, so the flight
# has no end. A height is in 32nds of the side of a square of the land. The valley bends to the left and to
# the right once in its length, and the eye follows it.
import math, os, random

COLS, ROWS = 64, 32
NEAR = 40          # the side of a square of the land in pixels, one square away from the eye
STEPS = 16         # a distance is counted in 16ths of a square
FAR = 8            # rows of the land that are drawn

def smooth(t): t = max(0.0, min(1.0, t)); return t * t * (3 - 2 * t)

def height(c, z):
    turn = 2 * math.pi * z / ROWS
    middle = COLS / 2 + 2 * math.sin(turn)
    d = abs(c - middle)
    floor = 3 + 1.5 * math.sin(2 * turn + c * 0.8) + 1.5 * math.sin(3 * turn + c * 1.7 + 1)
    ridge = 0.6 * abs(math.sin(c * 0.45 + 2 * turn)) + 0.4 * abs(math.sin(c * 0.21 + turn + 2))
    wall = smooth((d - 1.5) / 4.5)
    peak = (45 + 80 * ridge) * (1 - 0.4 * smooth((d - 11) / 9))
    return max(0, min(125, round(floor * (1 - wall) + wall * peak)))

out = ['/*',
       '    The land of `flight.c`, the tables of its view, and its sky.',
       '    `../asc2-kb/models/flight_gen.py` has written this file; change the land there.',
       '*/', '',
       '#define LAND_COLS  %d' % COLS,
       '#define LAND_ROWS  %d' % ROWS,
       '#define FAR_ROWS   %d /* rows of the land that are drawn */' % FAR,
       '#define STEPS      %d /* a distance is counted in %dths of a square of the land */' % (STEPS, STEPS), '',
       '/* The heights of the land, a row after the other, in 32nds of the side of a square: a valley between mountains. The rows repeat. */',
       'static const unsigned char land[LAND_ROWS * LAND_COLS] = {']
for z in range(ROWS):
    out.append('    ' + ', '.join('%d' % height(c, z) for c in range(COLS)) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')

last = FAR * STEPS
out.append('/* The side of a square of the land in 256ths of a pixel at every distance: %d pixels at one square away, and less as the distance grows */' % NEAR)
scales = [0] + [round(NEAR * STEPS * 256 / d) for d in range(1, last + 1)]
out.append('static const int scales[FAR_ROWS * STEPS + 1] = {')
for i in range(0, last + 1, 12): out.append('    ' + ', '.join(str(v) for v in scales[i:i + 12]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
out.append('/* How many squares of the land are seen to each side of the middle at every distance, and one more */')
wide = [0] + [math.ceil(64 * d / (NEAR * STEPS)) + 1 for d in range(1, last + 1)]
assert max(wide) + 4 < COLS // 2
out.append('static const unsigned char wide[FAR_ROWS * STEPS + 1] = {')
for i in range(0, last + 1, 24): out.append('    ' + ', '.join(str(v) for v in wide[i:i + 24]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')

# The sky: stars, and a moon that is a lit disc with a dark disc over a part of it. Each mark is a
# rectangle of the RP2040: the column, the row from bit 7, the width less one from bit 13, 1 for lit in bit 27.
def rect(x, y, width, lit): return x | y << 7 | (width - 1) << 13 | (1 if lit else 0) << 27
random.seed(7)
marks = []
moon_x, moon_y = 104, 8
while len(marks) < 22:
    x, y = random.randrange(2, 126), random.randrange(1, 30)
    if abs(x - moon_x) < 12 and abs(y - moon_y) < 10: continue
    marks.append(rect(x, y, 1, True))
for cx, cy, radius, lit in ((moon_x, moon_y, 5, True), (moon_x + 3, moon_y - 1, 4, False)):
    for dy in range(-radius, radius + 1):
        half = int(math.sqrt(radius * radius - dy * dy) + 0.5)
        marks.append(rect(cx - half, cy + dy, 2 * half + 1, lit))
out.append('/* The sky: %d stars and the moon, each a rectangle for the RP2040 */' % 22)
out.append('#define SKY_MARKS %d' % len(marks))
out.append('')
out.append('static const unsigned int sky[SKY_MARKS] = {')
for i in range(0, len(marks), 6): out.append('    ' + ', '.join('0x%08x' % v for v in marks[i:i + 6]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
out.append('/* The sine of an angle in 64ths; a circle is 256 */')
sines = [round(64 * math.sin(k * math.pi / 128)) for k in range(256)]
out.append('static const signed char sines[256] = {')
for i in range(0, 256, 16): out.append('    ' + ', '.join(str(v) for v in sines[i:i + 16]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};')
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'asc2-demos', 'flight_land.h')
open(path, 'w').write('\n'.join(out) + '\n')
print('written', os.path.normpath(path))
