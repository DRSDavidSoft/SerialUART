# C API and bindings

Include `SerialUART.h` and link `SerialUART::serialuart`. The header exposes no
platform handles or OS headers and has C linkage when included from C++.
The serial library has no dependency on the CLI or network examples.

```c
#include <SerialUART.h>
#include <stdio.h>

int main(void) {
    suart_config config;
    suart_port *port = NULL;
    unsigned char reply[256];
    const unsigned char command[] = {'S','T','A','T','U','S','\r','\n'};
    size_t sent = 0, received = 0;
    int result;
    suart_config_init(&config);
    config.baud = 115200;
    config.dtr = SUART_DTR_OFF;
    result = suart_open(&port, "COM12", &config); /* or /dev/ttyACM0 */
    if (result != SUART_OK) return 1;
    result = suart_write(port, command, sizeof(command), &sent, 1000);
    if (result == SUART_OK && sent == sizeof(command)) {
        result = suart_read(port, reply, sizeof(reply), &received, 1000);
        if (result == SUART_OK) fwrite(reply, 1, received, stdout);
    }
    suart_close(port);
    return result != SUART_OK || sent != sizeof(command);
}
```

## Contracts

| API | Contract |
|---|---|
| `suart_config_init` | 57600 baud, 8N1, no hardware flow control, DTR keep, no boot wait |
| `suart_enumerate` | Synchronous callback; nonzero stops; no ports opened |
| `suart_resolve` | Exact AND filters; no match → NOT_FOUND; >1 → AMBIGUOUS |
| `suart_open` | Creates a handle or sets output to NULL; validates configuration |
| `suart_read` / `suart_write` | Binary buffers with actual byte counts; OK + zero bytes is timeout |
| `suart_set_dtr` / `suart_set_rts` | Manual line control; do not manually set RTS while RTS/CTS is enabled |
| `suart_send_break` | Bounded 0..60000 ms break |
| `suart_close` | Releases handle; NULL is safe |
| `suart_native_error` | Last native error on an existing handle after a failed operation |

All buffer sizes are in bytes. Read/write lengths must fit in `INT_MAX`; timeouts
are 0..60000 ms. A write may accept only part of a buffer: advance by the actual
count and send the remainder within your own deadline. A native error can mean
delivery is uncertain, so reconnect must not blindly replay a command. A timeout
is not proof that a target executed or rejected an application command.

Return statuses: `OK=0`, `ERROR=-1`, `INVALID=-2`, `NOT_FOUND=-3`,
`AMBIGUOUS=-4`, `UNSUPPORTED=-5`, `TIMEOUT=-6`, `BUFFER_FULL=-7`.
Windows asks its driver to support the requested numeric baud. POSIX supports
the available native speed constants; Linux additionally uses `termios2/BOTHER`
for custom numeric rates and verifies driver readback. macOS custom rates outside
the supported constants return UNSUPPORTED. No silent baud fallback is performed.
Driver readback does not measure oscillator accuracy or USB adapter timing.

Use `suart_open_ex(&port, path, &config, &native_error)` to retain a failed open's
native error after cleanup. The original `suart_open` API remains available.
Invalid library arguments leave `native_error` zero. The existing configuration
struct layout and major library ABI are unchanged in 2.1.

Version 2.2 also accepts `parity='M'` (mark) and `'S'` (space) on supported
Windows/Linux drivers. Unsupported native modes return UNSUPPORTED before open.
`stop_bits=SUART_STOP_ONE_POINT_FIVE` (integer 15) requests 1.5 stop bits on
Windows with five data bits; other data widths are INVALID and POSIX returns
UNSUPPORTED. The existing field layout remains unchanged. Driver acceptance
and readback do not replace a physical framing/timing measurement.

Profiles, logs, rendering, board aliases, automatic selection policy and terminal
editing belong to the executable's host modules. They do not change library
read/write bytes or add a dependency to the C ABI.

## Queues, framing and diagnostics

