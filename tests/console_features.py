"""Real console PTY integration; no physical device is opened."""
import json, os, pathlib, select, shlex, struct, subprocess, sys, tempfile, time, unittest
CLI = sys.argv[1]
EMULATOR = shlex.split(os.environ.get('SUART_EMULATOR', ''))
sys.argv[1:] = []
class Console(unittest.TestCase):
    def test_json_and_closed_output(self):
        p = subprocess.run(EMULATOR+[CLI,'--list-json'], capture_output=True, timeout=10)
        self.assertEqual(p.returncode,0,p.stderr)
        for device in json.loads(p.stdout):
            self.assertIsInstance(device['instance_id'],str)
            self.assertIn('manufacturer',device)
            self.assertIn('problem_code',device)
        p = subprocess.run(EMULATOR+[CLI,'--tui'],input=b'quit\n',capture_output=True,timeout=5)
        self.assertNotEqual(p.returncode,0)
    @unittest.skipIf(os.name=='nt','requires POSIX pseudo terminals')
    def test_menu_edit_capture_terminal_and_restore(self):
        import pty, termios, fcntl
        console, screen = pty.openpty(); wire, port = pty.openpty()
        fcntl.ioctl(screen,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
        before=termios.tcgetattr(screen)
        with tempfile.TemporaryDirectory() as tmp:
            log=pathlib.Path(tmp)/'capture.bin'
            p=subprocess.Popen(EMULATOR+[CLI,'--tui','--port',os.ttyname(port),'--log',str(log)],stdin=screen,stdout=screen,stderr=screen,close_fds=True)
            output=bytearray()
            def until(needle, timeout=6):
                end=time.monotonic()+timeout; start=len(output)
                while time.monotonic()<end:
                    if needle in output[start:]: return
                    ready,_,_=select.select([console],[],[],.1)
                    if ready: output.extend(os.read(console,65536))
                self.fail('Missing '+repr(needle)+' in '+repr(bytes(output[-3500:])))
            def command(text): os.write(console,b'\x1b'); time.sleep(.08); os.write(console,text.encode()+b'\r')
            try:
                until(b'Serial workspace')
                command('settings'); until(b'Serial settings')
                # Change Baud through the actual inline field editor.
                os.write(console,b'\r\x159600\r')
                time.sleep(.1)
                command('connect'); until(b'Connected')
                if sys.platform.startswith('linux'):
                    # PTY termios2 validated elsewhere; the live settings must show requested rate.
                    self.assertIn(b'9600',output)
                os.write(wire,b'board\x00\xff\n')
                end=time.monotonic()+3
                while (not log.exists() or log.read_bytes()!=b'board\x00\xff\n') and time.monotonic()<end: time.sleep(.02)
                self.assertEqual(log.read_bytes(),b'board\x00\xff\n')
                command('history'); until(b'Receive history'); self.assertNotIn(b'\x00\xff',output)
                command('sendhex 00 FF 0D')
                ready,_,_=select.select([wire],[],[],3); self.assertTrue(ready)
                self.assertEqual(os.read(wire,3),b'\x00\xff\r')
                command('terminal'); until(b'Serial terminal')
                os.write(console,b'hello\x03\x04\x1d]')
                got=b''; end=time.monotonic()+3
                while len(got)<8 and time.monotonic()<end:
                    ready,_,_=select.select([wire],[],[],.1)
                    if ready: got+=os.read(wire,100)
                self.assertEqual(got,b'hello\x03\x04\x1d')
                os.write(console,b'\x1dm'); until(b'Serial workspace')
                command('disconnect'); until(b'Disconnected')
                command('quit'); self.assertEqual(p.wait(timeout=4),0)
                self.assertEqual(termios.tcgetattr(screen),before)
            finally:
                if p.poll() is None: p.terminate(); p.wait(timeout=3)
                for fd in (console,screen,wire,port): os.close(fd)
if __name__=='__main__': unittest.main()

