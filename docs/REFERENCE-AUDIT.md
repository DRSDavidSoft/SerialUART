# Serial reference audit

Reviewed on **2026-10-31** for SerialUART 2.1. The original links are preserved
below, including broken links. “Unavailable” means the content could not be
retrieved in this audit; it does not mean its advice was verified. Replacements
are identified separately. Tutorials informed independent implementations;
their source code was not imported or added as a dependency.

## What changed

| Finding | SerialUART change | Automated evidence |
|---|---|---|
| Accepted writes can remain queued; flow control can stall transmission | `suart_wait_tx` uses a monotonic deadline; `suart_discard` explicitly destroys queued bytes | PTY transmit/receive and discard tests; CLI EOF handling |
| A failed open has no surviving handle for error queries | `suart_open_ex` retains `errno` / `GetLastError` before cleanup | Missing-device and conflicting-owner tests |
| Inherited configuration can change bytes or hold control lines | Clear Linux `CMSPAR` and macOS DTR/DSR/DCD flow flags; expose driver configuration | Seeded raw-mode tests; native matrix |
| Two writers can corrupt a command stream | POSIX `flock` plus `TIOCEXCL`, released on failure and close | Conflicting open followed by successful reopen |
| Baud constants and numeric rates are different interfaces | Expanded standard rates; Linux `termios2` / `BOTHER` in a separate translation unit, with readback and restoration | 31250 baud, restoration of 23977 baud, x64/ARM64/ARMv7 |
| Byte counts alone miss corruption | `SerialLoopback` checks exact content and counts under a total deadline; optional Linux driver error deltas | Fragmented 64 KiB echo, deliberate corruption, silent peer |
| Continuous input defeats an idle timeout | `--until HEX` uses a total reply deadline and byte-exact terminator handling | NUL-containing line reply and continuous unterminated input |
| Fixed device-name lists miss native UART families | Linux enumeration walks sysfs device associations and reads USB ancestors without opening ports | Unusual names, USB metadata and non-device fixtures |
| Native build dialects differ | GNU Make and NMake entry points share the CMake target definitions | Linux/macOS `make test`, Windows `nmake /f Makefile.msvc test` |

See [API](API.md), [usage](USAGE.md), [builds](BUILDING.md), and
[validation](VALIDATION.md). Passing these checks does not prove physical DTR
timing, USB metadata, UART clock accuracy, electrical wiring, or a hardware soak.

## Every original reference

### Arduino reset

