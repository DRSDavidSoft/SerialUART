// Optional interoperability test using the official client; never shipped as a runtime dependency.
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {spawn} from 'node:child_process';
import {createServer} from 'node:net';
import {once} from 'node:events';
import assert from 'node:assert/strict';
const require = createRequire(resolve(process.argv[2], 'package.json'));
const {io} = require('socket.io-client');
const reserve = createServer();
reserve.listen(0, '127.0.0.1'); await once(reserve, 'listening');
const port = reserve.address().port;
await new Promise(done => reserve.close(done));
const server = spawn(resolve(process.argv[3]), ['socketio', String(port)]);
let client;
const timeout = setTimeout(() => { server.kill(); process.exitCode = 1; }, 55000);
try {
  const [ready] = await once(server.stdout, 'data');
  assert.match(ready.toString(), /READY/);
  client = io(`http://127.0.0.1:${port}`, {transports:['websocket'], upgrade:false, reconnection:false});
  await once(client, 'connect');
  const bytes = Buffer.from(Array.from({length:256}, (_, i) => i));
  const response = once(client, 'serial');
  client.emit('serial', bytes.toString('base64'));
  assert.deepEqual(Buffer.from((await response)[0], 'base64'), bytes);
  let heartbeats = 0;
  client.io.engine.on('ping', () => heartbeats++);
  await new Promise(done => setTimeout(done, 46000));
  assert.ok(client.connected, 'connection survived heartbeat');
  assert.ok(heartbeats >= 1, 'received real Engine.IO heartbeat');
  const after = once(client, 'serial');
  client.emit('serial', 'YWZ0ZXI=');
  assert.equal((await after)[0], 'YWZ0ZXI=');
  console.log('Official Socket.IO client: binary event and 46-second heartbeat passed');
} finally {
  clearTimeout(timeout); client?.close(); server.kill();
}