| API | Meaning |
|---|---|
| `suart_wait_tx(port, timeout_ms)` | Wait for the driver's TX queue to empty; returns TIMEOUT at the deadline |
| `suart_discard(port, SUART_RX \| SUART_TX)` | Explicitly discard pending input/output; select either queue or both |
| `suart_read_until(port, bytes, capacity, &actual, terminator, timeout_ms)` | Include the terminator, append no NUL, use a total deadline, never read past it |
| `suart_get_config(port, &config)` | Read driver baud/framing/RTS-CTS; open policies return KEEP and zero boot wait |
| `suart_get_modem(port, &modem)` | Driver logical assertions with separate `known` and `asserted` bitmasks |
| `suart_get_counters(port, &counters)` | Optional Linux TIOCGICOUNT totals; unsupported OSes/drivers return UNSUPPORTED |

A successful write means bytes were accepted. An empty driver queue does not
prove the last bit left the shift register, an adapter transmitted its USB
buffer, or the target executed a command. `wait_tx` avoids unbounded
`tcdrain`/`FlushFileBuffers`; it never discards data to claim completion. The
final native close may still incur driver-specific delays. `suart_close` itself
does not promise a transmit drain or application acknowledgement.

`read_until` returns TIMEOUT for a missing terminator and BUFFER_FULL when the
capacity is exhausted first. Both retain the actual partial bytes. A terminator
in the last available position is successful. Zero capacity is invalid; zero
timeout makes one nonblocking read attempt. Reuse the same open handle across
transactions to avoid repeated opening and reset behavior:

```c
unsigned char reply[256];
size_t actual = 0;
int status = suart_read_until(port, reply, sizeof(reply), &actual, '\n', 2000);
/* Process exactly actual bytes; they may include NUL.
 * Only SUART_OK proves this read encountered its terminator.
 * Send the next command on port without closing/reopening it. */
```

Modem bits are CTS=1, DSR=2, DCD=4, RI=8, DTR=16 and RTS=32. Test `known`
before interpreting `asserted`. Windows reports input modem lines; its DTR/RTS
output assertions are unavailable through this API. PTY modem operations usually
fail. Logical assertion does not specify pin voltage, especially on inverted
USB converters. Manual RTS changes are invalid while RTS/CTS owns the line.

Linux counters are driver totals, may include other sessions, and wrap at 32
bits. Compute deltas with unsigned arithmetic. They are distinct from application
byte counts; support and update semantics depend on the driver.

POSIX opens use `flock` and `TIOCEXCL`. This excludes cooperating processes and
future unprivileged opens; it cannot revoke existing descriptors or prevent all
privileged/noncooperating access. Cleanup releases ownership on every failed
open. Saved termios is restored on close except HUPCL stays cleared to avoid
introducing a close-time hangup. Linux saves/restores termios2 when available,
including custom rates; restore failures cannot be reported by the void close API.

Separate handles have independent state. Serialize operations on the **same**
handle; never close it while another thread is using it. Callbacks and buffers
are borrowed only for the duration of a call. The library allocates a handle
on open and frees it on close. It is a hosted C library using libc and native
OS services, rather than an embedded/freestanding firmware driver.

## Device identity

```c
suart_selector selector = {0};
suart_device device;
selector.vid = 0x2341;
selector.pid = 0x0043;
selector.serial = "your-board-serial";
if (suart_resolve(&selector, &device) == SUART_OK) {
    /* suart_open(..., device.path, ...); */
}
```

Strings are UTF-8; friendly names are exact and case-sensitive. A path-only
selector passes through to the OS and supports PTYs and `/dev/serial/by-id`
symlinks. Metadata is available only when the OS/driver exposes it. VID/PID
identify a product, not necessarily an individual board; use a serial filter
when multiple devices share the product IDs. On Windows generated instance IDs
are not reported as hardware serial numbers.

With additional metadata filters, POSIX paths are compared after `realpath`
resolution; Windows COM paths compare case-insensitively with an optional
`\\.\` prefix. Linux uses sysfs device associations rather than a hardcoded
port-name whitelist. Enumeration never opens device nodes to verify an attached
UART, so legacy driver placeholders may also be listed.

## C++ ownership

```cpp
#include <SerialUART.h>
#include <memory>
using SerialPort = std::unique_ptr<suart_port, decltype(&suart_close)>;
// After successful open:
// SerialPort owner(raw_handle, &suart_close);
```

For FFI, prefer the installed shared library. Match the header's C `unsigned`,
`size_t`, enums and struct layout exactly; initialize configuration through
`suart_config_init`. Opaque handles and byte buffers work with ctypes, cffi,
Rust `extern "C"`, .NET P/Invoke, JNI, and other native bindings. No automatic
binding generator or runtime is required by SerialUART itself.
