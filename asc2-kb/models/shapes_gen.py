# Writes ../../asc2-demos/shapes_models.h: the models of `shapes.c` (points, edges, sides, and the
# directions that the sides face) and its two tables. Run it from this directory: python3 shapes_gen.py
#
# The axes: x to the right, y up, z towards the eye. The points of a side are listed so that they go
# around against the clock for an eye that looks at the side from outside; the check below proves it.
import math, os

def norm(v):
    l = math.sqrt(sum(c * c for c in v)); return tuple(c / l for c in v)
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def sub(a, b): return tuple(x - y for x, y in zip(a, b))
def dot(a, b): return sum(x * y for x, y in zip(a, b))

class Model:
    def __init__(self, name): self.name = name; self.points = []; self.sides = []   # side: (indices, both_ways, outline)
    def point(self, x, y, z): self.points.append((round(x), round(y), round(z))); return len(self.points) - 1
    def side(self, indices, outside, both_ways=False):
        # orders the points against the clock as seen from `outside` (a direction away from the model)
        p = [self.points[i] for i in indices]
        n = cross(sub(p[1], p[0]), sub(p[2], p[0]))
        assert dot(n, n) > 0, (self.name, indices)
        if dot(n, outside) < 0: indices = list(reversed(indices)); n = tuple(-c for c in n)
        for q in p[3:]: assert abs(dot(norm(n), sub(q, p[0]))) < 1.0, (self.name, indices, 'not flat')
        self.sides.append((list(indices), both_ways, norm(n)))
    def edges(self):
        seen = []
        for indices, _, _ in self.sides:
            for a, b in zip(indices, indices[1:] + indices[:1]):
                e = (min(a, b), max(a, b))
                if e not in seen: seen.append(e)
        return seen

def cube():
    m = Model('cube'); s = 11
    for z in (-s, s):
        for x, y in ((-s, -s), (s, -s), (s, s), (-s, s)): m.point(x, y, z)
    for indices, out in (((0, 1, 2, 3), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)), ((0, 1, 5, 4), (0, -1, 0)),
                         ((2, 3, 7, 6), (0, 1, 0)), ((1, 2, 6, 5), (1, 0, 0)), ((0, 3, 7, 4), (-1, 0, 0))):
        m.side(indices, out)
    return m

def yurt():
    m = Model('yurt'); r = 17; low = -11; top = 0; tip = 11
    ring = [(r * math.cos(k * math.pi / 4), r * math.sin(k * math.pi / 4)) for k in range(8)]
    lows = [m.point(x, low, z) for x, z in ring]
    tops = [m.point(x, top, z) for x, z in ring]
    apex = m.point(0, tip, 0); m.apex = apex
    for k in range(8):
        n = (k + 1) % 8
        out = (ring[k][0] + ring[n][0], 0, ring[k][1] + ring[n][1])
        m.side((lows[k], lows[n], tops[n], tops[k]), out)
        if k == 0:
            # The door: a dark side on the first wall, drawn right after the wall
            a, b = m.points[lows[k]], m.points[lows[n]]
            def at(t, y): return m.point(a[0] + (b[0] - a[0]) * t, y, a[2] + (b[2] - a[2]) * t)
            door = (at(0.3, low), at(0.7, low), at(0.7, top - 3), at(0.3, top - 3))
            m.side(door, out)
            m.door = len(m.sides) - 1
    for k in range(8):
        n = (k + 1) % 8
        out = (ring[k][0] + ring[n][0], 2 * r, ring[k][1] + ring[n][1])
        m.side((tops[k], tops[n], apex), out)
    return m

def burkut():
    # It flies towards +x. The wings are flat here; the program moves the points of their outer parts.
    m = Model('burkut')
    nose = m.point(16, 0, 0); tail = m.point(-10, 0, 0); up = m.point(1, 6, 0); down = m.point(1, -5, 0)
    left = m.point(1, 0, 5); right = m.point(1, 0, -5)
    for a, b in ((up, left), (left, down), (down, right), (right, up)):
        for end in (nose, tail):
            p = [m.points[i] for i in (a, b, end)]
            middle = tuple(sum(c) / 3 for c in zip(*p))
            m.side((a, b, end), sub(middle, (1, 0, 0)))
    m.moving = len(m.points)
    wings = []
    for sign in (1, -1):
        front = m.point(7, 1, 3 * sign); back = m.point(-5, 1, 3 * sign)
        elbow_front = m.point(5, 1, 15 * sign); elbow_back = m.point(-3, 1, 15 * sign); tip = m.point(0, 1, 29 * sign)
        wings.append((front, back, elbow_front, elbow_back, tip))
        m.side((front, elbow_front, elbow_back, back), (0, 1, 0), True)
        m.side((elbow_front, tip, elbow_back), (0, 1, 0), True)
    fan_left = m.point(-19, 1, 5); fan_right = m.point(-19, 1, -5)
    m.side((tail, fan_left, fan_right), (0, 1, 0), True)
    m.wings = wings
    return m

