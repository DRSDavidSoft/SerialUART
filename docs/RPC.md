# SerialUART control API

Run the full application as a local service. No serial port opens until a client
calls `port.open`. The mini `SendCommand` has no API server.

```sh
SerialUART --api stdio
SerialUART --api tcp --api-port 8765
SerialUART --api websocket --api-port 8765
```

| Transport | Endpoint | Framing |
|---|---|---|
| stdio IPC | Child process stdin/stdout | UTF-8 NDJSON, one compact JSON value plus LF |
| TCP | `127.0.0.1:8765` | Same NDJSON stream; CRLF accepted |
| WebSocket | `ws://127.0.0.1:8765/rpc` | Text messages containing compact JSON, optionally LF terminated |

This is [JSON-RPC 2.0](https://www.jsonrpc.org/specification) with named object
parameters. Stdio and TCP use [NDJSON](https://github.com/ndjson/ndjson-spec).
Empty lines are ignored. WebSocket messages may contain multiple NDJSON lines;
the end of each complete text message supplies a missing final LF. Use compact
JSON: formatting a request across physical lines is not supported. Binary
WebSocket messages are rejected. Fragmented text messages and ping/pong work.

Responses go only to the protocol transport; diagnostics go to stderr. Always
drain both child-process pipes. The UI and RPC are separate application modes;
they do not simultaneously share a port. There is no HTTP REST endpoint or native
named-pipe/Unix-socket listener in this version. Stdio supplies portable local IPC.

## First request

```json
{"jsonrpc":"2.0","id":1,"method":"system.info"}
```

```json
{"jsonrpc":"2.0","id":1,"result":{"application":"SerialUART","version":"2.4.0","api_version":1,"request_bytes":16384,"transfer_bytes":4096,"transports":["stdio","tcp","websocket"],"discovery_source":"Windows present SetupAPI Ports / Configuration Manager"}}
```

The discovery source is platform-specific. IDs are echoed unchanged. String,
number and null IDs are accepted; use unique string or integer IDs for clients.
Omit `id` for notifications, which receive no response. Batches of 1–16 requests
are accepted and executed sequentially; they are not atomic transactions.
Unknown method/parameter names return structured errors. Positional parameters
are unsupported. Repeated object keys, including escaped aliases, are rejected.

Run the dependency-free Python example to inspect devices without opening one:

```sh
python examples/rpc_client.py --executable ./build/SerialUART
python examples/rpc_client.py --executable C:/Tools/SerialUART/bin/SerialUART.exe
```

## Methods

All parameters are optional unless marked **required**. `{}` may be omitted.
Unsigned integer parameters use plain decimal JSON integers, not strings,
fractions or exponent notation. Defaults are shown below.

| Method | Parameters | Result |
|---|---|---|
| `system.info` | none | Application/version, limits and discovery source |
| `devices.list` | `offset: 0`, `limit: 16` (1–16), `match` | `devices`, `total`, `offset`, `next_offset` or null |
| `port.open` | Selectors and settings below | Selected `path` and requested `configuration` |
| `port.status` | none | `connected`, `subscribed`, `rx_bytes`, `tx_bytes`, `path`, driver `configuration` or null |
| `port.close` | none | true; idempotent, cancels subscription |
| `port.write` | **`data`**: base64, `timeout_ms: 1000` (1–1000) | `written` byte count |
| `port.read` | `max_bytes: 4096` (1–4096), `timeout_ms: 0` (0–1000) | Base64 `data`, `encoding`, `bytes`; zero bytes on timeout |
| `port.subscribe` | `enabled: true` | New subscription state |
| `port.signals` | none | `known` and `asserted` modem bit masks |
| `port.set_signal` | **`line`**: `dtr` or `rts`, **`asserted`**: boolean | true |
| `port.break` | `duration_ms: 250` (1–1000) | true |
| `port.discard` | `receive: false`, `transmit: false`; at least one true | true |
| `server.shutdown` | none | true, then server exits and closes serial port |

Device records include `path`, `name`, `serial`, `manufacturer`, `product`,
`instance_id`, `location`, numeric `vid`/`pid`, `is_usb`, and nullable
`problem_code`. Missing strings are empty. Pagination enumerates current devices
each time; hotplug can change results between pages. Listing never opens ports.

`port.open` accepts `path`, `name`, `serial`, `instance_id`, `match`, `vid`, `pid`,
`baud`, `data_bits`, `parity`, `stop_bits`, `rts_cts`, `dtr`, and `boot_wait_ms`.
VID/PID are decimal JSON numbers (for example 9025 = USB VID 2341 hex).
Selectors/settings start with command-line defaults; supplied fields replace
their corresponding defaults. Selectors combine with AND, and ambiguity fails.
Listing obeys include/exclude/block and board-name rules. Opens follow the CLI's
selection policy: explicit manual selection can override include/exclude rules,
but never block rules; extended instance/match selection also enforces exclusions.
These filters are convenience rules, not an authentication boundary. `devices.list.match`
replaces the default match for that enumeration.

Default configuration is 57600 baud, 8N1, no RTS/CTS and DTR `keep`.
Baud is 1–4000000, data bits 5–8, parity `N/E/O/M/S`, stop bits `1/1.5/2`,
and DTR `keep/off/on/reset`. Support for a configuration depends on the driver;
unsupported combinations return a native error. Boot wait is 0–10000 ms.
An existing open port must be closed before opening/reconfiguring. Opening may
change DTR at driver level even with `keep`; see [Arduino and DTR](USAGE.md#arduino-and-dtr).

```json
{"jsonrpc":"2.0","id":2,"method":"port.open","params":{"path":"COM12","baud":115200,"dtr":"keep"}}
{"jsonrpc":"2.0","id":3,"method":"port.write","params":{"data":"U1RBVFVTDQo="}}
{"jsonrpc":"2.0","id":4,"method":"port.read","params":{"timeout_ms":1000}}
{"jsonrpc":"2.0","id":5,"method":"port.close"}
```

The write above sends `STATUS\r\n`. Base64 preserves all byte values, including
NUL; a transfer is at most 4096 decoded bytes. `written` means accepted by the
OS serial driver, not acknowledged by the device. Partial/failed writes close
the port and return the accepted byte count with `delivery_uncertain: true`.
The server never replays bytes. Clients must decide whether retrying is safe.

Modem masks use the C API's bits: CTS=1, DSR=2, DCD=4, RI=8, DTR=16, RTS=32.
Only bits present in `known` have reliable state. Native errors retain numeric
SerialUART `status` and OS `native` codes; see [the C API](API.md).

## Receive events and ownership

Subscribe after opening. Incoming serial bytes then produce notifications:

```json
{"jsonrpc":"2.0","method":"port.received","params":{"data":"AP8=","encoding":"base64","bytes":2}}
```

Chunks are arbitrary, not device-message boundaries. Accumulate and frame them
in your client. While subscribed, `port.read` is rejected to avoid competing
consumers. `port.subscribe` with `enabled: false` restores explicit reads.
Read failures close the port and emit `port.disconnected`. Unplug detection
depends on the OS/driver and a read attempt. A disconnected serial session is
not automatically reopened by RPC; call `port.open` explicitly after discovery.
The interactive application still supports its existing automatic reconnect.

Only one network client owns a server at a time. Client loss closes the serial
port, cancels events and discards any incomplete request; a new client starts
disconnected with zero byte counters. Stdio EOF and server termination also close the port. A request
already executing when a connection is lost may finish; transport loss cannot
undo serial writes. Capture logs record received serial bytes, never RPC JSON.

## WebSocket browser example

Serve the supplied page locally and allow its exact Origin:

```sh
python -m http.server 8000 --bind 127.0.0.1 --directory examples
SerialUART --api websocket --api-port 8765 --origin http://127.0.0.1:8000
```

Open `http://127.0.0.1:8000/rpc-client.html`. Connect, inspect devices, then enter
an explicit `port.open` request if desired. The page never opens a port on load.
Browser local-network policies may require permission for a local WebSocket.

All API listeners bind IPv4 loopback only. By default browser Origins are
rejected; `--origin` allows one exact Origin. This is not authentication: local
native processes can connect without Origin. The service has no TLS or remote
authentication and grants serial access under the launching user's permissions.
Use stdio for a parent-owned service; do not expose the listener through a remote
proxy without separate authentication and transport protection.

## Limits and errors

Each request line is at most 16384 bytes, nesting is at most 16 levels, and the
parser allows 1024 tokens and 128 decoded bytes per object key. IDs are at most
256 encoded bytes. Responses are bounded to 64 KiB; excessively large batches
can produce a response-capacity error after their operations have executed.
Split large discovery batches into individual paginated requests. Malformed or
oversized NDJSON is discarded through the next LF, then parsing resumes.
Native metadata with invalid UTF-8 is replaced with U+FFFD in RPC JSON.

Network sends have a 2-second deadline; serial read/write calls allow at most
1 second, and opening with boot wait can take 10 seconds plus native driver/reset
time. These are not hard real-time guarantees. Stdio clients must continuously
drain stdout to avoid pipe backpressure. No received serial data is retained for
clients after disconnect. The service runs synchronously and does not support
request cancellation or simultaneous UI access.

| Code | Meaning |
|---|---|
| -32700 | Invalid JSON/UTF-8, duplicate keys, or parser/input limit |
| -32600 | Invalid request envelope or batch |
| -32601 | Unknown method |
| -32602 | Invalid/unknown parameters |
| -32603 | Response capacity exceeded |
| -32000 | Native serial error, with status/native data |
| -32001 | A port is already open |
| -32002 | No open port |
| -32003 | Explicit read while subscribed |
| -32004 | Capture log failed |
| -32005 | Incomplete write; delivery uncertain and port closed |

Tests exercise all transports, binary PTY transfers on POSIX, client replacement,
parser recovery, WebSocket fragmentation/ping, and Origin rejection. They do not
replace [physical serial, USB identity or Arduino reset acceptance](VALIDATION.md).
