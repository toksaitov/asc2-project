/*
    Structures: their layout, copies, and their way through a call.

    A field starts at an address that is a multiple of its size, so a
    structure may have unused bytes between its fields and at its end.

    A structure passed to a function is copied onto the stack in place of an
    argument. A function that returns a structure gets the address of the
    place for the result as a hidden first argument: the caller makes the
    room, and the function fills it.
*/

#include "out.h"

typedef struct point
{
    int x;
    int y;
} point_t;

typedef struct mixed
{
    char letter;
    int number;
    short half;
    char tail;
    long long wide;
    char last;
} mixed_t;

typedef struct segment
{
    point_t start;
    point_t end;
    const char *name;
} segment_t;

typedef struct item
{
    signed char kind;
    unsigned short weight;
    int values[3];
} item_t;

/* A structure of a size that is not a whole number of words */
typedef struct tag
{
    char text[7];
} tag_t;

/* A structure of many words */
typedef struct page
{
    int numbers[300];
} page_t;

typedef struct shape
{
    const char *name;
    int (*area)(const struct shape *shape);
    int width;
    int height;
} shape_t;

static int rect_area(const shape_t *shape)
{
    return shape->width * shape->height;
}

static int triangle_area(const shape_t *shape)
{
    return shape->width * shape->height / 2;
}

/* A table of structures with addresses of strings and of functions. */
static shape_t SHAPES[3] = {
    { "rectangle", rect_area, 6, 7 },
    { "triangle", triangle_area, 6, 7 },
    { "square", rect_area, 5, 5 }
};

static item_t g_item = { -2, 60000, { 7, 8, 9 } };

static point_t make_point(int x, int y)
{
    point_t point = { x, y };

    return point;
}

static point_t add_points(point_t a, point_t b)
{
    a.x += b.x;
    a.y += b.y;

    return a;
}

static int cross(point_t a, int scale, point_t b)
{
    return (a.x * b.y - a.y * b.x) * scale;
}

static mixed_t make_mixed(int seed)
{
    mixed_t mixed;

    mixed.letter = (char) ('a' + seed);
    mixed.number = seed * 1000;
    mixed.half = (short) -seed;
    mixed.tail = (char) seed;
    mixed.wide = (long long) seed << 40;
    mixed.last = '!';

    return mixed;
}

static long long sum_mixed(mixed_t mixed)
{
    /* A change of the copy must not reach the caller's structure. */
    mixed.number += 1;

    return mixed.letter + mixed.number + mixed.half + mixed.tail + mixed.wide + mixed.last;
}

static int sum_tag(int before, tag_t tag, int after)
{
    int sum = before * 1000000 + after * 1000;

    for (int i = 0; i < 7; ++i) {
        sum += tag.text[i];
    }

    return sum;
}

static int sum_page(int before, page_t page, int after)
{
    page.numbers[1] += 1;

    return before + page.numbers[0] + page.numbers[1] + page.numbers[150] + page.numbers[299] + after;
}

static page_t g_page;

static void move_segment(segment_t *segment, point_t by)
{
    segment->start = add_points(segment->start, by);
    segment->end = add_points(segment->end, by);
}

static void test_layout(void)
{
    mixed_t mixed;
    char *base = (char *) &mixed;

    show("sizeof point_t", sizeof(point_t));
    show("sizeof mixed_t", sizeof(mixed_t));
    show("sizeof item_t", sizeof(item_t));
    show("offset of number", (char *) &mixed.number - base);
    show("offset of half", (char *) &mixed.half - base);
    show("offset of tail", (char *) &mixed.tail - base);
    show("offset of wide", (char *) &mixed.wide - base);
    show("offset of last", (char *) &mixed.last - base);
}

static void test_copies(void)
{
    point_t a = { 3, 4 };
    point_t b = a;

    b.x = 10;
    show("a.x", a.x);
    show("b.x", b.x);
    show("b.y", b.y);

    mixed_t first = make_mixed(5);
    mixed_t second;
    second = first;
    second.number = -1;
    show("first.letter", first.letter);
    show("first.number", first.number);
    show("first.half", first.half);
    show64("first.wide", first.wide);
    show("second.number", second.number);
    show("second.last", second.last);

    item_t items[3];
    for (int i = 0; i < 3; ++i) {
        items[i] = g_item;
        items[i].kind = (signed char) (items[i].kind * (i + 1));
        items[i].values[i] += 100;
    }
    show("items[2].kind", items[2].kind);
    show("items[1].weight", items[1].weight);
    show("items[1].values[1]", items[1].values[1]);
    show("items[1].values[2]", items[1].values[2]);
    show("g_item.values[0]", g_item.values[0]);

    item_t *cursor = &items[1];
    ++cursor;
    show("cursor->values[2]", cursor->values[2]);
    show("cursor - items", cursor - items);
}

static void test_calls(void)
{
    point_t a = make_point(2, 3);
    point_t b = make_point(-5, 7);
    point_t c = add_points(a, b);

    show("c.x", c.x);
    show("c.y", c.y);
    show("a.x", a.x);
    show("cross", cross(a, 10, b));
    show("add_points(...).y", add_points(c, make_point(1, 1)).y);
    show("cross of results", cross(add_points(a, b), 2, make_point(4, 5)));

    mixed_t mixed = make_mixed(3);
    show64("sum_mixed", sum_mixed(mixed));
    show("mixed.number", mixed.number);
    show64("sum_mixed(make_mixed(9))", sum_mixed(make_mixed(9)));

    segment_t segment = { { 0, 0 }, { 10, 20 }, "diagonal" };
    move_segment(&segment, make_point(5, -5));
    put_str(segment.name);
    put_char('\n');
    show("segment.start.x", segment.start.x);
    show("segment.start.y", segment.start.y);
    show("segment.end.x", segment.end.x);
    show("segment.end.y", segment.end.y);

    tag_t tag = { "abcdef" };
    show("sizeof tag_t", sizeof(tag_t));
    show("sum_tag", sum_tag(1, tag, 2));
    show("sum_tag", sum_tag(sum_tag(0, tag, 0), tag, sum_tag(0, tag, 1) - 600));

    g_page.numbers[0] = 1;
    g_page.numbers[1] = 20;
    g_page.numbers[150] = 300;
    g_page.numbers[299] = 4000;
    show("sum_page", sum_page(50000, g_page, 600000));
    show("g_page.numbers[1]", g_page.numbers[1]);

    for (int i = 0; i < 3; ++i) {
        put_str(SHAPES[i].name);
        put_char(' ');
        put_dec(SHAPES[i].area(&SHAPES[i]));
        put_char('\n');
    }
}

int main(void)
{
    test_layout();
    test_copies();
    test_calls();

    return 0;
}
