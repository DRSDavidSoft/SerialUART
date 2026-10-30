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
`AMBIGUOUS=-4`, `UNSUPPORTED=-5`. A baud rate outside the POSIX backend's
supported constants returns UNSUPPORTED; Windows asks the driver to support
the configured rate. No silent baud fallback is performed. Open-time native
errors are not retained after the failed handle is released.

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