def word(v): return '0x%06x' % ((v[0] & 0xff) | (v[1] & 0xff) << 8 | (v[2] & 0xff) << 16)

def record(m, indices, both, normal, moves):
    # Three words: the direction that the side faces with how far its plane lies from the middle of the
    # model, then eight bytes: the number of its points and the points, the first one once more at the end.
    n = tuple(round(c * 127) for c in normal)
    far = 0 if moves else round(dot(normal, m.points[indices[0]]) * 127 / 32)
    assert -128 <= far <= 127, (m.name, indices)
    face = (n[0] & 0xff) | (n[1] & 0xff) << 8 | (n[2] & 0xff) << 16 | (far & 0xff) << 24
    row = [len(indices) + (128 if both else 0)] + indices + [indices[0]]
    row += [0] * (8 - len(row))
    return [face, row[0] | row[1] << 8 | row[2] << 16 | row[3] << 24, row[4] | row[5] << 8 | row[6] << 16 | row[7] << 24]

out = ['/*',
       '    The models of `shapes.c` and its tables. `../asc2-kb/models/shapes_gen.py` has written this file;',
       '    change the models there.',
       '',
       '    A point is a word of three signed bytes: x, y, and z, x in the low byte. x goes to the right, y up,',
       '    and z towards the eye. An edge is two bytes, its two points.',
       '',
       '    A side is three words. The first one is the direction that the side faces, three signed bytes with',
       '    the length 127, and in the fourth byte how far the plane of the side lies from the middle of the',
       '    model, in 127ths of 32. The other two words are eight bytes: the number of its points (3 or 4; 128',
       '    more if both of its faces can be seen), then the points, which go around against the clock for an',
       '    eye that looks at the side from outside, and the first point once more.',
       '*/', '',
       '#define SIDE_WORDS     3',
       '#define SIDE_BOTH_WAYS 128', '']
for m in (cube(), yurt(), burkut()):
    n = m.name
    assert max(math.sqrt(dot(p, p)) for p in m.points) < 30.5, n
    edges = m.edges()
    out.append('#define %s_POINTS %d' % (n.upper(), len(m.points)))
    out.append('#define %s_EDGES  %d' % (n.upper(), len(edges)))
    out.append('#define %s_SIDES  %d' % (n.upper(), len(m.sides)))
    if n == 'yurt': out.append('#define YURT_DOOR   %d /* the side that is the door: always dark */' % m.door)
    if n == 'yurt': out.append('#define YURT_APEX   %d /* the point at the top of the roof */' % m.apex)
    if n == 'burkut':
        out.append('#define BURKUT_WINGS  %d /* the first point of the wings: for each wing the two points at the body, the two of the elbow, and the tip */' % m.moving)
        out.append('#define BURKUT_WING_SIDES %d /* the first side of the wings: for each wing the inner part and the outer part */' % 8)
    out.append('')
    out.append('static unsigned int %s_points[%s_POINTS] = {' % (n, n.upper()))
    for i in range(0, len(m.points), 6): out.append('    ' + ', '.join(word(p) for p in m.points[i:i + 6]) + ',')
    out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
    out.append('static const unsigned char %s_edges[2 * %s_EDGES] = {' % (n, n.upper()))
    for i in range(0, len(edges), 10): out.append('    ' + ', '.join('%d, %d' % e for e in edges[i:i + 10]) + ',')
    out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
    out.append('static unsigned int %s_sides[SIDE_WORDS * %s_SIDES] = {' % (n, n.upper()))
    for k, (indices, both, normal) in enumerate(m.sides):
        moves = n == 'burkut' and 8 <= k < 12 # the program turns the parts of the wings; their planes pass close to the middle
        out.append('    ' + ', '.join('0x%08x' % w for w in record(m, indices, both, normal, moves)) + ',')
    out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')

