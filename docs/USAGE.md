# Terminal, commands and pipelines

`SendCommand --help` is the complete option reference. Selection options combine:

```sh
SendCommand --list
SendCommand --port COM12 --baud 115200 --terminal
SendCommand --name "USB Serial Device (COM12)" --terminal
SendCommand --vid 2341 --pid 0043 --serial YOUR_SERIAL --terminal --reconnect 1000
```

VID and PID are hexadecimal (with or without `0x`). Friendly names, paths and
USB serials are exact matches. No selection filters means exactly one enumerated
device must exist. Do not rely on that on a machine with multiple serial ports.
`--list` prints tab-separated path, friendly name, VID:PID and serial fields.

## One-shot commands

```sh
SendCommand -p COM12 -b 115200 --send STATUS
SendCommand -p /dev/ttyUSB0 --hex "53 54 41 54 55 53 0D 0A" --timeout 2000
```

Text is sent exactly as supplied; no newline is added. Hex input accepts pairs
of digits separated by spaces, tabs or colons, including `00` and `FF`. Replies
are written as raw bytes. After a command, the process exits once no new serial
bytes arrive for `--timeout` milliseconds (default 1000). Any response yields
exit 0 after idle; no response yields 124. This is an idle deadline, so a device
that sends continuously can keep a command session open. The caller can impose
an overall deadline when needed.

## Binary pipes

Unix shell:

```sh
cat request.bin | SendCommand -p /dev/ttyACM0 -b 115200 --timeout 500 > reply.bin
SendCommand -p /dev/ttyUSB0 --receive > capture.bin
```

Windows `cmd.exe` (binary redirection):

```bat
SendCommand.exe -p COM12 --timeout 500 < request.bin > reply.bin
```

The default mode concurrently reads stdin and the serial port. Stdin EOF starts
the receive idle deadline. `--receive` ignores stdin and runs until interrupted.
Status is written only to stderr; `--quiet` suppresses connection status.
Old PowerShell versions can transform native pipeline bytes, so use `cmd.exe`
redirection for binary files or a byte-preserving process API.

## Interactive terminal

```sh
SendCommand -p /dev/ttyACM0 -b 115200 --terminal --reconnect 1000
```

Local echo and line buffering are disabled. Serial output is displayed as-is.
The device's line endings and ANSI sequences determine the display. On POSIX,
terminal input bytes pass through unchanged apart from the escape prefix.
Windows console input encodes characters as UTF-8 and maps common navigation
keys to ANSI sequences. To send arbitrary non-UTF-8 bytes, use pipes/hex mode.

| Keys | Action |
|---|---|
| Ctrl+] then `?` | Show shortcuts on stderr |
| Ctrl+] then `q` | Quit and restore terminal |
| Ctrl+] then `b` | Send a 250 ms serial break |
| Ctrl+] then `d` | Pulse DTR low 100 ms, then high |
| Ctrl+] then `]` | Send a literal Ctrl+] byte |
| Ctrl+C | Send byte 03 to the device in terminal mode |

Unrecognized prefix commands are consumed. In pipe mode **all** bytes are data;
no shortcut parser runs. Outside raw terminal mode, Ctrl+C/SIGINT stops normally.
Normal exits and SIGINT/SIGTERM restore terminal input settings; forced process
termination cannot run cleanup. POSIX users can recover with `stty sane`.

## Arduino and DTR

```sh
# Deliberately reset and wait for bootloader/application startup:
SendCommand -p COM12 -b 115200 --dtr reset --boot-wait 2000 --send STATUS

# Avoid intentional reset pulses; keep control low:
SendCommand -p /dev/ttyACM0 -b 115200 --dtr off --terminal
```

`keep` leaves the driver's current DTR policy alone; `off` requests low; `on`
requests high; `reset` requests low for 100 ms then high. Boot wait applies after
every successful open. Reset therefore also occurs after reconnect if selected.
The application does not flush incoming startup bytes silently.

**A software flag cannot guarantee no reset on every board.** Some OS drivers
toggle DTR while opening, before userspace can change it. USB CDC firmware and
boards vary. POSIX disables HUPCL to avoid a close-triggered hangup, but initial
driver pulses may still occur. Boards needing a strict no-reset guarantee may
require board-specific auto-reset disable hardware. Test the actual board and
driver. PTYs cannot verify DTR, RTS or electrical timing.

The [Arduino echo sketch](../examples/arduino_echo/arduino_echo.ino) is useful for
binary transfer checks. Select its configured 115200 baud and allow for startup.

## Reconnect and errors

`--reconnect MS` retries initial absence and serial read/disconnect failures,
re-running all selection filters before each open. Until connected, stdin is
not consumed. There is no unbounded outgoing queue. An ambiguous selection stops
immediately. Completed commands and EOF sessions do not reopen and replay.
Write errors stop with exit 1 because delivery is uncertain. A network peer
disconnect leaves a listener available for the next peer; TCP client connection
errors end the process. `--reconnect` concerns the serial device only.

Exit codes: **0** normal, **1** I/O/configuration failure, **2** usage or ambiguous
selection, **124** command received no reply. OS errors on an existing serial
handle are included in read-error status messages.

## Migrating from v1.3

Version 2 is an API/CLI redesign. Compile library sources separately rather than
including `.c` files. Replace numeric COM selection with `--port COM12` and
`--command` with `--send`. `--baud` replaces `--baudrate`. `--hex` now accepts
an explicit hex byte string instead of enabling backslash substitutions.
Timing options use **milliseconds**, not the prototype's seconds. Removed scan,
debug, verify-only and slash-prefixed prototype flags are rejected; the program
does not scan/open arbitrary ports looking for the highest COM number.
