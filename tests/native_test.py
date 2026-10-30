"""Exercise native library APIs, CLI framing and counting tests through real PTYs."""
import os
import pathlib
import pty
import select
import shlex
import subprocess
import sys
import tempfile
import termios
import threading
import time
import tty
import unittest

NATIVE, LOOPBACK, CLI, DISCOVERY = sys.argv[1:5]
EMULATOR = shlex.split(os.environ.get("SUART_EMULATOR", ""))
sys.argv[1:] = []


class Native(unittest.TestCase):
    def test_library(self):
        master, slave = pty.openpty()
        try:
            t = termios.tcgetattr(slave)
            t[0] |= termios.IXON | termios.ICRNL | termios.ISTRIP
            if sys.platform.startswith("linux"):
                t[2] |= 0x40000000  # Linux UAPI CMSPAR, absent in Python termios.
            elif sys.platform == "darwin":
                # Apple termios.h CDTR_IFLOW | CDSR_OFLOW | CCAR_OFLOW.
                t[2] |= 0x001C0000
            termios.tcsetattr(slave, termios.TCSANOW, t)
            p = subprocess.run(EMULATOR + [NATIVE, os.ttyname(slave), str(master), str(slave)],
                               pass_fds=(master, slave), capture_output=True, timeout=10)
            self.assertEqual(p.returncode, 0, p.stderr)
        finally:
            os.close(master)
            os.close(slave)

    def loopback(self, corrupt=False, silent=False):
        master, slave = pty.openpty()
        tty.setraw(slave)
        stop = threading.Event()
        os.set_blocking(master, False)

        def peer():
            pending = bytearray()
            while not stop.is_set():
                r, w, _ = select.select([master], [master] if pending else [], [], .01)
                if r:
                    try:
                        data = os.read(master, 4096)
                    except BlockingIOError:
                        continue
                    if not silent:
                        pending.extend(bytes(x ^ 1 for x in data) if corrupt else data)
                if w:
                    try:
                        n = os.write(master, pending[:257])  # Fragment replies.
                        del pending[:n]
                    except BlockingIOError:
                        pass

        thread = threading.Thread(target=peer)
        thread.start()
        try:
            p = subprocess.run(EMULATOR + [LOOPBACK, os.ttyname(slave), "115200", "65536", "500" if silent else "5000"],
                               capture_output=True, timeout=8)
            self.assertEqual(p.returncode, 124 if silent else 1 if corrupt else 0, p.stderr)
            if not silent:
                self.assertIn(b"TX=65536 RX=65536", p.stderr)
        finally:
            stop.set()
            thread.join(timeout=2)
            self.assertFalse(thread.is_alive())
            os.close(master)
            os.close(slave)

    def test_loopback(self):
        self.loopback()

    def test_corruption(self):
        self.loopback(corrupt=True)

    def test_silent_timeout(self):
        self.loopback(silent=True)

    def test_line_command_and_total_deadline(self):
        for terminate in (True, False):
            master, slave = pty.openpty()
            p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--sendline", "STATUS",
                                            "--until", "0A", "--timeout", "300", "--show-config"],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                self.assertTrue(select.select([master], [], [], 3)[0])
                self.assertEqual(os.read(master, 100), b"STATUS\n")
                if terminate:
                    os.write(master, b"OK\x00\nleftover")
                else:
                    # Incoming bytes keep resetting an idle timer, but must
                    # never extend --until's total deadline.
                    for _ in range(20):
                        if p.poll() is not None:
                            break
                        os.write(master, b"x")
                        time.sleep(.025)
                out, err = p.communicate(timeout=3)
                self.assertEqual(p.returncode, 0 if terminate else 124, err)
                self.assertIn(b"Driver: 57600 baud", err)
                if terminate:
                    self.assertEqual(out, b"OK\x00\n")
            finally:
                if p.poll() is None:
                    p.kill()
                    p.communicate(timeout=3)
                os.close(master)
                os.close(slave)

    @unittest.skipUnless(sys.platform == "darwin", "macOS PTY retains an unread TX queue")
    def test_receive_continues_while_transmit_queue_is_blocked(self):
        master, slave = pty.openpty()
        p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--send", "X",
                                        "--write-timeout", "300", "--timeout", "100"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertIn(b"Connected", p.stderr.readline())
            self.assertTrue(select.select([master], [], [], 3)[0])
            # Leave X unread to block the TX queue; RX must still be serviced.
            time.sleep(.05)
            os.write(master, b"incoming while TX is blocked")
            out, err = p.communicate(timeout=3)
            self.assertEqual(p.returncode, 1, err)
            self.assertEqual(out, b"incoming while TX is blocked")
            self.assertIn(b"TX queue did not empty", err)
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate(timeout=3)
            os.close(master)
            os.close(slave)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux sysfs discovery")
    def test_discovery_no_name_whitelist_or_device_opens(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            sysroot, devroot = root / "sys", root / "dev"
            sysroot.mkdir()
            devroot.mkdir()
            usb = root / "usb"
            (usb / "interface").mkdir(parents=True)
            for key, value in {"idVendor": "2341", "idProduct": "0043", "product": "Fixture board", "serial": "ABC"}.items():
                (usb / key).write_text(value + "\n")
            for name in ("ttyO7", "customUART9", "ttyNotADevice"):
                (sysroot / name).mkdir()
                (sysroot / name / "device").symlink_to(usb / "interface")
            # /dev/null is a character device; /dev entries need only stat(),
            # not opening/probing. A regular file must never be enumerated.
            (devroot / "ttyO7").symlink_to("/dev/null")
            (devroot / "customUART9").symlink_to("/dev/null")
            (devroot / "ttyNotADevice").write_text("not a character device")
            (sysroot / "tty0").mkdir()  # No hardware device association.
            p = subprocess.run(EMULATOR + [DISCOVERY, str(sysroot), str(devroot)], capture_output=True, timeout=3)
            self.assertEqual(p.returncode, 0, p.stderr)
            lines = p.stdout.decode().splitlines()
            self.assertEqual(len(lines), 2)
            self.assertTrue(all("Fixture board\t2341:0043\tABC" in line for line in lines))
            self.assertTrue(any("customUART9\t" in line for line in lines))


if __name__ == "__main__":
    unittest.main(verbosity=2)
