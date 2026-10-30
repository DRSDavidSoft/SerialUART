# Validation and physical acceptance

## Automated checks

Run `ctest --test-dir build -C Release --output-on-failure` after building.

- C ABI validation and C++ consumer linkage.
- RFC6455 published handshake vector, fragmented/bytewise parsing, control
  frames, invalid masking, UTF-8 and close-frame validation.
- Strict base64 and hexadecimal codecs, including NUL and high-bit data.
- Real localhost TCP, WebSocket and Socket.IO exchanges using a C echo adapter.
- Browser Origin rejection and explicit allowlisting.
- POSIX PTY binary pipelines, text/hex command replies, timeout exit and serial
  reconnect through a changed device symlink.
- Real CLI TCP/WS/Socket.IO-to-PTY bridges and TCP client mode.
- Terminal prefix shortcuts, literal Ctrl+C and input-state restoration on POSIX.
- Official Socket.IO 4.8.1 client binary exchange and 46-second heartbeat session.
- Installed CMake consumer compilation/link/execution, per native platform.
- ARMv7 cross-compiled binaries execute under QEMU; this is emulation evidence.

Python's standard library drives integration tests. Windows skips PTY-specific
tests and runs the real socket tests. No test opens arbitrary physical COM ports.

## Hardware checklist (requires actual devices)

Record OS, compiler/version, commit, executable hash, USB adapter/driver, board
model/firmware and observed result. Keep this checklist open until measured:

- [ ] Windows COM port and POSIX path enumerate with correct friendly name.
- [ ] VID/PID and serial match OS device-manager/sysfs/IOKit evidence.
- [ ] Multiple matching boards reject ambiguous selectors; serial selects one.
- [ ] Loopback or Arduino echo transfers all byte values repeatedly in both directions.
- [ ] 8N1 and supported nondefault parity/data/stop/RTS-CTS settings agree with peer.
- [ ] `--dtr reset --boot-wait 2000` resets the actual board and receives startup data.
- [ ] `--dtr off`/`keep` behavior measured across open/close/reconnect; note driver pulses.
- [ ] Unplug/replug resumes with stable identity; an unrelated board is never selected.
- [ ] Terminal shortcuts and restored console verified in Windows Terminal and POSIX terminal.
- [ ] Physical Raspberry Pi GPIO UART and USB serial verified separately.
- [ ] Network bridge reconnects after peer loss while the real serial device stays open.
- [ ] Long transfer/soak with slow consumers and physical flow control checked for loss.

Build, package and virtual serial success do not close electrical/reset/driver,
physical Pi, Windows serial-device or human terminal acceptance items.
