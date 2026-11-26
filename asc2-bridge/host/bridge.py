#!/usr/bin/env python3
"""Runs a program on the board through the bridge.

The bridge is the firmware of the Pico next to the chip. Its USB serial port
carries the console of the chip, and Ctrl-A starts a line for its monitor.

    bridge.py run IMAGE              load a firmware image, run it, and be its terminal
    bridge.py run IMAGE --for 2      the same, but show the output of 2 seconds and leave
    bridge.py check IMAGE EXPECT     run a firmware image and compare what it prints with the file EXPECT
    bridge.py check IMAGE EXPECT --steps TRACE
                                     the same, but first run the instructions of the emulator's TRACE one at a time;
                                     with --regs the debug port of the chip is read around every step
    bridge.py check IMAGE EXPECT --lock
                                     the same, for a program that locks the debug port of the chip: the port must
                                     be open before the program starts and locked after its last line
    bridge.py check IMAGE EXPECT --keys "1c f0 1c"
                                     the same, with these bytes sent to the program as a keyboard sends them
    bridge.py term                   be the terminal of the program that runs
    bridge.py cmd "stat"             give the monitor one line and show its answer
    bridge.py disk put IMAGE         write a disk image into the flash of the Pico
    bridge.py save IMAGE             keep a firmware image in the flash of the Pico: the Pico runs it at every power-up
    bridge.py disk get IMAGE         read the disk of the Pico into a file

In the terminal Ctrl-] leaves, and Ctrl-A starts a line for the monitor.

The disk commands need `picotool`. It restarts the Pico, so the program that
was loaded is gone after them.
"""

import argparse
import csv
import difflib
import glob
import os
import select
import subprocess
import sys
import termios
import time
import tty
import zlib

MONITOR_KEY = b"\x01"
PROMPT = b"\r\nbridge> "  # what the monitor prints when it has got its key
LEAVE_KEY = 0x1d  # Ctrl-]
PORTS = "/dev/cu.usbmodem*"
RESTART_SECONDS = 20  # longest wait for a Pico that picotool has restarted
LOADER_SECONDS = 3  # time that the Pico takes to come up in its boot loader
PICOTOOL_SECONDS = 90   # the longest that a call of picotool may take


class BridgeError(Exception):
    pass


class Bridge:
    def __init__(self, port=None):
        if port is None:
            ports = sorted(glob.glob(PORTS))
            if len(ports) != 1:
                raise BridgeError(f"Expected one serial port of a Pico, found {ports}")
            port = ports[0]
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
        tty.setraw(self.fd)
        attrs = termios.tcgetattr(self.fd)
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.buf = b""      # bytes that have come and are not looked at yet
        self.printed = b""  # bytes that the program printed before a command of the monitor

    def close(self):
        os.close(self.fd)

    def receive(self, timeout):
        if select.select([self.fd], [], [], timeout)[0]:
            return os.read(self.fd, 65536)
        return b""

    def read(self, timeout):
        """Returns the bytes that have come, or b"" after `timeout` seconds without any."""
        # What the program printed around the commands of the monitor comes first
        if self.printed or self.buf:
            data = self.printed + self.buf
            self.printed = self.buf = b""
            return data
        return self.receive(timeout)

    def forget(self):
        """Drops what has been printed until now: it is not the output of the program that starts."""
        self.printed = b""

    def write(self, data):
        while data:
            data = data[os.write(self.fd, data):]

    def answer(self, timeout=5.0):
        """Waits for the monitor's answer; returns the lines before it and the answer."""
        lines = []
        end = time.time() + timeout
        while time.time() < end:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                text = raw.decode(errors="replace").strip()
                if text.startswith("ok") or text.startswith("err"):
                    return lines, text
                if text and not text.startswith("bridge>"):
                    lines.append(text)
            self.buf += self.receive(0.05)
        raise BridgeError("The bridge has not answered")

    def command(self, line, timeout=5.0):
        """Gives the monitor one line; returns the lines before its answer and the answer.

        The bytes that came before the monitor's prompt are output of the program and are kept for `read`.
        The monitor prints nothing of the program between its prompt and its answer.
        """
        self.write(MONITOR_KEY + line.encode() + b"\n")

        # The prompt, and after it the line as the monitor repeats it
        end = time.time() + timeout
        while PROMPT not in self.buf or b"\n" not in self.buf.split(PROMPT, 1)[1]:
            if time.time() > end:
                raise BridgeError("The bridge has not answered")
            self.buf += self.receive(0.05)
        printed, rest = self.buf.split(PROMPT, 1)
        self.printed += printed
        self.buf = rest.split(b"\n", 1)[1]

        return self.answer(timeout)

    def must(self, line, timeout=5.0):
        lines, text = self.command(line, timeout)
        if not text.startswith("ok"):
            raise BridgeError(f"The bridge refused '{line}': {text}")
        return lines, text

    def load(self, path):
        with open(path, "rb") as image_file:
            image = image_file.read()

        self.buf = b""
        self.write(MONITOR_KEY + f"load {len(image)}\n".encode())
        _, text = self.answer()
        if text != "ok send":
            raise BridgeError(f"The bridge refused the image: {text}")

        self.write(image)
        _, text = self.answer(timeout=30.0)
        if not text.startswith("ok"):
            raise BridgeError(f"The bridge refused the image: {text}")
        if text.split("crc=")[-1] != f"{zlib.crc32(image):08x}":
            raise BridgeError(f"The image arrived with other bytes: {text}")
        return text


