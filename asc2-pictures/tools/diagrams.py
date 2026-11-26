#!/usr/bin/env python3
"""
Draws the diagrams of the Readme as SVG files, each in two versions: one for
GitHub's light theme and one for its dark theme.

    diagrams.py output_directory

Every diagram is 680 units wide with no text smaller than 20 units (16 in the
screen of the emulator), so that it still reads on a phone. The width of every
text is fixed from the widths of Arial's letters below, so the text takes the
same room with any font, and the script stops if a text crosses the edge of a
box, a line, or the drawing.

The numbers of the charts are the times that the board measured
(../../asc2-kb/array-design.md, "The Time in Clocks and on the Board") and the
cells that the build of the chip counted, and the screen of the emulator is the
text of ../emulator.txt.
"""

import sys
from pathlib import Path

SANS = "Arial, Helvetica, sans-serif"
MONO = "Menlo, Consolas, 'Liberation Mono', 'Courier New', monospace"
MONO_ADVANCE = 0.6

# The widths of Arial's letters, in thousandths of the font size.
LETTERS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00d7\u00b7\u00b5\u2192\u2013\u00b0"
REGULAR = dict(zip(LETTERS, (
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833,
    722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556, 333, 556, 556, 500, 556,
    556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334,
    260, 334, 584, 584, 333, 576, 1000, 556, 400,
)))
BOLD = dict(zip(LETTERS, (
    278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833,
    722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556, 333, 556, 611, 556, 611,
    556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389,
    280, 389, 584, 584, 333, 576, 1000, 556, 400,
)))

THEMES = {
    "light": {
        "page": "#ffffff", "ink": "#1f2328", "muted": "#59636e",
        "blue": "#0969da", "red": "#b31b1b", "on_color": "#ffffff",
        "bar": "#8c959f", "red_soft": "#e07070",
    },
    "dark": {
        "page": "#0d1117", "ink": "#f0f6fc", "muted": "#9198a1",
        "blue": "#1f6feb", "red": "#c62828", "on_color": "#ffffff",
        "bar": "#6e7681", "red_soft": "#e07070",
    },
}

# The text on a filled box: white on a color, and the color of the page on a box filled with ink
ON_FILL = {"blue": "on_color", "red": "on_color", "ink": "page"}


def measure(string, size, bold=False, mono=False):
    """The width of a text in the units of the drawing."""
    if mono:
        return len(string) * size * MONO_ADVANCE
    table = BOLD if bold else REGULAR
    return sum(table[letter] for letter in string) * size / 1000