DISTANCE = 90
out.append('/* What a size at a depth is multiplied by, in 64ths: %d / (%d - z) for z from -32 to 31. A point that is nearer looks larger. */' % (DISTANCE, DISTANCE))
zoom = [round(DISTANCE * 64 / (DISTANCE - z)) for z in range(-32, 32)]
out.append('static const short zoom[64] = {')
for i in range(0, 64, 16): out.append('    ' + ', '.join(str(v) for v in zoom[i:i + 16]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
out.append('/* 65536 divided by a number of rows from 1 to 63: the CPU has no instruction that divides */')
out.append('static const int parts[64] = {')
parts = [0] + [65536 // d for d in range(1, 64)]
for i in range(0, 64, 12): out.append('    ' + ', '.join(str(v) for v in parts[i:i + 12]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
out.append('/* The sine of an angle in 64ths; a circle is 256 */')
sines = [round(64 * math.sin(k * math.pi / 128)) for k in range(256)]
out.append('static const signed char sines[256] = {')
for i in range(0, 256, 16): out.append('    ' + ', '.join(str(v) for v in sines[i:i + 16]) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')

# A beat of the wings of the burkut in 32 steps. The inner part of a wing swings around the line where it
# joins the body, and the outer part follows a little later. A flat wing faces up; a wing at an angle
# faces up and to the body.
def sine(angle): return sines[angle & 255]
bird = burkut()
out.append('/*')
out.append('    The beat of the wings of the burkut in %d steps, ten words each: for the left wing and then for the' % 32)
out.append('    right one the two points of the elbow and the tip, and then for each wing the directions that')
out.append('    its inner part and its outer part face.')
out.append('*/')
out.append('#define BEATS      32')
out.append('#define BEAT_WORDS 10')
out.append('')
out.append('static const unsigned int wing_beats[BEATS * BEAT_WORDS] = {')
for step in range(32):
    beat = step * 8
    inner = sine(beat) * 18 >> 6
    outer = inner + (sine(beat - 40) * 14 >> 6)
    elbow_y = 1 + (12 * sine(inner) >> 6)
    elbow_z = 3 + (12 * sine(inner + 64) >> 6)
    tip_y = elbow_y + (14 * sine(outer) >> 6)
    tip_z = elbow_z + (14 * sine(outer + 64) >> 6)
    points = []; faces = []
    for wing, sign in enumerate((1, -1)):
        front, back, elbow_front, elbow_back, tip = bird.wings[wing]
        points.append((bird.points[elbow_front][0], elbow_y, sign * elbow_z))
        points.append((bird.points[elbow_back][0], elbow_y, sign * elbow_z))
        points.append((bird.points[tip][0], tip_y, sign * tip_z))
        faces.append((0, 127 * sine(inner + 64) >> 6, -sign * (127 * sine(inner) >> 6)))
        faces.append((0, 127 * sine(outer + 64) >> 6, -sign * (127 * sine(outer) >> 6)))
    out.append('    ' + ', '.join(word(v) for v in points + faces) + ',')
out[-1] = out[-1].rstrip(','); out.append('};'); out.append('')
out.append('/*')
out.append('    The rows of a side that are lit, for each way of drawing a side (see `fill_sides`) and for each of')
out.append('    the four rows that the first row of the side can be: the bit 31 is the first row, the bit 30 the')
out.append('    next one, and so on. Of every four rows of the screen none, one, two, three, or all four are lit.')
out.append('*/')
out.append('static const unsigned int lit_rows[8 * 4] = {')
shades = [0x0, 0x0, 0x1, 0x5, 0x7, 0xf, 0xf, 0x0] # the bit 0 is for the rows 0, 4, 8, and so on
for shade in shades:
    words = []
    for first in range(4):
        w = 0
        for k in range(32):
            if shade >> ((first + k) & 3) & 1: w |= 1 << (31 - k)
        words.append('0x%08x' % w)
    out.append('    ' + ', '.join(words) + ',')
out[-1] = out[-1].rstrip(','); out.append('};')
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'asc2-demos', 'shapes_models.h')
open(path, 'w').write('\n'.join(out) + '\n')
print('written', os.path.normpath(path))
