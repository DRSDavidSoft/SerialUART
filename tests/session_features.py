"""Feature-port checks: actual native output, profile processing, logs and PTYs."""
import os
import pathlib
import re
import select
import shlex
import subprocess
import sys
import tempfile
import time
import unittest

CLI, VIEW = sys.argv[1:3]
EMULATOR = shlex.split(os.environ.get("SUART_EMULATOR", ""))
sys.argv[1:] = []
ROOT = pathlib.Path(__file__).resolve().parent.parent


def view(data, *args):
    p = subprocess.run(EMULATOR + [VIEW, *args], input=data, capture_output=True, timeout=5)
    if p.returncode:
        raise AssertionError(p.stderr)
    return p.stdout


class Sessions(unittest.TestCase):
    def test_binary_and_display_modes(self):
        data = bytes(range(256)) * 5
        self.assertEqual(view(data), data)
        self.assertEqual(view(b"A\0\x7f\xff\r\n\b\t", "--display", "raw"), b"A[00][7F][FF]\r\n\b\t")
        self.assertEqual(view("café".encode(), "--display", "ascii"), b"caf??")
        self.assertEqual(view(b"A\r\nB\r", "--force-newline", "on"), b"A\n\nB\n")

    def test_fragmented_osc_filter_and_raw_capture(self):
        data = b"A\x1b]0;old\x07B\x1b]2;new\x1b\\C\x1b[31mD\x1b]9;keep\x07tail\x1b]2"
        expected = b"ABC\x1b[31mD\x1b]9;keep\x07tail\x1b]2"
        with tempfile.TemporaryDirectory() as tmp:
            log = pathlib.Path(tmp) / "串口 capture.bin"
            self.assertEqual(view(data, "--status", "off", "--log", str(log)), expected)
            self.assertEqual(log.read_bytes(), data)
            self.assertEqual(view(data, "--status", "off", "--log", str(log), "--log-mode", "overwrite", "--log-format", "display"), expected)
            self.assertEqual(log.read_bytes(), expected)

    def test_logging_append_overwrite_time_and_prompt_flush(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = pathlib.Path(tmp) / "log.bin"
            log.write_bytes(b"existing\0")
            view(b"one\xff", "--log", str(log))
            self.assertEqual(log.read_bytes(), b"existing\0one\xff")
            view(b"two", "--log", str(log), "--log-mode", "overwrite")
            self.assertEqual(log.read_bytes(), b"two")
            view(b"first\nsecond", "--log", str(log), "--log-mode", "overwrite", "--log-format", "display", "--log-time")
            self.assertRegex(log.read_text(), r"^\[\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ\] first\n\[.*Z\] second$")
            p = subprocess.Popen(EMULATOR + [VIEW, "--log", str(log), "--log-mode", "overwrite"],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                p.stdin.write(b"low rate")
                p.stdin.flush()
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline and log.read_bytes() != b"low rate":
                    time.sleep(.01)
                self.assertEqual(log.read_bytes(), b"low rate")
                p.stdin.close()
                p.stdin = None
                p.communicate(timeout=3)
                self.assertEqual(p.returncode, 0)
            finally:
                if p.poll() is None:
                    p.kill()
                    p.communicate(timeout=3)

    def test_profiles_precedence_unicode_and_literal_option_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            global_dir = root / "global" / ("SerialUART" if os.name == "nt" else "serialuart")
            global_dir.mkdir(parents=True)
            (global_dir / "settings.cfg").write_text("--baud 9600\n")
            (root / ".serialuart").mkdir()
            (root / ".serialuart/settings.cfg").write_text("--baud=19200\n")
            explicit = root / "串口 profile.cfg"
            explicit.write_text('# comment\n--baud 38400\n--name="Mixed Case 串口"\n', encoding="utf-8-sig")
            env = dict(os.environ)
            env["APPDATA" if os.name == "nt" else "XDG_CONFIG_HOME"] = str(root / "global")
            p = subprocess.run(EMULATOR + [CLI, "--profiles", "--baud", "115200", "--config", str(explicit), "--list-settings"],
                               cwd=root, env=env, capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 0, p.stderr)
            text = p.stdout.decode("utf-8")
            positions = [text.index(x) for x in ("9600", "19200", "38400", "115200")]
            self.assertEqual(positions, sorted(positions))
            self.assertIn("Mixed Case 串口", text)
            p = subprocess.run(EMULATOR + [CLI, "--terminal", "--title", "--config", "--list-settings"], capture_output=True, timeout=3)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertIn(b"--config", p.stdout)

    def test_invalid_profiles_and_feature_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile = pathlib.Path(tmp) / "invalid.cfg"
            for text in ("--send ACTUATE\n", "--config nested.cfg\n", "--unknown value\n", "--echo maybe\n", "x" * 9000):
                profile.write_text(text)
                p = subprocess.run(EMULATOR + [CLI, "--config", str(profile), "--list-settings"], capture_output=True, timeout=3)
                self.assertEqual(p.returncode, 2, p.stderr)
        for args in (("--log-time",), ("--display", "bad"), ("--exit-key", "AB"), ("--enter-hex", "xyz"),
                     ("--include", "vid=xyz"), ("--echo", "on"), ("--board", "ZZZZ:0001=bad")):
            p = subprocess.run(EMULATOR + [CLI, *args], capture_output=True, timeout=3)
            self.assertEqual(p.returncode, 2 if args != ("--log-time",) else 1, p.stderr)

    def test_catalog_generation_and_listing(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = pathlib.Path(tmp) / "boards.inc"
            p = subprocess.run([sys.executable, str(ROOT / "tools/import_boards.py"), str(ROOT / "data/simplyserial-boards.json"), "--output", str(output)],
                               capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), (ROOT / "cli/boards.inc").read_text(encoding="utf-8"))
        p = subprocess.run(EMULATOR + [CLI, "--board", "1234:ABCD=My Custom Board", "--list-boards"], capture_output=True, timeout=3)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn(b"My Custom Board", p.stdout)
        self.assertGreater(len(p.stdout.splitlines()), 470)


@unittest.skipIf(os.name == "nt", "POSIX terminal integration; Windows encoder tested separately")
class Terminal(unittest.TestCase):
    def test_line_edit_echo_binary_enter_and_exit_key(self):
        import pty
        master, slave = pty.openpty()
        keyboard, console = pty.openpty()
        p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--terminal", "--line-buffer", "on",
                                        "--echo", "on", "--enter-hex", "00 ff 0d 0a", "--exit-key", "X"],
                             stdin=console, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertIn(b"Ctrl+]", p.stderr.readline())
            self.assertIn(b"Connected", p.stderr.readline())
            os.write(keyboard, b"ab\xc3\xa9\x7fC")
            self.assertFalse(select.select([master], [], [], .1)[0])
            os.write(keyboard, b"\r")
            self.assertTrue(select.select([master], [], [], 3)[0])
            self.assertEqual(os.read(master, 100), b"abC\0\xff\r\n")
            os.write(keyboard, b"\x18")
            out, err = p.communicate(timeout=3)
            self.assertEqual(p.returncode, 0, err)
            self.assertIn(b"ab\xc3\xa9\b \bC\r\n", out)
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate(timeout=3)
            for fd in (master, slave, keyboard, console):
                os.close(fd)

    def test_auto_one_virtual_path_and_offline_exit(self):
        import pty
        keyboard, console = pty.openpty()
        with tempfile.TemporaryDirectory() as tmp:
            p = subprocess.Popen(EMULATOR + [CLI, "-p", str(pathlib.Path(tmp) / "absent"), "--terminal",
                                            "--autoconnect", "one", "--exit-key", "X"],
                                 stdin=console, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                self.assertIn(b"Ctrl+]", p.stderr.readline())
                os.write(keyboard, b"\x18")
                p.communicate(timeout=3)
                self.assertEqual(p.returncode, 0)
            finally:
                if p.poll() is None:
                    p.kill()
                    p.communicate(timeout=3)
                os.close(keyboard)
                os.close(console)


if __name__ == "__main__":
    unittest.main(verbosity=2)