def show(data):
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def terminal(bridge):
    """Joins the keyboard and the screen with the console of the chip until Ctrl-]."""
    stdin = sys.stdin.fileno()
    saved = termios.tcgetattr(stdin)
    try:
        tty.setraw(stdin)
        show(bridge.read(0))
        while True:
            ready = select.select([stdin, bridge.fd], [], [])[0]
            if bridge.fd in ready:
                show(bridge.read(0))
            if stdin in ready:
                keys = os.read(stdin, 1024)
                if LEAVE_KEY in keys:
                    break
                bridge.write(keys)
    finally:
        termios.tcsetattr(stdin, termios.TCSADRAIN, saved)
        print()


def watch(bridge, seconds, text):
    """Shows what the chip prints for `seconds`; `text` is typed after the first half second."""
    end = time.time() + seconds
    type_at = time.time() + 0.5
    while time.time() < end:
        if text is not None and time.time() >= type_at:
            bridge.write(text)
            text = None
        show(bridge.read(0.05))


def numbers_of(stat):
    """Returns the numbers of the monitor's answer to `stat` by their names."""
    return dict(pair.split("=") for pair in stat.split()[2:])


def fetches_of(lines):
    """Returns the address and the instruction of every fetch among the lines that `step` has printed.

    Both are text in hex, as the monitor prints them: `fetch 800000: 00000990`.
    """
    fetches = []
    for line in lines:
        kind, address, value = line.split(maxsplit=2)
        if kind == "fetch":
            fetches.append((address.rstrip(":"), value))
    return fetches


