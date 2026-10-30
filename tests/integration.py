"""Standard-library-only integration tests. Python is never a runtime dependency."""
import base64
import faulthandler
import contextlib
import hashlib
import os
import pathlib
import select
import shlex
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

CLI, ECHO = sys.argv[1:3]
EMULATOR = shlex.split(os.environ.get("SUART_EMULATOR", ""))
sys.argv[1:] = []
faulthandler.dump_traceback_later(25, repeat=True)


def exact(sock, n):
    result = b""
    while len(result) < n:
        part = sock.recv(n - len(result))
        if not part:
            raise EOFError("peer closed")
        result += part
    return result


def masked(data, op=2, fin=True):
    key = b"abcd"
    head = bytes([(128 if fin else 0) | op])
    head += bytes([128 | len(data)]) if len(data) < 126 else b"\xfe" + struct.pack("!H", len(data))
    return head + key + bytes(x ^ key[i % 4] for i, x in enumerate(data))


def ws_read(sock):
    a, b = exact(sock, 2)
    assert not b & 128
    n = b & 127
    if n == 126:
        n = struct.unpack("!H", exact(sock, 2))[0]
    return a & 15, exact(sock, n)


@contextlib.contextmanager
def server(mode, origin=None):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    args = [ECHO, mode, str(port)] + ([origin] if origin else [])
    p = subprocess.Popen(EMULATOR + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        assert p.stdout.readline().strip() == b"READY"
        with socket.create_connection(("127.0.0.1", port), timeout=3) as s:
            s.settimeout(3)
            yield s
    finally:
        p.kill()
        p.communicate(timeout=5)


def upgrade(s, mode="ws", origin=None, split=False):
    path = "/socket.io/?EIO=4&transport=websocket" if mode == "socketio" else "/serial"
    request = (f"GET {path} HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
               "Connection: keep-alive, Upgrade\r\nSec-WebSocket-Version: 13\r\n"
               "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
    if origin:
        request += f"Origin: {origin}\r\n"
    request = (request + "\r\n").encode()
    if split:
        for b in request:
            s.sendall(bytes([b]))
    else:
        s.sendall(request)
    response = b""
    while not response.endswith(b"\r\n\r\n"):
        response += exact(s, 1)
    assert b"101 Switching Protocols" in response
    assert b"s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" in response


class Network(unittest.TestCase):
    def test_tcp_binary(self):
        payload = bytes(range(256)) * 150
        with server("tcp") as s:
            s.sendall(payload)
            self.assertEqual(exact(s, len(payload)), payload)

    def test_websocket_fragment_ping_binary(self):
        with server("ws") as s:
            upgrade(s, split=True)
            s.sendall(masked(b"abc", fin=False) + masked(b"ping", 9) + masked(b"\x00\xff", 0))
            self.assertEqual(ws_read(s), (10, b"ping"))
            self.assertEqual(ws_read(s), (2, b"abc\x00\xff"))
            payload = bytes(range(256)) * 40
            s.sendall(masked(payload))
            data = b""
            while len(data) < len(payload):
                op, part = ws_read(s)
                self.assertEqual(op, 2)
                data += part
            self.assertEqual(data, payload)
            s.sendall(masked(struct.pack("!H", 1000), 8))
            self.assertEqual(ws_read(s), (8, struct.pack("!H", 1000)))

    def test_socketio(self):
        with server("socketio") as s:
            upgrade(s, "socketio")
            op, packet = ws_read(s)
            self.assertEqual(op, 1)
            self.assertTrue(packet.startswith(b'0{"sid":'))
            s.sendall(masked(b"40", 1))
            self.assertEqual(ws_read(s), (1, b'40{"sid":"serialuart"}'))
            payload = bytes(range(256))
            event = b'42["serial","' + base64.b64encode(payload) + b'"]'
            s.sendall(masked(event, 1))
            self.assertEqual(ws_read(s), (1, event))

    def test_origin_policy(self):
        with server("ws") as s:
            with self.assertRaises((EOFError, ConnectionError)):
                upgrade(s, origin="https://untrusted.example")
        with server("ws", "http://localhost:8000") as s:
            upgrade(s, origin="http://localhost:8000")
            s.sendall(masked(b"allowed"))
            self.assertEqual(ws_read(s), (2, b"allowed"))

    def test_invalid_frame(self):
        with server("ws") as s:
            upgrade(s)
            s.sendall(b"\x82\x01X")
            self.assertEqual(s.recv(1), b"")


class CLIValidation(unittest.TestCase):
    def test_invalid_options(self):
        for args in (["--baud", "0"], ["--baud", "115200junk"], ["--vid", "xyz"],
                     ["--data-bits", "9"], ["--reconnect", "0"], ["--parity", ""],
                     ["--hex", "1"], ["--terminal", "--receive"], ["--unknown", "x"]):
            p = subprocess.run(EMULATOR + [CLI] + args, capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 2, (args, p.stderr))
            self.assertEqual(p.stdout, b"")


@unittest.skipIf(os.name == "nt", "POSIX pseudo-terminal tests; Windows real serial requires hardware")
class SerialPTY(unittest.TestCase):
    def test_network_serial_bridges(self):
        import pty
        for mode in ("tcp", "ws", "socketio"):
            with self.subTest(mode=mode):
                master, slave = pty.openpty()
                with socket.socket() as reserve:
                    reserve.bind(("127.0.0.1", 0))
                    port = reserve.getsockname()[1]
                p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--listen", mode, "127.0.0.1", str(port)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    self.assertIn(b"Listening", p.stderr.readline())
                    self.assertIn(b"Connected", p.stderr.readline())
                    with socket.create_connection(("127.0.0.1", port), timeout=3) as s:
                        s.settimeout(3)
                        if mode != "tcp":
                            upgrade(s, mode)
                        if mode == "socketio":
                            self.assertEqual(ws_read(s)[0], 1)
                            s.sendall(masked(b"40", 1))
                            self.assertTrue(ws_read(s)[1].startswith(b"40"))
                        payload = b"\x00\xffSTATUS\r\n"
                        if mode == "tcp":
                            s.sendall(payload)
                        elif mode == "ws":
                            s.sendall(masked(payload))
                        else:
                            s.sendall(masked(b'42["serial","' + base64.b64encode(payload) + b'"]', 1))
                        self.assertTrue(select.select([master], [], [], 3)[0])
                        self.assertEqual(os.read(master, len(payload)), payload)
                        os.write(master, payload)
                        if mode == "tcp":
                            self.assertEqual(exact(s, len(payload)), payload)
                        elif mode == "ws":
                            self.assertEqual(ws_read(s), (2, payload))
                        else:
                            self.assertEqual(ws_read(s), (1, b'42["serial","' + base64.b64encode(payload) + b'"]'))
                finally:
                    p.terminate()
                    p.communicate(timeout=5)
                    os.close(master)
                    os.close(slave)

    def test_tcp_connect_serial_bridge(self):
        import pty
        master, slave = pty.openpty()
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.settimeout(3)
            p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--connect", "tcp", "127.0.0.1", str(listener.getsockname()[1])], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                with listener.accept()[0] as s:
                    s.settimeout(3)
                    self.assertIn(b"Connecting", p.stderr.readline())
                    self.assertIn(b"Connected", p.stderr.readline())
                    s.sendall(b"request")
                    self.assertTrue(select.select([master], [], [], 3)[0])
                    self.assertEqual(os.read(master, 7), b"request")
                    os.write(master, b"response")
                    self.assertEqual(exact(s, 8), b"response")
            finally:
                p.terminate()
                p.communicate(timeout=5)
                os.close(master)
                os.close(slave)

    def test_binary_pipe(self):
        import pty
        master, slave = pty.openpty()
        data = bytes(range(256)) * 100
        stop = threading.Event()
        errors = []
        # A blocking master write can remain stuck after the CLI closes on macOS,
        # preventing even close(master) in another thread from finishing.
        os.set_blocking(master, False)

        def board():
            pending = bytearray()
            try:
                while not stop.is_set():
                    readable, writable, _ = select.select([master], [master] if pending else [], [], 0.01)
                    if readable:
                        try:
                            pending.extend(os.read(master, 4096))
                        except BlockingIOError:
                            pass
                    if writable:
                        try:
                            n = os.write(master, pending)
                            del pending[:n]
                        except BlockingIOError:
                            pass
            except OSError as exc:
                if not stop.is_set():
                    errors.append(exc)

        t = threading.Thread(target=board)
        t.start()
        try:
            p = subprocess.run(EMULATOR + [CLI, "-p", os.ttyname(slave), "--timeout", "300", "-q"],
                               input=data, capture_output=True, timeout=15)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(p.stdout, data)
            self.assertFalse(errors)
        finally:
            stop.set()
            t.join(timeout=2)
            os.close(master)
            os.close(slave)

    def test_send_and_timeout(self):
        import pty
        master, slave = pty.openpty()
        try:
            p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--hex", "00 ff 41", "--timeout", "150", "-q"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertTrue(select.select([master], [], [], 3)[0])
            self.assertEqual(os.read(master, 3), b"\x00\xffA")
            os.write(master, b"OK\x00\xff")
            out, err = p.communicate(timeout=5)
            self.assertEqual((p.returncode, out), (0, b"OK\x00\xff"), err)
            p = subprocess.run(EMULATOR + [CLI, "-p", os.ttyname(slave), "-s", "X", "--timeout", "20", "-q"], capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 124)
        finally:
            os.close(master)
            os.close(slave)

    def test_reconnect_symlink(self):
        import pty
        master, slave = pty.openpty()
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "serial"
            path.symlink_to(os.ttyname(slave))
            p = subprocess.Popen(EMULATOR + [CLI, "-p", str(path), "--receive", "--reconnect", "30"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                self.assertIn(b"Connected", p.stderr.readline())
                os.close(master)
                os.close(slave)
                master, slave = pty.openpty()
                path.unlink()
                path.symlink_to(os.ttyname(slave))
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    line = p.stderr.readline()
                    if b"Connected" in line:
                        break
                else:
                    self.fail("did not reconnect")
                os.write(master, b"reconnected")
                self.assertTrue(select.select([p.stdout], [], [], 3)[0])
                self.assertEqual(os.read(p.stdout.fileno(), 11), b"reconnected")
            finally:
                p.terminate()
                p.communicate(timeout=5)
                os.close(master)
                os.close(slave)

    def test_terminal_shortcuts_and_restore(self):
        import pty
        import termios
        master, slave = pty.openpty()
        keyboard, console = pty.openpty()
        original = termios.tcgetattr(console)
        p = subprocess.Popen(EMULATOR + [CLI, "-p", os.ttyname(slave), "--terminal"],
                             stdin=console, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertIn(b"Ctrl+]", p.stderr.readline())
            self.assertIn(b"Connected", p.stderr.readline())
            os.write(keyboard, b"A\x03\x1d]B\x1d?")
            serial = b""
            deadline = time.monotonic() + 3
            while len(serial) < 4 and time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    serial += os.read(master, 16)
            self.assertEqual(serial, b"A\x03\x1dB")
            os.write(keyboard, b"\x1dq")
            p.communicate(timeout=3)
            self.assertEqual(p.returncode, 0)
            restored = termios.tcgetattr(console)
            if sys.platform == "darwin":
                # XNU sets transient PENDIN when ICANON is re-enabled even when
                # tcsetattr receives the exact saved flags (bsd/kern/tty.c).
                pending = getattr(termios, "PENDIN", 0x20000000)
                restored[3] &= ~pending
                original[3] &= ~pending
            self.assertEqual(restored, original)
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate(timeout=3)
            for fd in (master, slave, keyboard, console):
                os.close(fd)

    def test_terminal_quit_while_device_absent(self):
        import pty
        keyboard, console = pty.openpty()
        with tempfile.TemporaryDirectory() as tmp:
            p = subprocess.Popen(EMULATOR + [CLI, "-p", str(pathlib.Path(tmp) / "absent"), "--terminal", "--reconnect", "20"],
                                 stdin=console, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                self.assertIn(b"Ctrl+]", p.stderr.readline())
                os.write(keyboard, b"\x1dq")
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