1. [DisablingAutoResetOnSerialConnection](https://playground.arduino.cc/Main/DisablingAutoResetOnSerialConnection)
   — **Unavailable.** Used the current [Arduino platform specification](https://docs.arduino.cc/arduino-cli/platform-specification#1200-bps-bootloader-reset)
   and [UNO R3 reset-enable schematic](https://docs.arduino.cc/resources/schematics/A000066-schematics.pdf)
   as replacements. DTR assertion is a logical signal, not a universal voltage.
   `keep`/`off` cannot undo an initial driver pulse. Native USB 1200-bps bootloader
   touch is a separate mechanism; `--dtr reset` does not implement it.

### Windows

2. [Serial-Programming-Win32API-C](https://github.com/xanthium-enterprises/Serial-Programming-Win32API-C)
   — **Original repository returned 404.** Inspected the author's maintained
   [Win32 tutorial repository](https://github.com/xanthium-enterprises/serial-port-programming-tutorial-win32-api),
   including DCB configuration, open-error handling, polling and `WaitCommEvent`
   examples. Applied retained open errors and driver readback. SerialUART keeps
   bounded polling rather than introducing a blocking event wait.
3. [Original Win32 article](http://xanthium.in/Serial-Port-Programming-using-Win32-API)
   — **Unavailable at the original URL.** The replacement repository above
   supplies the primary source examples. Checked their assumptions against
   Microsoft documentation; settings inherited from `GetCommState` still need
   explicit binary and flow-control configuration.
4. [Batchloaf SerialSend](https://batchloaf.wordpress.com/serialsend/)
   — **Read article, embedded C source and relevant discussion.** Its partial
   write loop and close-delay behavior highlight transmit completion. Added a
   bounded queue wait, while retaining explicit device selection. Scanning by
   opening successively lower COM numbers can reset unrelated boards and is
   unsuitable for discovery. A queue-empty result is not an application ACK.

### POSIX, Raspberry Pi and Linux

5. [IBM z/OS `rtcgis.htm`](https://www.ibm.com/support/knowledgecenter/SSLTBW_1.13.0/com.ibm.zos.r13.bpxbd00/rtcgis.htm)
   — **Unavailable**, including attempted current IBM documentation URLs.
   No z/OS support or source-specific conclusion is claimed. Verified speed
   access through native termios documentation instead; Linux kernel structs
   are isolated from libc structs.
6. [FSU Linux 2.6.25 `suncore.c`](https://www.cs.fsu.edu/~baker/devices/lxr/source/2.6.25/linux/drivers/serial/suncore.c)
   — **Mirror unavailable; read the same version from
   [upstream Linux](https://github.com/torvalds/linux/blob/v2.6.25/drivers/serial/suncore.c).**
   This is Sun kernel console/mouse code, not a portable userspace API. Its
   fallback to 9600 and platform-specific device-tree configuration are not
   suitable library policies. SerialUART rejects unsupported settings.
7. [WiringPi `serialTest.c`](https://raw.githubusercontent.com/WiringPi/WiringPi/master/examples/serialTest.c)
   — **Read**, together with `wiringPi/wiringSerial.c` and its history. The
   counting-byte loopback informed `SerialLoopback`; broader speed constants
   informed the native backend. SerialUART preserves partial-write counts,
   bounded waits, and explicit DTR policy rather than copying the example's
   unconditional control-line assertions. WiringPi remains an external reference.
8. [I-Programmer: Serial C and the Raspberry Pi](http://www.i-programmer.info/programming/cc/10027-serial-c-and-the-raspberry-pi.html?start=2)
   — **Read setup and polling material.** Useful explanation of raw mode,
   `VMIN`/`VTIME`, and waiting for data. Its small receive example writes a
   terminator beyond an eleven-byte array; SerialUART uses byte lengths and
   bounded buffers. Current Pi documentation replaces assumptions about a
   universal `/dev/ttyAMA0` mapping.
9. [Todbot `arduino-serial.c`](https://github.com/todbot/arduino-serial/blob/master/arduino-serial.c)
   — **Read CLI and library**, plus history and issues. The project now uses
   `main`. Added `--sendline` and terminator-based replies. Kept binary data
   separate from C strings and used a real elapsed deadline rather than a
   loop counter that advances only when reads return no bytes.
10. [Original Xanthium Linux article](http://xanthium.in/Serial-Port-Programming-on-Linux)
    — **Original unavailable.** Inspected the author's maintained
    [Linux tutorial repository](https://github.com/xanthium-enterprises/linux-serialport-programming-using-c-termios-api),
    including receive, echo and discovery examples. Its replacement article
    timed out during retrieval. Applied explicit binary configuration and
    startup waiting, without inheriting fixed delays, implicit discards, or
    shell-based `/dev` glob discovery.
11. [cbrake/linux-serial-test](https://github.com/cbrake/linux-serial-test)
    — **Deep source/history/issue review; see below.** Main lessons: count and
    verify every byte, respect partial writes, separate driver error counters
    from application counts, and bound waits under flow control. Added the
    independent loopback utility, optional `TIOCGICOUNT` reporting and Linux
    custom rates. Linux-only RS485/divisor operations are not portable defaults.
12. [Teuniz RS-232](http://www.teuniz.net/RS-232/)
    — **Read API, changelog and linked
    [source repository](https://gitlab.com/Teuniz/RS-232).** Its ownership,
    error cleanup, modem inputs, explicit RX/TX discard and partial-write
    changes informed the APIs and tests. Its fixed port table and unconditional
    control-line changes were not adopted. GPL source was not imported.
13. [TLDP Serial HOWTO](http://www.tldp.org/HOWTO/Serial-HOWTO.html)
    — **Read serial basics and flow-control guidance.** Application buffering,
    UART buffering and cable handshaking are different layers. RTS/CTS needs
    matching wiring and peer configuration; manual RTS is rejected when
    handshake owns it. Historic ISA IRQ, `setserial`, and device-node recipes
    are not general USB-serial setup instructions.

### DTR and modem control

14. [Experts Exchange termios DTR question](https://www.experts-exchange.com/questions/26929457/How-to-set-DTR-on-termios-in-Linux.html)
    — **403; unread.** No paid answer is inferred. Used native
    [modem ioctl documentation](https://man7.org/linux/man-pages/man2/TIOCMGET.2const.html)
    to verify the independent implementation.
15. [Original Xanthium RTS/DTR article](http://xanthium.in/Controlling-RTS-and-DTR-pins-SerialPort-in-Linux)
    — **Original unavailable; read the author's
    [current article](https://www.xanthium.in/control-rts-dtr-pins-serial-port-linux-bsd-unix-c-ioctl).**
    It explains individual bit set/clear and inverted converter outputs.
    SerialUART uses logical assertion terminology and preserves unrelated
    modem bits. Access through an appropriate device group is preferable to
    running an entire serial application as root.
16. [Stack Overflow: disable DTR for Arduino](https://stackoverflow.com/questions/30143244/disable-dtr-with-termios-h-for-arduino)
    — **404; unread.** Verified HUPCL and modem ioctl behavior independently.
    Documentation distinguishes close-time hangup suppression from an opening
    pulse and from board-level auto-reset disable circuitry.
17. [GalaxyHunter `set-serial-signal.c`](https://github.com/GuLinux/GalaxyHunter/blob/master/old/set-serial-signal/set-serial-signal.c)
    — **Read legacy source and path history.** Useful HUPCL explanation;
    its inverted command-line convention must not define SerialUART's `on`
    semantics. SerialUART checks native failures and uses individual bit
    operations instead of a whole modem-state read/modify/write. LGPL source
    was not imported.
18. [Xanthium `serial_rts_dtr.c`](https://github.com/xanthium-enterprises/RTS-DTR-Pin-Control-Linux/blob/master/serial_rts_dtr.c)
    — **Read source, README, history and issue titles.** Confirms the
    `TIOCMBIS` / `TIOCMBIC` approach. SerialUART adds checked errors and modem
    readback. Examples cannot establish voltage polarity for every adapter,
    and unsupported PTY modem ioctls must remain unavailable rather than “off.”

### macOS and builds

19. [Stack Overflow: termios configuration on OS X](https://stackoverflow.com/questions/28659045/termios-configuration)
    — **Read question and comments**, then verified the flags against
    [Apple's termios header](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/termios.h).
    Cleared inherited DTR/DSR/DCD flow modes and added exclusive ownership.
    Start bits are inherent in asynchronous UART framing, not another termios
    setting. Application framing requires its own delimiter or length handling.
20. [Todbot Makefile](https://raw.githubusercontent.com/todbot/arduino-serial/master/Makefile)
    — **Read.** Separation of reusable serial objects from the CLI is useful;
    obsolete macOS architecture/deployment flags and optional Mongoose sources
    are not. Added a GNU Make entry point using the existing CMake target graph.
21. [Legacy MSDN NMake page](https://msdn.microsoft.com/en-us/library/f35ctcxw.aspx?f=255&MSPPError=-2147217396)
    — **Unavailable; read the current
    [Microsoft NMake reference](https://learn.microsoft.com/en-us/cpp/build/reference/nmake-reference).**
    Added `Makefile.msvc` and CI coverage from a Visual Studio developer
    environment; NMake and GNU Make are separate entry points.
22. [Stack Overflow: NMake and `cl.exe`](https://stackoverflow.com/questions/45882757/how-to-write-makefilenmake-for-visual-studio-build-tool-cl-exe)
    — **Read.** GCC library flags are not MSVC linker flags. Direct-build
    instructions retain `/I`, `/link`, and native `.lib` dependencies. The
    NMake entry point shares platform linkage with CMake to avoid drift.

### Usage and discovery

23. [Todbot's 2006 Arduino serial article](http://todbot.com/blog/2006/12/06/arduino-serial-c-code-to-talk-to-arduino/)
    — **Read.** Command/response use and board startup time remain relevant.
    SerialUART exposes startup waiting and explicit line endings; it does not
    assume every port opening is a reset or every device replies as text.
24. [Todbot's 2013 update](http://todbot.com/blog/2013/04/29/arduino-serial-updated/)
    — **Read.** Separating the CLI/library and keeping a connection open
    across transactions avoids repeated open-time resets. Added a reusable
    binary `suart_read_until` API for that workflow. Delimiter reads report
    incomplete replies distinctly from completed replies.
25. [Stack Overflow: enumerate serial devices without opening them](https://stackoverflow.com/q/2530096/1454514)
    — **Read question and relevant answers.** Changed Linux discovery to
    sysfs device associations, without probing device handles. USB metadata
    comes from ancestors. Driver-created legacy UART entries can exist without
    attached equipment; listing a port is not proof that hardware is connected.

## Repository review record

These are the revisions inspected, not runtime dependencies. Windows-invalid
filenames in two historical checkouts were read using `git show`; no source
paths were rewritten to make those checkouts appear complete.

| Repository | Revision | Material inspected |
|---|---|---|
| [linux-serial-test](https://github.com/cbrake/linux-serial-test/tree/6a28493a29f7b941172bc58cf4fdabbf8109022f) | `6a28493` | README, source setup, read/write loops, stats, cleanup, speed/divisor logic, MIT license, recent commits and issue inventory |
| [arduino-serial](https://github.com/todbot/arduino-serial/tree/e5958dc3bfd44a303456ff22da53974424da646d) | `e5958dc` | CLI/library/header/Makefile, MIT license, history and issues |
| [WiringPi](https://github.com/WiringPi/WiringPi/tree/31f49ecdf7865c10483a91994aa51f73a7c656aa) | `31f49ec` | Serial test, serial host implementation, serial-path history and LGPL notice |
| [Xanthium Win32](https://github.com/xanthium-enterprises/serial-port-programming-tutorial-win32-api/tree/87aeed3355dacf18e76bcbb69ff03ab106f02bd6) | `87aeed3` | Source inventory, DCB, open errors, polling/event receive, README and history |
| [Xanthium Linux](https://github.com/xanthium-enterprises/linux-serialport-programming-using-c-termios-api/tree/8660ec86a2b44741d5d5e1fe3ab255695e3e4b52) | `8660ec8` | Receive, bidirectional echo, discovery source, README and history |
| [Xanthium RTS/DTR](https://github.com/xanthium-enterprises/RTS-DTR-Pin-Control-Linux/tree/c59507f13ff9a53ff509195b9928b77708036d35) | `c59507f` | Source, README, history and issue inventory |
| [GalaxyHunter](https://github.com/GuLinux/GalaxyHunter/tree/367a6979a973c81575d58d910a825fc1a6913cbe) | `367a697` | Legacy control-signal source, its path history and licensing notice |
| [Teuniz RS-232](https://gitlab.com/Teuniz/RS-232/-/tree/d74c912c72eebd5538f4c31aded576683ba98a33) | `d74c912` | Native source ownership, cleanup, modem, discard and partial-write paths; website changelog and GPL notice |

### Lessons from failures, not just examples

- `linux-serial-test` [#27](https://github.com/cbrake/linux-serial-test/issues/27)
  reports a stalled `tcdrain` with flow control;
  [#62](https://github.com/cbrake/linux-serial-test/issues/62) reports driver
  close delays. SerialUART bounds its queue wait but does **not** claim that
  every driver's final `close` is bounded. It does not change privileged,
  device-wide `closing_wait` settings as a side effect.
- [#17](https://github.com/cbrake/linux-serial-test/issues/17) motivates modern
  `BOTHER` rates instead of persistent divisor changes.
  [#68](https://github.com/cbrake/linux-serial-test/pull/68) and the setup
  history highlight respecting explicit settings rather than silently carrying
  an old hardware mode into a test. SerialUART does not silently change RS485
  driver configuration; RTS/CTS is not an RS485 direction-control implementation.
- The upstream HUPCL change in
  [#70](https://github.com/cbrake/linux-serial-test/pull/70) is appropriate for
  its diagnostic program. SerialUART deliberately suppresses close-time HUPCL
  for its Arduino use case, with the limitations documented above.
- Todbot [#10](https://github.com/todbot/arduino-serial/issues/10) identifies a
  terminator-buffer overflow; [#13](https://github.com/todbot/arduino-serial/issues/13)
  discusses nonblocking reads; [#14](https://github.com/todbot/arduino-serial/issues/14)
  discusses arbitrary sleeps around flushing. SerialUART has no implicit
  two-second flush, handles `EAGAIN`, reports byte counts, and keeps destructive
  discard separate from waiting for transmission.

## Native authorities used to check the conclusions

- [Linux termios](https://man7.org/linux/man-pages/man3/termios.3.html),
  [termios2 and its incompatible struct layout](https://man7.org/linux/man-pages/man2/TCGETS.2const.html),
  [exclusive terminal mode](https://man7.org/linux/man-pages/man2/TIOCEXCL.2const.html),
  and [modem signals / driver counters](https://man7.org/linux/man-pages/man2/TIOCMGET.2const.html).
- Microsoft [PurgeComm](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-purgecomm)
  and [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers):
  discard destroys bytes; the synchronous drain may ignore write timeouts.
- [Raspberry Pi UART documentation](https://www.raspberrypi.com/documentation/computers/configuration.html#configuring-uarts):
  routing, Bluetooth sharing, login-console ownership and model-specific names
  must be checked on the actual Pi.

Features outside this change include a portable RS485 policy, mark/space parity,
platform-specific custom baud on macOS, and exhaustive driver-counter support
on Windows/macOS. Their absence is explicit; no unsupported native operation
is emulated as a successful hardware result.
