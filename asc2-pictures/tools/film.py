#!/usr/bin/env python3
"""
Turns the frames of a run on the emulator into an animated GIF or a PNG.

The trace tool writes every frame that reaches the screen of the machine to a
file with `-m` (see ../../asc2-tools/Readme.md): a line holds the number of
instructions finished, the clock of the bridge in microseconds, and the 1,024
bytes of the frame in hex, 64 rows of 16 bytes, the low bit of a byte on the
left.

    film.py gif  film_file out.gif from_s to_s [-r rate] [-s scale] [-e end_hold_s]
    film.py png  film_file out.png at_s [-s scale]
    film.py list film_file

The times are seconds of the bridge's clock. The GIF plays at the speed of
that clock: for every tick of its rate it shows the frame that the screen held
at that moment.
"""

import argparse
import sys

from PIL import Image

WIDTH, HEIGHT, ROW_BYTES = 128, 64, 16
DARK, LIT = (0, 0, 0), (255, 255, 255)


def read_film(path):
    frames = []
    with open(path) as file:
        for line in file:
            _, time_us, pixels = line.split()
            frames.append((int(time_us) / 1e6, bytes.fromhex(pixels)))
    return frames


def picture(pixels, scale):
    image = Image.new("P", (WIDTH, HEIGHT), 0)
    image.putpalette(DARK + LIT)
    image.putdata([(pixels[i // 8] >> (i % 8)) & 1 for i in range(WIDTH * HEIGHT)])
    return image.resize((WIDTH * scale, HEIGHT * scale), Image.NEAREST)


def frame_at(frames, time_s):
    shown = frames[0][1]
    for at, pixels in frames:
        if at > time_s:
            break
        shown = pixels
    return shown


def make_gif(args):
    frames = read_film(args.film)
    step_ms = round(1000 / args.rate / 10) * 10
    pictures, durations = [], []
    at = args.start
    last = None
    while at <= args.end:
        pixels = frame_at(frames, at)
        if pixels == last:
            durations[-1] += step_ms
        else:
            pictures.append(picture(pixels, args.scale))
            durations.append(step_ms)
            last = pixels
        at += step_ms / 1000
    durations[-1] += round(args.end_hold * 1000)
    pictures[0].save(
        args.out, save_all=True, append_images=pictures[1:], duration=durations,
        loop=0, optimize=False, disposal=1
    )
    print(f"{args.out}: {len(pictures)} pictures, {sum(durations) / 1000:.1f} s")


def make_png(args):
    frames = read_film(args.film)
    picture(frame_at(frames, args.at), args.scale).save(args.out, optimize=True)
    print(f"{args.out}: the screen at {args.at} s")


def list_film(args):
    frames = read_film(args.film)
    print(f"{len(frames)} frames, from {frames[0][0]:.1f} s to {frames[-1][0]:.1f} s")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    gif = commands.add_parser("gif")
    gif.add_argument("film")
    gif.add_argument("out")
    gif.add_argument("start", type=float)
    gif.add_argument("end", type=float)
    gif.add_argument("-r", "--rate", type=float, default=20)
    gif.add_argument("-s", "--scale", type=int, default=3)
    gif.add_argument("-e", "--end-hold", type=float, default=0)
    gif.set_defaults(run=make_gif)

    png = commands.add_parser("png")
    png.add_argument("film")
    png.add_argument("out")
    png.add_argument("at", type=float)
    png.add_argument("-s", "--scale", type=int, default=3)
    png.set_defaults(run=make_png)

    listing = commands.add_parser("list")
    listing.add_argument("film")
    listing.set_defaults(run=list_film)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    sys.exit(main())