class Drawing:
    def __init__(self, width, height, theme):
        self.width, self.height = width, height
        self.colors = THEMES[theme]
        self.parts = []
        self.boxes, self.texts, self.lines = [], [], []

    def color(self, name):
        return self.colors.get(name, name)

    def rect(self, x, y, w, h, fill="page", line="ink", width=3, dash=None):
        dashes = f' stroke-dasharray="{dash}"' if dash else ""
        stroke = f' stroke="{self.color(line)}" stroke-width="{width}"{dashes}' if line else ""
        self.parts.append(f'<rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" fill="{self.color(fill)}"{stroke}/>')
        if line:
            self.boxes.append((x, y, x + w, y + h))

    def text(self, x, y, string, size=22, anchor="middle", bold=False, color="ink", mono=False):
        width = measure(string, size, bold, mono)
        left = {"start": x, "middle": x - width / 2, "end": x - width}[anchor]
        self.texts.append((left, y - size * 0.76, left + width, y + size * 0.22, string))
        weight = ' font-weight="bold"' if bold else ""
        family = f' font-family="{MONO}"' if mono else ""
        escaped = string.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        self.parts.append(
            f'<text x="{left:.1f}" y="{y:g}" font-size="{size}"{weight}{family} textLength="{width:.1f}" '
            f'lengthAdjust="spacingAndGlyphs" fill="{self.color(color)}">{escaped}</text>'
        )

    def line(self, x1, y1, x2, y2, end=True, start=False, width=2.5, color="ink", dash=None):
        markers = (' marker-end="url(#head)"' if end else "") + (' marker-start="url(#tail)"' if start else "")
        dashes = f' stroke-dasharray="{dash}"' if dash else ""
        self.parts.append(
            f'<line x1="{x1:g}" y1="{y1:g}" x2="{x2:g}" y2="{y2:g}" stroke="{self.color(color)}" '
            f'stroke-width="{width}"{dashes}{markers}/>'
        )
        self.lines.append((min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)))

    def cell(self, x, y, w, h, title, note=None, fill="page", size=22, note_size=20, bold=True, mono=False,
             color=None, line="ink", dash=None):
        """A box with a title and, under it, a note; both centered."""
        color = color or ON_FILL.get(fill, "ink")
        self.rect(x, y, w, h, fill, line, 3 if not dash else 2.5, dash)
        if note is None:
            self.text(x + w / 2, y + h / 2 + size * 0.35, title, size, bold=bold, mono=mono, color=color)
            return
        gap = 5
        top = y + (h - size - gap - note_size) / 2
        self.text(x + w / 2, top + size * 0.8, title, size, bold=bold, mono=mono, color=color)
        self.text(x + w / 2, top + size + gap + note_size * 0.8, note, note_size, color=color)

    def check(self, name):
        """Stops if a text leaves the drawing or crosses the edge of a box or a line."""
        for left, top, right, bottom, string in self.texts:
            where = f"{name}: the text {string!r}"
            if left < 6 or top < 2 or right > self.width - 6 or bottom > self.height - 2:
                sys.exit(f"{where} leaves the drawing")
            for x1, y1, x2, y2 in self.boxes:
                apart = right <= x1 or left >= x2 or bottom <= y1 or top >= y2
                inside = left >= x1 + 7 and right <= x2 - 7 and top >= y1 + 3 and bottom <= y2 - 3
                if not apart and not inside:
                    sys.exit(f"{where} crosses the box at {x1:g}, {y1:g}")
            for x1, y1, x2, y2 in self.lines:
                if not (right <= x1 - 3 or left >= x2 + 3 or bottom <= y1 - 3 or top >= y2 + 3):
                    sys.exit(f"{where} crosses the line at {x1:g}, {y1:g}")

    def svg(self):
        ink = self.color("ink")
        head = (
            f'<marker id="head" viewBox="0 0 12 12" refX="11" refY="6" markerWidth="12" markerHeight="12" '
            f'markerUnits="userSpaceOnUse" orient="auto"><path d="M0 0 L12 6 L0 12 z" fill="{ink}"/></marker>'
            f'<marker id="tail" viewBox="0 0 12 12" refX="1" refY="6" markerWidth="12" markerHeight="12" '
            f'markerUnits="userSpaceOnUse" orient="auto"><path d="M12 0 L0 6 L12 12 z" fill="{ink}"/></marker>'
        )
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.width:g} {self.height:g}" '
            f'width="{self.width:g}" height="{self.height:g}" font-family="{SANS}">\n'
            f"<defs>{head}</defs>\n"
            f'<rect width="{self.width:g}" height="{self.height:g}" fill="{self.color("page")}"/>\n'
            + "\n".join(self.parts)
            + "\n</svg>\n"
        )


def system(theme):
    """The chip, the bridge beside it, and what goes between them."""
    d = Drawing(680, 380, theme)
    top, row, step = 68, 62, 72

    parts = (("CPU", "own instruction set", "blue"), ("AI accelerator", "12 weights, 12 inputs", "red"),
             ("Instruction cache", "32 words", "page"), ("Timer, keyboard", "debug port", "page"))
    wide = max(max(measure(title, 22, True), measure(note, 20)) for title, note, _ in parts) + 28
    d.rect(20, 20, wide + 30, 340)
    d.text(35, 53, "Almurut II chip", 24, "start", bold=True)
    for i, (title, note, fill) in enumerate(parts):
        d.cell(35, top + i * step, wide, row, title, note, fill)
    chip_right = 20 + wide + 30

    devices = (("RAM", "ROM"), ("Disk", "Console"), ("Screen", "Button"), ("Clock", None))
    narrow = 112
    left = 660 - 2 * narrow - 10 - 30
    d.rect(left, 20, 660 - left, 340)
    d.text(left + 15, 53, "Bridge", 24, "start", bold=True)
    d.text(645, 53, "microcontroller", 20, "end", color="muted")
    for i, pair in enumerate(devices):
        for k, name in enumerate(pair):
            x, y = left + 15 + k * (narrow + 10), top + i * step
            if name:
                d.cell(x, y, narrow, row, name)
            else:
                d.cell(x, y, narrow, row, "yours", None, "page", 20, bold=False, color="muted", line="muted", dash="8 6")

    middle = (chip_right + left) / 2
    d.text(middle, 150, "bus", 22, bold=True)
    d.line(chip_right + 4, 164, left - 4, 164, start=True)
    d.text(middle, 194, "10 wires", 20, color="muted")
    d.text(middle, 262, "clock, reset", 20, color="muted")
    d.line(left - 4, 276, chip_right + 4, 276)
    return d


