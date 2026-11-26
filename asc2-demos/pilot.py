"""
Flies the burkut of the game on the emulator and writes the presses of the
button as an events file for `asc_reftrace`.

The emulator is the same at every run, so the pilot flies in rounds: it runs
the game with the presses it has, reads the film of the screen, decides the
presses for the frames that the film now shows, and runs again. It knows how
the burkut rises and sinks, and it reads the holes of the pillars from the
frames, as a player does. After the wanted number of points it lets go of
the button.

    python3 -I pilot.py REFTRACE IMAGE POINTS EVENTS_FILE
"""

import subprocess
import sys
import tempfile

WIDTH, HEIGHT = 128, 64
FIRST_PRESS = 50000   # instructions before the press that starts the flight
PRESS_AFTER = 100     # instructions between a frame's arrival and the press for the next frame
PRESS_LENGTH = 200
STEPS_PER_FRAME = 4200

# The numbers of burkut.c
BURKUT_X, BURKUT_H, GROUND_Y = 20, 8, 62
PILLARS, PILLAR_W, PILLAR_STEP, GAP, SPEED = 3, 10, 48, 24, 21
GRAVITY, FLAP, MAX_FALL = 2, -20, 27


def pixels(hex_frame):
    data = bytes.fromhex(hex_frame)
    return lambda x, y: (data[y * (WIDTH // 8) + x // 8] >> (x % 8)) & 1


def is_flight(at):
    """A frame of a flight has the ground; the last one, under the sign, counts too."""
    return all(at(x, GROUND_Y) for x in range(WIDTH))


def is_over(at):
    return all(at(x, 14) for x in range(10, 118)) and not at(64, 18)


def run(reftrace, image, events, steps):
    with tempfile.TemporaryDirectory() as folder:
        events_path, film_path = folder + "/events", folder + "/film"
        with open(events_path, "w") as file:
            for order, level in events:
                file.write(f"{order},{level}\n")
        subprocess.run([reftrace, "-q", "-r", "-n", str(steps), "-e", events_path, "-m", film_path, image],
                       stderr=subprocess.DEVNULL, check=False)
        film = []
        with open(film_path) as file:
            for line in file:
                order, _, frame = line.split()
                film.append((int(order), pixels(frame)))
    return film


def hole_of(at, x):
    """The first row of the hole of the pillar at x, read from the bottom up, or None if the pillar hides."""
    for column in (x + PILLAR_W - 1, x):
        if 0 <= column < WIDTH and not BURKUT_X <= column < BURKUT_X + 12:
            row = GROUND_Y - 1
            while at(column, row):
                row -= 1
            return row - GAP + 1
    return None


def plan(film, points):
    """The frames of the first flight before which the button is pressed, and whether the flight is as long as wanted."""
    flight = [(order, at) for order, at in film if is_flight(at)]
    pillar_x = [(WIDTH + i * PILLAR_STEP) << 4 for i in range(PILLARS)]
    holes = [None] * PILLARS
    height, speed = 24 << 4, 0
    passed = 0
    presses = []
    is_pressed = False

    for order, at in flight:
        if is_over(at):
            return presses, passed >= points

        # What the program did for this frame
        if is_pressed:
            speed = FLAP
        speed = min(speed + GRAVITY, MAX_FALL)
        height = max(height + speed, 0)
        if height == 0:
            speed = 0
        for i in range(PILLARS):
            pillar_x[i] -= SPEED
            if (pillar_x[i] >> 4) < -PILLAR_W:
                pillar_x[i] += (PILLARS * PILLAR_STEP) << 4
                holes[i] = None
            was_ahead = ((pillar_x[i] + SPEED) >> 4) + PILLAR_W >= BURKUT_X
            if was_ahead and (pillar_x[i] >> 4) + PILLAR_W < BURKUT_X:
                passed += 1
            hole = hole_of(at, pillar_x[i] >> 4)
            if hole is not None:
                holes[i] = hole

        # The next pillar that the burkut has not left yet
        ahead = [(pillar_x[i], holes[i]) for i in range(PILLARS) if (pillar_x[i] >> 4) + PILLAR_W > BURKUT_X]
        hole = min(ahead)[1] if ahead else None
        wanted = (hole + (GAP - BURKUT_H) // 2) if hole is not None else 27

        sunk = (height + min(speed + GRAVITY, MAX_FALL)) >> 4
        is_pressed = passed < points and sunk > wanted + 2
        if is_pressed:
            presses.append(order + PRESS_AFTER)

    return presses, False


def main():
    reftrace, image, points, events_path = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]

    events = [(FIRST_PRESS, "down"), (FIRST_PRESS + PRESS_LENGTH, "up")]
    presses = []
    for _ in range(400):
        steps = FIRST_PRESS + STEPS_PER_FRAME * (60 * points + 200)
        film = run(reftrace, image, events, steps)
        new_presses, is_done = plan(film, points)
        if is_done:
            break
        if new_presses == presses:
            sys.exit(f"the burkut fell with {len(presses)} presses and the pilot knows no better")
        presses = new_presses
        events = [(FIRST_PRESS, "down"), (FIRST_PRESS + PRESS_LENGTH, "up")]
        for order in presses:
            events += [(order, "down"), (order + PRESS_LENGTH, "up")]
    else:
        sys.exit("the pilot did not finish the flight")

    with open(events_path, "w") as file:
        file.write("# The presses of the button that fly the burkut of the game on the emulator,\n")
        file.write("# written by `pilot.py`: the number of instructions that the program has\n")
        file.write("# finished before the button moves, and its new level.\n")
        for order, level in events:
            file.write(f"{order},{level}\n")


if __name__ == "__main__":
    main()
