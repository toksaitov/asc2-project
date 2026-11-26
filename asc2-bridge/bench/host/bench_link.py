"""Links to the two bench boards: the Pico over USB serial and the DE10 over JTAG.

Pico: line commands to the firmware; every command answers one line that
starts with "ok" or "err".

DE10: a persistent `quartus_stp -s` session on the `quartus` host, talking to
an In-System Sources and Probes instance in the loaded design.
"""

import glob
import os
import re
import select
import subprocess
import sys
import termios
import time
import tty


class BenchError(Exception):
    pass


class Pico:
    def __init__(self):
        ports = sorted(glob.glob("/dev/cu.usbmodem*"))
        if len(ports) != 1:
            raise BenchError(f"expected exactly one Pico serial port, found {ports}")
        self.port = ports[0]
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY)
        tty.setraw(self.fd)
        attrs = termios.tcgetattr(self.fd)
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.buf = b""
        time.sleep(0.2)
        while select.select([self.fd], [], [], 0.05)[0]:
            if not os.read(self.fd, 4096):
                break

    def ask(self, line, timeout=2.0):
        """Send one command; return the answer line, "ok ..." or "err ..."."""
        self.buf = b""
        os.write(self.fd, (line + "\n").encode())
        end = time.time() + timeout
        while time.time() < end:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                text = raw.decode(errors="replace").strip()
                if text.startswith("ok") or text.startswith("err"):
                    return text
            if select.select([self.fd], [], [], 0.05)[0]:
                self.buf += os.read(self.fd, 4096)
        raise BenchError(f"no answer from the Pico to {line!r}")

    def lines(self, line, timeout=10.0):
        """Send one command that prints several lines before its answer;
        return (lines before the answer, answer line)."""
        self.buf = b""
        out = []
        os.write(self.fd, (line + "\n").encode())
        end = time.time() + timeout
        while time.time() < end:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                text = raw.decode(errors="replace").strip()
                if text.startswith("ok") or text.startswith("err"):
                    return out, text
                if text:
                    out.append(text)
            if select.select([self.fd], [], [], 0.05)[0]:
                self.buf += os.read(self.fd, 65536)
        raise BenchError(f"no answer from the Pico to {line!r}")

    def cmd(self, line, timeout=2.0):
        """Send one command that must succeed; return the answer line."""
        text = self.ask(line, timeout)
        if not text.startswith("ok"):
            raise BenchError(f"Pico rejected {line!r}: {text}")
        return text

    def close(self):
        os.close(self.fd)


class De10:
    def __init__(self, instance, source_width, probe_width, extra=None):
        """instance: the main ISSP instance (name, widths); extra: more
        instances as {name: (source_width, probe_width)}. read() and
        write() take an instance name and default to the main one."""
        self.p = subprocess.Popen(
            ["ssh", "-T", "quartus", "quartus_stp -s"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.buf = ""
        self._tcl('puts "@@ready@@"', "@@ready@@", 60)
        self._tcl(
            "set hw [lindex [get_hardware_names] 0]; "
            "set dev [lindex [get_device_names -hardware_name $hw] 0]; "
            'foreach i [get_insystem_source_probe_instance_info -hardware_name $hw -device_name $dev] { puts "@@inst $i@@" }; '
            "start_insystem_source_probe -hardware_name $hw -device_name $dev; "
            'puts "@@started@@"',
            "@@started@@", 60)
        found = {}
        for info in re.findall(r"@@inst ([^@]*)@@", self.buf):
            parts = info.split()
            if len(parts) == 4:
                found[parts[3]] = (int(parts[0]), int(parts[1]), int(parts[2]))
        wanted = {instance: (source_width, probe_width)}
        wanted.update(extra or {})
        self.main = instance
        self.index = {}
        for name, (sw, pw) in wanted.items():
            if name not in found or found[name][1:] != (sw, pw):
                self.close()
                raise BenchError(f"the DE10 is not running the expected design: want {name} {sw} {pw}, found {found}")
            self.index[name] = found[name][0]
        if extra is None and self.index[instance] != 0:
            self.close()
            raise BenchError(f"the DE10 is not running the expected design: {instance} is not instance 0")

    def _tcl(self, line, tag, timeout):
        self.p.stdin.write((line + "; flush stdout\n").encode())
        self.p.stdin.flush()
        end = time.time() + timeout
        while time.time() < end:
            if tag in self.buf:
                return
            if select.select([self.p.stdout], [], [], 0.1)[0]:
                chunk = os.read(self.p.stdout.fileno(), 4096)
                if not chunk:
                    break
                self.buf += chunk.decode(errors="replace")
        raise BenchError(f"no answer from quartus_stp waiting for {tag}: {self.buf[-300:]}")

    def read(self, name=None):
        idx = self.index[name or self.main]
        self.buf = ""
        self._tcl(f'puts "@@P[read_probe_data -instance_index {idx} -value_in_hex]@@"', "@@P", 10)
        end = time.time() + 5
        while time.time() < end:
            m = re.search(r"@@P([0-9A-Fa-f]+)@@", self.buf)
            if m:
                return int(m.group(1), 16)
            if select.select([self.p.stdout], [], [], 0.1)[0]:
                self.buf += os.read(self.p.stdout.fileno(), 4096).decode(errors="replace")
        raise BenchError(f"bad probe answer: {self.buf[-200:]}")

    def write(self, value, name=None):
        idx = self.index[name or self.main]
        self.buf = ""
        self._tcl(
            f"write_source_data -instance_index {idx} -value_in_hex -value {value:X}; "
            'puts "@@W@@"',
            "@@W@@", 10)

    def close(self):
        try:
            self.p.stdin.write(b"end_insystem_source_probe\nexit\n")
            self.p.stdin.flush()
            self.p.wait(timeout=20)
        except Exception:
            self.p.kill()
