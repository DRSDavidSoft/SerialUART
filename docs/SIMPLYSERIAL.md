# SimplySerial features in SerialUART

SerialUART 2.2 ports the useful terminal/session capabilities of
[SimplySerial](https://github.com/fasteddy516/SimplySerial), reviewed at
[`8e0f4bc`](https://github.com/fasteddy516/SimplySerial/tree/8e0f4bc750609118a34d7ecdfb19b4cc951102f1).
These features run in native C on Windows, Linux, Raspberry Pi OS and macOS.
The serial library remains separate from the terminal, profile and board-catalog
modules. No .NET, JSON library, Python runtime, or network downloader is required.

## Feature map

| SimplySerial capability | SerialUART equivalent |
|---|---|
| Port list and excluded devices | `--list`, `--list-all`; native name, USB identity, recognized model and availability |
| Board recognition and custom models | Offline 472-entry catalog; `--board VID:PID=NAME`, `--board-name NAME`, `--list-boards` |
| Include/exclude filters | Repeatable `--include`, `--exclude`; exact/contains rules with multiple fields |
| Disabling board preference | `--prefer-boards off` |
| Preventing a particular connection | `--block`; remains enforced for explicit manual ports |
| NONE / ONE / ANY auto-connect | `--autoconnect none|one|any`; busy-port fallback and identity pinning |
| Logging and append/overwrite | `--log`, `--log-mode append|overwrite`, raw or display capture |
| Slow-fill logging | Flush after each receive block; final close errors are reported |
| Log timestamps requested upstream | `--log-time` with display logs, UTC line prefixes |
| UTF-8 / ASCII / RAW display | `--display utf8|ascii|raw`; default output remains byte-for-byte |
| Forced newline | `--force-newline on` replaces received CR bytes with LF |
| Device title/status updates | Default pass-through; `--status off` strips OSC 0/2 across read boundaries |
| Fixed window title | `--title TEXT`, restored on exit; implies status suppression unless overridden later |
| Clear on connection | `--clear-screen on`; prefix `Ctrl+] c` clears on demand |
| Local echo | `--echo on`; prefix `Ctrl+] e` toggles it |
| Bulk paste | `--bulk-send on` (default); Windows batches queued console records and honors key repeats |
| Exit key | `--exit-key X` reserves Ctrl+X in terminal mode; prefix `Ctrl+] q` remains available |
| Enter mapping | `--enter cr|lf|crlf|text:TEXT`, `--enter-hex HEX`, or default `preserve` |
| True line-send proposal | `--line-buffer on`, with UTF-8 backspace editing and bounded pending input |
| Saved settings | `--config FILE`; optional global/project layers through `--profiles` |
| Inspecting settings | `--list-settings`, `--list-filters`, `--list-boards` |
| Navigation/function keys | Windows Insert/Delete/Home/End/PageUp/PageDown, arrows and F1–F12; POSIX terminal sequences pass through |
| Additional framing | Mark/space parity on supported Windows/Linux drivers; Windows 5-bit / 1.5-stop-bit framing |
| Terminal integration | Run SendCommand directly in VS Code or a Windows Terminal profile |
| Board-data refresh | Reviewed offline import and rebuild, with retained license and source checksum |

CLI syntax is intentionally unambiguous: use the documented long options.
SimplySerial's abbreviated `-name:value` arguments, `.simplyserial` directory,
and JSON filter/custom-board files are not parsed by SerialUART. Its .NET/MSI
installation and automatic HTTPS catalog updater are not runtime dependencies;
SerialUART provides native install prefixes, ZIP packages and the offline refresh
workflow below. Its zero-stop-bit / four-data-bit inputs are not valid native
SerialUART framing requests.

## A CircuitPython-style terminal

```sh
SendCommand --config examples/profiles/circuitpython.cfg
# Limit automatic discovery to a particular USB vendor if needed:
SendCommand --config examples/profiles/circuitpython.cfg --vid 2E8A
```

The profile selects 115200 baud, terminal mode, ONE auto-connect, bulk input,
and Ctrl+X exit. Ctrl+C/Ctrl+D are forwarded to the board's REPL. Catalog
recognition identifies a model or capability; it does not prove CircuitPython
is running or determine the current firmware. An unknown board is still usable
by its native path or USB identity.

Default SerialUART behavior is preserved: no implicit auto-connect, local echo,
screen clear, profile loading, log file, newline conversion or display conversion.
The existing `--reconnect MS` continues to re-resolve its explicit selectors.
`--autoconnect one|any` opts into choosing the first eligible device, with
recognized boards preferred. `--prefer-boards off` uses path ordering instead.
Windows COM numbers sort numerically. Unknown serial rates are not guessed;
choose the baud explicitly or use a profile.

ONE tries eligible ports until an initial open succeeds, then pins the available
USB serial/VID/PID identity, preserving the original selectors. If no serial is
available it pins the original port path and available USB IDs. A renumbered
board with a unique serial can reconnect; duplicate identities remain ambiguous.
ANY may switch to another eligible device after disconnect. Filters are reapplied
on discovery. Neither mode replays bytes whose delivery is uncertain. Pending
unsubmitted lines are discarded on disconnect; offline keyboard input is not
queued for a later device. Exit shortcuts remain active while waiting.

## AT commands, custom Enter and line editing

```sh
SendCommand -p COM12 --config examples/profiles/at-commands.cfg
SendCommand -p /dev/ttyUSB0 --terminal --line-buffer on --echo on --enter crlf
SendCommand -p /dev/ttyUSB0 --terminal --enter 'text:MiXeD Case'
SendCommand -p COM12 --terminal --enter-hex '00 FF 0D 0A'
```

Without line buffering, input is sent promptly. With line buffering, bytes stay
local until Enter, then the complete line and its configured ending are submitted
together. This differs from merely draining queued keyboard events faster.
Backspace/Delete removes the last buffered UTF-8 code point; terminal glyph width
can still vary for wide/combining characters. A CRLF pair submits one line when
editing or Enter mapping is enabled. The line limit is 4096 bytes and the custom
ending limit is 256 bytes; overflow fails instead of sending a truncated command.
Exit and disconnect discard unsubmitted input.

Custom text preserves case. Hex endings support NUL/high bytes without text
encoding. Defaults preserve input bytes, and terminal controls do not run in
ordinary binary pipe mode. `--bulk-send off` processes one input byte/key event
per loop; use the default bulk mode for large pastes. Board firmware can impose
its own pacing or command/response requirements.

## Logs and display

```sh
# Exact received bytes, appended to the existing file:
SendCommand -p COM12 --receive --log capture.bin
# Explicitly replace the previous capture:
SendCommand -p /dev/ttyUSB0 --receive --log capture.bin --log-mode overwrite
# Inspect control bytes while retaining an exact raw capture:
SendCommand -p /dev/ttyUSB0 --receive --display raw --log capture.bin
# Human-readable log of displayed data with UTC timestamps:
SendCommand -p COM12 --terminal --log session.txt --log-format display --log-time
```

Raw logs contain RX bytes only, including NUL and escape sequences. They exclude
application banners and local echo. Display logs contain the rendered RX and
local echo. Application messages stay on stderr. Logging defaults to append;
overwrite requires an explicit option. Each receive block is flushed to the OS
and failures stop the session; this is not a power-loss durability guarantee.

`utf8` passes bytes unchanged for the terminal's decoder. `ascii` replaces each
non-ASCII byte with `?`. `raw` displays control/high bytes as `[XX]`, retaining
backspace, tab, CR and LF. `--force-newline on` performs CR-to-LF replacement;
an existing CRLF therefore becomes two LF bytes. These are explicit presentation
choices and do not change an accompanying raw log.

`--status off` filters OSC 0/2 window-title sequences terminated by BEL or ST,
even when split across reads. Other ANSI/OSC sequences pass through; this is not
a general terminal sanitizer. `--title` also suppresses device title changes,
unless a later `--status on` overrides it. Screen/title commands are emitted only
to an actual terminal, and saved window/console state is restored on normal exit.
Network bridges support raw RX capture; display transforms/display logs are
rejected there so they cannot alter the network protocol.

## Profile layers

Profile files contain **one long option per line**, optionally followed by a
value. Both `--baud 115200` and `--baud=115200` work. Outer single/double quotes
may wrap a value containing spaces; there is no shell expansion or backslash
unescaping. Blank lines and whole-line `#` comments are ignored. UTF-8 BOM and
CRLF files are accepted. Actions such as `--send`, `--listen`, `--help`, and
nested `--config` are forbidden inside profiles.

`--config FILE` loads explicit profiles in their command-line order. With
`--profiles`, layers are applied in this order:

1. Windows `%APPDATA%/SerialUART/settings.cfg`, or Unix
   `$XDG_CONFIG_HOME/serialuart/settings.cfg` (fallback
   `$HOME/.config/serialuart/settings.cfg`).
2. `.serialuart/settings.cfg` in the current directory.
3. Explicit `--config` files.
4. Actual CLI settings.

Later scalar settings win; repeated filter/board entries accumulate. Conflicting
mode flags remain errors. Missing optional default files are ignored; malformed
or unreadable files fail. Explicit files must exist. Limits are 8192 bytes per
profile line, 1 MiB per file, 1024 expanded arguments and 1 MiB of argument text.
`--list-settings` shows application order without opening a serial port.

## Board aliases and device filters

```sh
SendCommand --board '1234:ABCD=Lab Controller' --list
SendCommand --board '1234:ABCD=Lab Controller' --board-name 'Lab Controller' --terminal
SendCommand --autoconnect one --terminal --include 'vid=239A' --exclude 'name~bluetooth'
SendCommand --autoconnect any --terminal --include 'vid=2341,pid=0043' --block 'serial=DO-NOT-OPEN'
SendCommand --include 'vid=239A' --list-all
```

Fields are `port`, native `name`, `serial`, `vid`, `pid`, and recognized `board`.
`=` means exact matching; `~` means substring matching with ASCII case folding.
USB IDs normalize to four uppercase hexadecimal digits for exact comparisons.
Comma-separated predicates within one rule all must match. Include rules combine
with OR; when any exist, automatic selection starts with only their matches.
Exclusions then remove matches. A `*` predicate is ignored; empty all-wildcard
rules are rejected. There are at most 64 rules, 64 custom aliases and 256 discovery
candidates. Selection fails rather than silently truncating an oversized result.

Include/exclude controls automatic discovery and ordinary listings. An explicit
manual selection can use an excluded device; a block rule remains enforced.
`--list-all` shows excluded/blocked entries without opening them. Rules use only
metadata exposed by the OS; unknown fields cannot supply an identity guarantee.
Custom names supplement the OS name and do not rewrite it. USB product IDs alone
are model hints; combine serial and path constraints for individual hardware.

## Refreshing the offline catalog

The snapshot is [data/simplyserial-boards.json](../data/simplyserial-boards.json),
from the reviewed SimplySerial revision. Its 472 board entries are compiled
into the CLI. The original [MIT notice](../data/SIMPLYSERIAL-LICENSE.txt) is retained
and installed in every package; it does not assign a license to unrelated
SerialUART source.

After obtaining and reviewing a replacement local JSON file:

```sh
python tools/import_boards.py path/to/reviewed-boards.json --output cli/boards.inc
# Replace data/simplyserial-boards.json with that reviewed input as well.
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The importer checks sizes, IDs and names, and records a source SHA-256. Keep the
snapshot and generated file together and retain the license. Git pins both data
files to LF for reproducible cross-platform checksums. Python is a maintenance
tool only; an installed executable needs no JSON parser or update service.
Simple local names can instead be overridden immediately using `--board` in a
profile, with no rebuild.

## VS Code and Windows Terminal

Open a normal integrated terminal in VS Code and run a profile command above.
For a Windows Terminal profile, use an installed native executable:

```json
{
  "name": "SerialUART · Lab board",
  "commandline": "C:\\Tools\\SerialUART\\bin\\SendCommand.exe -p COM12 --terminal --exit-key X"
}
```

This requires no VS Code extension or .NET host. Install paths and PATH setup
remain under your control; see the [build/install guide](BUILDING.md).

## Source and validation notes

The review covered `SimplySerial.cs`, `Arguments.cs`, `Boards.cs`, `ComPorts.cs`,
`Filters.cs`, the board snapshot, README, MIT license and relevant history.
The multi-instance auto-connect fix `b8b74b2` informed busy-port fallback.
[Issue #11](https://github.com/fasteddy516/SimplySerial/issues/11) informed prompt
log flushing, [#36](https://github.com/fasteddy516/SimplySerial/issues/36) bulk
input, and [#50](https://github.com/fasteddy516/SimplySerial/issues/50) highlighted
removal races. Native discovery does not depend on WMI. The pending
[line-buffer PR #57](https://github.com/fasteddy516/SimplySerial/pull/57) informed
the Enter-driven editor; it is not claimed to be merged upstream.

Automated tests exercise binary/Unicode/escape boundaries, raw/display logs,
flushing, profile precedence and failures, editor overflow, byte endings,
catalog reproducibility, blocked/busy candidates and pinned identities. POSIX
PTY tests exercise the real terminal lifecycle. MSVC/MinGW encoder tests cover
UTF-16 pairs and key sequences; they do not replace physical keyboard/console,
USB removal, CircuitPython REPL or modem timing acceptance in
[the hardware checklist](VALIDATION.md).