def accelerator(theme):
    """The rows of weights and inputs, and the four cells they pass through."""
    d = Drawing(680, 336, theme)
    first, wide, gap, high = 150, 72, 20, 26
    columns = [first + k * (wide + gap) for k in range(4)]
    right = columns[-1] + wide

    def rows(top, fill):
        for i in range(3):
            for x in columns:
                d.rect(x, top + i * high, wide, high, fill)

    rows(24, "blue")
    d.text(20, 60, "Weights", 22, "start", bold=True)
    d.text(20, 86, "3 rows of 4", 20, "start", color="muted")

    for k, x in enumerate(columns):
        d.line(x + wide / 2, 102, x + wide / 2, 137)
        d.line(x + wide / 2, 230, x + wide / 2, 197)
        d.cell(x, 140, wide, 54, "w \u00d7 x", None, "red")
        if k:
            d.line(x - gap, 167, x - 3, 167)
    d.text(20, 175, "Cells", 22, "start", bold=True)
    d.line(right, 167, 527, 167)
    d.cell(530, 140, 90, 54, "sum", None, "ink")

    rows(230, "page")
    d.text(20, 266, "Inputs", 22, "start", bold=True)
    d.text(20, 292, "3 rows of 4", 20, "start", color="muted")

    d.line(560, 63, right + 4, 63)
    d.text(568, 70, "learn", 22, "start", bold=True, mono=True)
    d.text(512, 100, "a step of training", 20, "start", color="muted")
    d.line(560, 269, right + 4, 269)
    d.text(568, 276, "xsh", 22, "start", bold=True, mono=True)
    d.text(512, 306, "a new byte", 20, "start", color="muted")
    return d


def instruction(theme):
    """The one format of every instruction, with two examples."""
    d = Drawing(680, 236, theme)
    left = 150
    fields = (("immediate", "16 bits", 250, "page", "16"), ("A", "4", 60, "page", "12"),
              ("B", "4", 60, "page", "8"), ("opcode", "8 bits", 150, "blue", "0"))
    examples = (("add r1 r2", ("0x0000", "1", "2", "0x20")), ("add 0x4 r2", ("0x0004", "0", "2", "0xA0")))

    d.text(left + 4, 30, "31", 20, "start", color="muted", mono=True)
    x = left
    for name, size, width, fill, last in fields:
        d.cell(x, 40, width, 64, name, size, fill)
        d.text(x + width - 4, 30, last, 20, "end", color="muted", mono=True)
        x += width
    d.text(left - 14, 80, "one word", 22, "end", bold=True)

    for row, (source, values) in enumerate(examples):
        y = 126 + row * 50
        d.text(left - 14, y + 27, source, 20, "end", mono=True)
        x = left
        for (_, _, width, _, _), value in zip(fields, values):
            d.cell(x, y, width, 40, value, None, "page", 20, bold=False, mono=True)
            x += width
    return d


def gain(theme):
    """How much faster six small jobs run, as the board measured them."""
    d = Drawing(680, 420, theme)
    jobs = (
        ("Learning step", 3.4, 38.8),
        ("One-bit neuron", 7.2, 10.0),
        ("Small network", 2.0, 9.9),
        ("Filter output", 6.2, 9.3),
        ("Layer neuron", 4.5, 4.6),
        ("Vertex transform", 3.9, 3.8),
    )
    zero, unit, base = 200, 9.0, 374

    # Both bars are red, the color of the accelerator: the cache alone in a soft shade, the accelerator in the full one
    d.text(20, 30, "Times faster than the plain chip. Longer is better.", 22, "start", bold=True)
    d.rect(20, 50, 18, 18, "red_soft", None)
    d.text(48, 66, "with the cache", 20, "start")
    d.rect(214, 50, 18, 18, "red", None)
    d.text(242, 66, "with the cache and the AI accelerator", 20, "start")

    for i, (name, with_cache, with_both) in enumerate(jobs):
        y = 90 + i * 46
        d.text(zero - 14, y + 27, name, 22, "end")
        d.rect(zero, y, with_cache * unit, 12, "red_soft", None)
        d.rect(zero, y + 14, with_both * unit, 24, "red", None)
        d.text(zero + with_both * unit + 10, y + 34, f"{with_both:.1f}\u00d7", 22, "start", bold=True)

    d.line(zero, 82, zero, base, end=False, width=2, color="muted")
    d.line(zero, base, zero + 42 * unit, base, end=False, width=2, color="muted")
    for mark in (10, 20, 30, 40):
        x = zero + mark * unit
        d.line(x, base, x, base + 8, end=False, width=2, color="muted")
        d.text(x, base + 34, f"{mark}\u00d7", 20, color="muted")
    return d


