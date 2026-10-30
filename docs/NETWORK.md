# Network host adapters

The network examples bridge one serial connection to **one active peer**.
They use only native sockets and the bounded C protocol helpers in
[`examples/`](../examples/). They are separate from the serial library and can
be replaced without changing its ABI.

## Raw TCP

```sh
SendCommand -p /dev/ttyACM0 -b 115200 --listen tcp 127.0.0.1 8765
SendCommand -p COM12 --connect tcp 127.0.0.1 8765
```

A listener accepts a client and transfers bytes in both directions. A TCP
client connects to an existing TCP server. TCP is a byte stream: serial reads,
socket reads and application messages need not share boundaries. SendCommand
does not implement UDP, IP raw sockets, RFC2217, or remote port reconfiguration.
Here “raw socket” means an unframed TCP stream.

## WebSocket

```sh
SendCommand -p /dev/ttyACM0 -b 115200 --listen ws 127.0.0.1 8765 --origin http://localhost:8000
```

Use `ws://127.0.0.1:8765/serial` (or `/`). Client text and binary payloads become
serial bytes; outgoing serial chunks are binary WebSocket messages. RFC6455
handshake, masking, fragmented messages, UTF-8 text validation, close and
ping/pong control frames are handled. Messages are bounded to 64 KiB;
64-bit WebSocket length encodings, extensions and compression are rejected.

Serve the included [browser preview](../examples/web-terminal.html) using your
preferred local web server at the allowed Origin. With Python installed for
development: `python3 -m http.server 8000 --directory examples`. Open
`http://localhost:8000/web-terminal.html`. This is a real WebSocket byte console,
not a simulated serial device. The browser is optional; headless clients can
connect without an Origin header.

## Socket.IO

```sh
SendCommand -p /dev/ttyACM0 --listen socketio 127.0.0.1 8765 --origin http://localhost:8000
```

The adapter implements an explicit subset of Engine.IO v4 / Socket.IO protocol
v5, compatible with Socket.IO 4.x clients configured for **WebSocket only**:

```javascript
// io is supplied by the official Socket.IO client in your application.
const serial = io("http://127.0.0.1:8765", {
  transports: ["websocket"], upgrade: false
});
serial.on("serial", base64 => {
  const bytes = Uint8Array.from(atob(base64), c => c.charCodeAt(0));
  console.log(bytes);
});
serial.emit("serial", btoa("STATUS\r\n"));
```

The exact endpoint is `/socket.io/?EIO=4&transport=websocket`. The root namespace
is accepted with optional auth object, then the server exchanges `serial` events
whose single argument is a **base64 string**. This preserves arbitrary binary
bytes without JSON escaping or binary-attachment state. The event's wire JSON is
the compact form emitted by the official client: `42["serial","BASE64"]`.
Engine.IO open, namespace connect/disconnect, 25-second ping and 20-second pong
timeout are supported. Long polling/upgrades, custom namespaces/events,
acknowledgements, native binary attachments and arbitrary JSON formatting are
outside this example's contract. Install the Socket.IO client only in your
consumer application if needed; SerialUART itself has no Socket.IO dependency.

References: [RFC6455](https://datatracker.ietf.org/doc/html/rfc6455),
[Engine.IO protocol](https://socket.io/docs/v4/engine-io-protocol/),
[Socket.IO protocol](https://socket.io/docs/v4/socket-io-protocol/).

## Exposure, flow control and disconnects

- Bind `127.0.0.1` or `::1` for local use. Explicit non-loopback binding exposes
  serial control to that interface. There is no authentication or TLS.
- Browser Origins are rejected unless `--origin` exactly matches. This protects
  against unrelated browser pages; it is not authentication for native clients.
- Use an authenticated SSH/VPN/TLS tunnel to cross trust boundaries. Do not expose
  an unauthenticated bridge directly to the Internet.
- The serial device stays open across peer changes. Serial input received while
  no peer is ready is discarded; no historical data is replayed to a new peer.
- Incoming queues are bounded to 64 KiB. A slow socket write has a two-second
  deadline; a failing peer is dropped. There is no delivery guarantee or durable
  buffering. Use RTS/CTS where supported and an application protocol with
  acknowledgements if reliable delivery matters.
- Network data never changes baud/DTR or triggers a local terminal shortcut.