# The registers that the debug port shows, by their names in a trace of the emulator and in the answer of `regs`
REG_NAMES = ("r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "fp", "ksp", "lr", "usp", "cr1", "cr2", "cr3", "cr4")


def check_regs(bridge, expected):
    """Steps through the instructions of `expected` with a look at the debug port around each one.

    Before a step the port must show the address of the instruction in `ip`, and after it the instruction
    itself, with the CPU at rest between two instructions, and every register with the value that the
    emulator has after the same instruction. Returns the lines of all steps and None, or None and what
    went wrong.
    """
    lines = []
    for number, (address, inst, registers) in enumerate(expected):
        before = numbers_of(bridge.must("regs")[1])
        if int(before["ip"], 16) & 0x7fffffff != int(address, 16) or before["at_rest"] != "1":
            return None, (f"before step {number} the debug port shows ip {before['ip']} and at_rest {before['at_rest']}, "
                          f"and the emulator is at {address}")
        lines += bridge.must("step", timeout=30.0)[0]
        after = numbers_of(bridge.must("regs")[1])
        if after["inst"] != inst or after["at_rest"] != "1":
            return None, (f"after step {number} the debug port shows the instruction {after['inst']} and at_rest "
                          f"{after['at_rest']}, and the emulator has run {inst}")
        for name, wanted in registers.items():
            if name not in after:
                return None, f"the debug port does not show {name}; the firmware or the chip is an older one"
            if int(after[name], 16) != wanted:
                return None, (f"after step {number} the debug port shows {name} {after[name]}, "
                              f"and the emulator has {wanted:08x}")
    return lines, None


def check_steps(bridge, trace, with_regs):
    """Starts the loaded image with its CPU held and steps through it; returns None, or what went wrong.

    `trace` is a trace of the emulator: a line of names, and a line of values for every instruction. The
    chip must fetch the same instructions from the same addresses, one at a step. The address is the
    physical one, so the instructions must run without paging. The program is then let go, stopped in the
    middle of its run, stepped once more, and let go again. With `with_regs` the debug port of the chip is
    read around every step, see `check_regs`.
    """
    with open(trace, newline="") as trace_file:
        rows = list(csv.DictReader(trace_file))
    expected = [(f"{int(row['ip'], 16) & 0x7fffffff:08x}", row["inst"]) for row in rows]
    # The chip keeps the page part of cr3 alone
    shown = [{name: int(row[name], 16) & (0xfffff000 if name == "cr3" else 0xffffffff) for name in REG_NAMES}
             for row in rows]

    bridge.must("run stopped")
    bridge.forget()
    time.sleep(0.2)
    numbers = numbers_of(bridge.must("stat")[1])
    if numbers["requests"] != "0":
        return f"the CPU has sent {numbers['requests']} requests although it is held from the start"

    if with_regs:
        lines, error = check_regs(bridge, [step + (registers,) for step, registers in zip(expected, shown)])
        if error is not None:
            return error
    else:
        lines, _ = bridge.must(f"step {len(expected)}", timeout=30.0)
    fetched = fetches_of(lines)
    for number, (wanted, got) in enumerate(zip(expected, fetched)):
        if wanted != got:
            return (f"step {number} ran the instruction {got[1]} at {got[0]}, "
                    f"and the emulator runs {wanted[1]} at {wanted[0]}")
    if len(fetched) != len(expected):
        return f"{len(expected)} steps ran {len(fetched)} instructions"

    bridge.must("go")
    time.sleep(0.5)
    bridge.must("stop")
    before = numbers_of(bridge.must("stat")[1])
    time.sleep(0.2)
    after = numbers_of(bridge.must("stat")[1])
    if before["requests"] != after["requests"]:
        return "the chip sends requests after `stop`"
    lines, _ = bridge.must("step")
    stepped = numbers_of(bridge.must("stat")[1])
    if len(fetches_of(lines)) != 1 or int(stepped["fetches"]) != int(after["fetches"]) + 1:
        return "a step in the middle of the run did not run one instruction"
    bridge.must("go")
    return None


def parse_press(text):
    """A press of `--press`: the time in seconds, and the length in milliseconds or None for the usual one."""
    at, _, length = text.partition(":")
    return float(at), int(length) if length else None


def check(bridge, image, expect, seconds, clock, typed, trace, keys, with_regs, frame=None, presses=None,
          with_lock=False):
    """Runs an image and compares its output with the file `expect`; returns True if they are equal.

    `typed` is None or a file with bytes that are typed to the program. They are sent at once: the bridge
    keeps them until the program reads. `trace` is None or a trace of the emulator for `check_steps`, and
    `with_regs` says whether the steps also read the debug port.
    `keys` is None or bytes in hex for the keyboard of the chip. The bridge sends them a moment after the
    start, when the program waits for them: the keyboard controller keeps one byte.
    `frame` is None or a file with what the screen must show at the end, as the runner of the emulator
    writes it. `presses` is None or the times in seconds after the start at which the bridge presses the
    push button. `with_lock` says that the program locks the debug port: the port must be open while the CPU
    is held before its first instruction, and locked after the run.
    """
    with open(expect, "rb") as expect_file:
        expected = expect_file.read()

    name = os.path.splitext(os.path.basename(image))[0]
    if typed is not None:
        name += " with " + os.path.basename(typed)
    if trace is not None:
        name += " with steps and the debug port" if with_regs else " with steps"
    if keys is not None:
        name += " with a keyboard"
    if presses is not None:
        name += " with the button"
    if frame is not None:
        name += " with the screen"
    if with_lock:
        name += " with the lock of the debug port"

    bridge.must("halt")
    bridge.load(image)
    if clock is not None:
        bridge.must(f"clock {clock}")
    if trace is None and with_lock:
        bridge.must("run stopped")
        port = numbers_of(bridge.must("regs")[1])
        if port.get("locked") != "0":
            print(f"FAILED: {name} on the board: after a reset the debug port does not show an open port")
            bridge.must("halt")
            return False
        bridge.must("go")
        bridge.forget()
    elif trace is None:
        bridge.must("run")
        bridge.forget()
    else:
        error = check_steps(bridge, trace, with_regs)
        if error is not None:
            print(f"FAILED: {name} on the board: {error}")
            bridge.must("halt")
            return False
    if typed is not None:
        with open(typed, "rb") as typed_file:
            bridge.write(typed_file.read())
    if keys is not None:
        time.sleep(0.3)
        bridge.must(f"key {keys}")

    # The run is over when the whole output has come and nothing has followed it for a while
    got = b""
    start = time.time()
    end = start + seconds
    quiet_since = None
    presses = sorted(presses) if presses is not None else []
    while time.time() < end:
        if presses and time.time() - start >= presses[0][0]:
            length_ms = presses.pop(0)[1]
            bridge.must("press" if length_ms is None else f"press {length_ms}")
        data = bridge.read(0.05)
        got += data
        printed = got.replace(b"\r\n", b"\n")
        if data or printed != expected:
            quiet_since = None
        elif quiet_since is None:
            quiet_since = time.time()
        elif time.time() - quiet_since > 0.3:
            break
    printed = got.replace(b"\r\n", b"\n")

    # What went to the display last, in the form of the emulator's runner
    is_frame_right = True
    if frame is not None:
        rows, answer = bridge.must("frame")
        with open(frame) as frame_file:
            wanted = frame_file.read()
        if answer.endswith(" console"):
            # The screen shows the console: the pixels count, and not the number of frames that the
            # programs before have shown, which depends on the speed of the machine
            wanted = wanted.partition("\n")[2]
            shown = "".join(row + "\n" for row in rows)
        else:
            shown = "frames " + answer.split("frames=")[-1] + "\n" + "".join(row + "\n" for row in rows)
        is_frame_right = shown == wanted

    # The port of a program that has locked it shows the lock and no register
    is_lock_right = True
    if with_lock:
        port = numbers_of(bridge.must("regs")[1])
        is_lock_right = port.get("locked") == "1" and "ip" not in port

    _, stat = bridge.must("stat")
    numbers = numbers_of(stat)
    bridge.must("halt")

    if printed == expected and not is_lock_right:
        print(f"FAILED: {name} on the board (the debug port is not locked after the run)")
        return False

    if printed == expected and not is_frame_right:
        print(f"FAILED: {name} on the board (the screen differs from {frame})")
        sys.stdout.write(shown)
        return False

    if printed == expected and numbers["watchdog_resets"] == "0" and numbers["typed_lost"] == "0":
        print(f"passed: {name} on the board ({numbers['requests']} requests, {numbers['recoveries']} broken off, "
              f"{numbers['repeats']} repeated)")
        return True

    if printed != expected and expected.startswith(printed):
        print(f"FAILED: {name} on the board (it has printed {len(printed)} of {len(expected)} bytes in {seconds:g} seconds)")
    else:
        print(f"FAILED: {name} on the board")
        diff = difflib.unified_diff(expected.decode(errors="replace").splitlines(keepends=True),
                                    printed.decode(errors="replace").splitlines(keepends=True),
                                    "expected", "printed")
        sys.stdout.writelines(list(diff)[:40])
    print(stat)
    return False


def move_disk(port, direction, image):
    """Writes the file `image` to the disk in the flash of the Pico, or reads the disk into it.

    The monitor tells where the disk is, and picotool does the work. It restarts the Pico into its boot
    loader and back, so the serial port goes away and comes back.
    """
    bridge = Bridge(port)
    try:
        _, stat = bridge.must("stat")
    finally:
        bridge.close()
    start = int(numbers_of(stat)["disk_flash"], 16)
    size = int(numbers_of(stat)["disk_size"])

    if direction == "put":
        if os.path.getsize(image) != size:
            raise BridgeError(f"The disk has {size} bytes, and {image} has {os.path.getsize(image)}")
        arguments = ["load", "-v", image, "-t", "bin", "-o", f"{start:#x}"]
    else:
        arguments = ["save", "-r", f"{start:#x}", f"{start + size:#x}", image, "-t", "bin"]

    # Three steps: into the boot loader, the work, and back. picotool can do all three in one call (-f),
    # but with the firmware of 2026-11-24 the boot loader then stops answering after the work, for a
    # reason that is not known; in three calls it does not.
    for step in (["reboot", "-f", "-u"], arguments, ["reboot"]):
        try:
            result = subprocess.run(["picotool"] + step, capture_output=True, text=True, timeout=PICOTOOL_SECONDS)
        except subprocess.TimeoutExpired:
            # It has happened (once, on 2026-11-26, after dozens of good calls that night) that picotool
            # restarted the Pico into the boot loader and then did not return. The Pico is in the loader
            # then, and the work can go on.
            if "-u" in step:
                continue
            raise BridgeError(f"picotool {step[0]} has not ended in {PICOTOOL_SECONDS} seconds; `picotool reboot` "
                              "restarts the Pico out of its boot loader")
        if result.returncode != 0:
            raise BridgeError(f"picotool {step[0]} has failed:\n{result.stdout}{result.stderr}")
        if step[0] == "reboot" and "-u" in step:
            time.sleep(LOADER_SECONDS)

    # The firmware is back when its monitor answers
    end = time.time() + RESTART_SECONDS
    while time.time() < end:
        time.sleep(0.5)
        if port is None and len(glob.glob(PORTS)) != 1:
            continue
        try:
            bridge = Bridge(port)
        except OSError:
            continue
        try:
            bridge.must("stat", timeout=1.0)
            return
        except (BridgeError, OSError):
            pass
        finally:
            bridge.close()
    raise BridgeError("The Pico has not come back after picotool")


def main():
    parser = argparse.ArgumentParser(description="Runs a program on the board through the bridge.")
    parser.add_argument("--port", help="serial port of the Pico (found by itself if there is one Pico)")
    commands = parser.add_subparsers(dest="command", required=True)

    run = commands.add_parser("run", help="load a firmware image and run it")
    run.add_argument("image")
    run.add_argument("--clock", type=int, metavar="N", help="clock of the chip: the system clock of the Pico divided by N")
    run.add_argument("--for", dest="seconds", type=float, metavar="S", help="show the output of S seconds and leave")
    run.add_argument("--type", dest="text", metavar="TEXT", help="with --for: type TEXT to the program")

    save = commands.add_parser("save", help="keep a firmware image in the flash of the Pico, which then runs it at every power-up")
    save.add_argument("image")

    chk = commands.add_parser("check", help="run a firmware image and compare what it prints with a file")
    chk.add_argument("image")
    chk.add_argument("expect")
    chk.add_argument("--clock", type=int, metavar="N", help="clock of the chip: the system clock of the Pico divided by N")
    chk.add_argument("--for", dest="seconds", type=float, default=300.0, metavar="S", help="longest run in seconds (300)")
    chk.add_argument("--input", metavar="FILE", help="type the bytes of FILE to the program")
    chk.add_argument("--steps", metavar="TRACE", help="first run the instructions of the emulator's TRACE one at a time")
    chk.add_argument("--keys", metavar="BYTES", help="send these bytes, given in hex, as a keyboard does")
    chk.add_argument("--frame", metavar="FILE", help="compare what went to the display last with FILE")
    chk.add_argument("--press", metavar="TIMES", help="press the push button at these times, in seconds after the start; "
                          "a time may come with the length of the press in milliseconds, as 12.5:3000")
    chk.add_argument("--regs", action="store_true", help="with --steps: read the debug port of the chip around every step")
    chk.add_argument("--lock", action="store_true", help="the program locks the debug port: open before the run, locked after it")

    commands.add_parser("term", help="be the terminal of the program that runs")

    cmd = commands.add_parser("cmd", help="give the monitor one line")
    cmd.add_argument("line")

    disk = commands.add_parser("disk", help="write or read the disk in the flash of the Pico")
    disk.add_argument("direction", choices=["put", "get"])
    disk.add_argument("image")

    args = parser.parse_args()

    status = 0
    bridge = None
    try:
        if args.command == "disk":
            move_disk(args.port, args.direction, args.image)
            return 0

        bridge = Bridge(args.port)
        if args.command == "run":
            print(bridge.load(args.image), file=sys.stderr)
            if args.clock is not None:
                print(bridge.must(f"clock {args.clock}")[1], file=sys.stderr)
            bridge.must("run")
            bridge.forget()
            if args.seconds is None:
                terminal(bridge)
            else:
                watch(bridge, args.seconds, args.text.encode().decode("unicode_escape").encode() if args.text else None)
        elif args.command == "check":
            status = 0 if check(bridge, args.image, args.expect, args.seconds, args.clock, args.input, args.steps,
                                args.keys, args.regs, args.frame,
                                [parse_press(at) for at in args.press.split()] if args.press else None,
                                args.lock) else 1
        elif args.command == "save":
            print(bridge.load(args.image), file=sys.stderr)
            print(bridge.must("save", timeout=60.0)[1], file=sys.stderr)
            bridge.must("run")
        elif args.command == "term":
            terminal(bridge)
        else:
            lines, text = bridge.command(args.line)
            for line in lines:
                print(line)
            print(text)
            status = 0 if text.startswith("ok") else 1
    except (BridgeError, OSError) as error:
        print(error, file=sys.stderr)
        status = 1
    finally:
        if bridge is not None:
            bridge.close()

    return status


if __name__ == "__main__":
    sys.exit(main())