def cells(theme):
    """What the chip is made of: the cells of the layout, as its build counted them."""
    d = Drawing(680, 356, theme)
    kinds = (
        ("Logic gates", 7200), ("Flip-flops", 2791), ("Delay cells", 1761), ("Clock tree", 1366),
        ("Multiplexers", 1204), ("Buffers", 1037), ("Diodes", 700),
    )
    zero, unit = 160, 0.058
    for i, (name, count) in enumerate(kinds):
        y = 20 + i * 46
        d.text(zero - 14, y + 26, name, 22, "end")
        d.rect(zero, y + 4, count * unit, 30, "bar", None)
        d.text(zero + count * unit + 10, y + 26, f"{count:,}", 22, "start", bold=True)
    d.line(zero, 14, zero, 20 + len(kinds) * 46, end=False, width=2, color="muted")
    return d


def platform(theme):
    """What runs on what: the programs, the kernel, and the four machines."""
    d = Drawing(680, 316, theme)
    d.cell(20, 20, 640, 62, "Programs", "the shell, the utilities, the showcase, your own")
    d.cell(20, 92, 640, 62, "Rach2 kernel", "processes, virtual memory, files")
    d.cell(20, 164, 640, 62, "Almurut II instruction set", "one definition for every tool and machine", "blue")
    machines = (("Emulator", "in C"), ("Simulation", "of the gates"), ("FPGA board", "DE10-Lite"), ("Chip", "TTSKY26e"))
    for i, (name, note) in enumerate(machines):
        d.cell(20 + i * 163, 236, 151, 62, name, note)
    return d


def bus(theme):
    """The bytes of two requests on the bus."""
    d = Drawing(680, 204, theme)
    first, high, one, four = 112, 52, 84, 168
    answer = first + one + four + 40

    d.text(first + (one + four) / 2, 30, "the chip sends", 20, bold=True)
    d.text(answer + (one + four) / 2, 30, "the bridge answers", 20, bold=True)
    for y, name, size, address in ((44, "Read", "10 bytes", True), (132, "Fetch", "6 bytes", False)):
        d.text(first - 14, y + 22, name, 22, "end", bold=True)
        d.text(first - 14, y + 46, size, 20, "end", color="muted")
        d.cell(first, y, one, high, "header", None, "blue", 20, bold=False)
        if address:
            d.cell(first + one, y, four, high, "address, 4 bytes", None, "blue", 20, bold=False)
        else:
            d.cell(first + one + 8, y, four - 8, high, "no address", None, "page", 20, bold=False, color="muted",
                   line="muted", dash="8 6")
        d.line(first + one + four + 4, y + high / 2, answer - 4, y + high / 2)
        d.cell(answer, y, one, high, "status", None, "page", 20, bold=False)
        d.cell(answer + one, y, four, high, "data, 4 bytes", None, "page", 20, bold=False)
    return d


def emulator(theme):
    """The top left part of the emulator's screen, from the text in emulator.txt."""
    rows, columns, size, leading, pad = 13, 67, 16, 22, 16
    advance = size * MONO_ADVANCE
    lines = (Path(__file__).resolve().parent.parent / "emulator.txt").read_text().split("\n")[:rows]
    d = Drawing(columns * advance + 2 * pad, rows * leading + 2 * pad, theme)
    d.rect(1.5, 1.5, d.width - 3, d.height - 3, "page", "ink", 2)
    for row, line in enumerate(lines):
        line = line[:columns]
        column = 0
        while column < len(line):
            if line[column] == " ":
                column += 1
                continue
            end = line.find("  ", column)
            end = len(line) if end < 0 else end
            d.text(pad + column * advance, pad + row * leading + 16, line[column:end], size, "start", mono=True)
            column = end
    return d


DIAGRAMS = {
    "system": system, "accelerator": accelerator, "instruction": instruction, "gain": gain,
    "cells": cells, "platform": platform, "bus": bus, "emulator": emulator,
}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    for name, draw in DIAGRAMS.items():
        for theme in THEMES:
            drawing = draw(theme)
            drawing.check(name)
            path = directory / f"{name}-{theme}.svg"
            path.write_text(drawing.svg())
            print(path)


if __name__ == "__main__":
    main()
