# Checks that a film of the screen (the `-m` file of the trace tool, a line
# for every frame with its pixels as hex digits) has a frame with the pixels
# of a screen file (the `-f` file, rows of `#` and `.`).
import sys

film_path, frame_path = sys.argv[1], sys.argv[2]

rows = [line.strip() for line in open(frame_path) if line.strip() and set(line.strip()) <= set("#.")]
frame = bytearray()
for row in rows:
    for start in range(0, len(row), 8):
        frame.append(sum(1 << bit for bit in range(8) if row[start + bit] == "#"))

wanted = frame.hex()
if not any(line.split()[-1] == wanted for line in open(film_path) if line.strip()):
    sys.exit(f"no frame of {film_path} has the pixels of {frame_path}")
