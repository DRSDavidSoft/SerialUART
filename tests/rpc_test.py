"""Real NDJSON pipe/TCP/WebSocket RPC and virtual serial ownership tests."""
import base64, contextlib, faulthandler, json, os, pathlib, queue, select, shlex, socket, struct
import subprocess, sys, tempfile, threading, time, unittest
APP, MINI = sys.argv[1:3]
EMULATOR=shlex.split(os.environ.get('SUART_EMULATOR',''));sys.argv[1:]=[]
def request(method,params=None,id=1):
    value={'jsonrpc':'2.0','id':id,'method':method}
    if params is not None: value['params']=params
    return value
def exact(s,n):
    result=b''
    while len(result)<n:
        part=s.recv(n-len(result))
        if not part: raise EOFError()
        result+=part
    return result
def frame(data,op=1,fin=True):
    key=b'abcd';head=bytes([(128 if fin else 0)|op])
    head+=bytes([128|len(data)]) if len(data)<126 else b'\xfe'+struct.pack('!H',len(data))
    return head+key+bytes(b^key[i%4] for i,b in enumerate(data))
def ws_read(s):
    a,b=exact(s,2);assert not b&128;n=b&127
    if n==126: n=struct.unpack('!H',exact(s,2))[0]
    return a&15,exact(s,n)
def upgrade(s,origin=None):
    text='GET /rpc HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n'
    if origin: text+='Origin: '+origin+'\r\n'
    s.sendall((text+'\r\n').encode());data=b''
    while not data.endswith(b'\r\n\r\n'): data+=exact(s,1)
    assert b'101 Switching Protocols' in data
class Client:
    def __init__(self,p,transport,port=None):
        self.p=p;self.transport=transport;self.s=None;self.file=None;self.q=queue.Queue()
        if transport=='stdio':
            def read():
                for line in iter(p.stdout.readline,b''): self.q.put(line)
                self.q.put(None)
            threading.Thread(target=read,daemon=True).start()
        else:
            self.s=socket.create_connection(('127.0.0.1',port),timeout=4)
            if transport=='websocket': upgrade(self.s)
            else: self.file=self.s.makefile('rb')
    def send(self,value):
        data=json.dumps(value,separators=(',',':')).encode()
        if self.transport=='stdio': self.p.stdin.write(data+b'\n');self.p.stdin.flush()
        elif self.transport=='tcp': self.s.sendall(data+b'\r\n')
        else: self.s.sendall(frame(data)) # a complete message needs no LF
    def receive(self):
        if self.transport=='stdio': data=self.q.get(timeout=5)
        elif self.transport=='tcp': data=self.file.readline()
        else:
            op,data=ws_read(self.s);assert op==1
        if data is None: raise EOFError()
        return json.loads(data)
    def call(self,method,params=None,id=1):
        self.send(request(method,params,id));result=self.receive();assert result['id']==id,result
        return result
    def close(self):
        if self.file: self.file.close()
        if self.s: self.s.close()
@contextlib.contextmanager
def service(transport,extra=()):
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    args=[APP,'--api',transport]+([] if transport=='stdio' else ['--api-port',str(port)])+list(extra)
    p=subprocess.Popen(EMULATOR+args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    q=queue.Queue()
    threading.Thread(target=lambda:q.put(p.stderr.readline()),daemon=True).start()
    c=None
    try:
        ready=q.get(timeout=8);assert b'API ready' in ready,ready
        c=Client(p,transport,port);yield c,p,port
    finally:
        if c: c.close()
        if p.poll() is None: p.kill()
        p.wait(timeout=5)
        for f in (p.stdin,p.stdout,p.stderr): f.close()
class API(unittest.TestCase):
    def test_product_split_and_descriptive_listing(self):
        for exe in (MINI,APP):
            p=subprocess.run(EMULATOR+[exe,'--list-devices','--output-format','json'],capture_output=True,timeout=8)
            self.assertEqual(p.returncode,0,p.stderr);self.assertIsInstance(json.loads(p.stdout),list)
            p=subprocess.run(EMULATOR+[exe,'--list-devices','--output-format','ndjson'],capture_output=True,timeout=8)
            self.assertEqual(p.returncode,0,p.stderr)
            for line in p.stdout.splitlines(): self.assertIn('instance_id',json.loads(line))
            for flag in ('--tui','--list-json'):
                self.assertEqual(subprocess.run(EMULATOR+[exe,flag],capture_output=True,timeout=5).returncode,2)
        self.assertEqual(subprocess.run(EMULATOR+[MINI,'--api','stdio'],capture_output=True,timeout=5).returncode,2)
        mini_help=subprocess.check_output(EMULATOR+[MINI,'--help'])
        self.assertNotIn(b'--api',mini_help);self.assertNotIn(b'--interactive',mini_help)
        self.assertIn(b'default: interactive workspace',subprocess.check_output(EMULATOR+[APP,'--help']))
        for args in (['--api-port','1234'],['--api','stdio','--api-port','1234'],['--api','tcp','--send','x'],['--output-format','json']):
            self.assertEqual(subprocess.run(EMULATOR+[APP]+args,capture_output=True,timeout=5).returncode,2)
    def test_all_transports_notifications_batches_unicode_and_errors(self):
        for transport in ('stdio','tcp','websocket'):
            with self.subTest(transport=transport),service(transport) as (c,p,_):
                info=c.call('system.info',id='شناسه\U0001f680')['result']
                self.assertEqual(info['application'],'SerialUART')
                self.assertIn('websocket',info['transports'])
                self.assertIsInstance(c.call('devices.list',{'limit':1})['result']['devices'],list)
                self.assertEqual(c.call('port.read')['error']['code'],-32002)
                self.assertEqual(c.call('unknown')['error']['code'],-32601)
                self.assertEqual(c.call('system.info',{'typo':1})['error']['code'],-32602)
                c.send({'jsonrpc':'2.0','method':'system.info'}) # no reply
                c.send([{'jsonrpc':'2.0','method':'system.info'},request('port.status',id=2),False,request('system.info',id=None)])
                batch=c.receive();self.assertEqual(len(batch),3)
                self.assertEqual(batch[0]['id'],2);self.assertEqual(batch[1]['error']['code'],-32600);self.assertIsNone(batch[2]['id'])
                c.send([]);self.assertEqual(c.receive()['error']['code'],-32600)
                c.call('server.shutdown');self.assertEqual(p.wait(timeout=5),0)
    def test_strict_parser_limits_and_recovery(self):
        bad=[b'{',b'{"jsonrpc":"2.0","id":1,"id":2,"method":"system.info"}',
             b'{"jsonrpc":"2.0","id":1,"\\u0069d":2,"method":"system.info"}',
             b'{"x":"\\ud800"}',b'{"x":"\xff"}',b'{"x":01}',b'{"x":1.}',b'{"x":true,}',
             b'['*18+b'0'+b']'*18,b'{"x":"'+b'x'*17000+b'"}',b'{"x":null}\0']
        payload=b'\r\n'.join(bad)+b'\n'+json.dumps(request('system.info',id=9)).encode()+b'\n'
        p=subprocess.run(EMULATOR+[APP,'--api','stdio'],input=payload,capture_output=True,timeout=10)
        self.assertEqual(p.returncode,0,p.stderr);responses=[json.loads(x) for x in p.stdout.splitlines()]
        self.assertEqual(len(responses),len(bad)+1)
        self.assertTrue(all(x['error']['code']==-32700 for x in responses[:-1]))
        self.assertEqual(responses[-1]['id'],9)
    def test_websocket_fragmentation_ping_and_origin(self):
        with service('websocket') as (c,p,port):
            data=json.dumps(request('system.info')).encode();c.s.sendall(frame(data[:12],fin=False)+frame(b'ping',op=9)+frame(data[12:],op=0))
            self.assertEqual(ws_read(c.s),(10,b'ping'));self.assertEqual(c.receive()['id'],1)
            c.close()
            with socket.create_connection(('127.0.0.1',port),timeout=4) as other:
                with self.assertRaises((EOFError,ConnectionResetError)): upgrade(other,'https://untrusted.example')
        with service('websocket',('--origin','http://127.0.0.1:8000')) as (c,p,port):
            c.close()
            with socket.create_connection(('127.0.0.1',port),timeout=4) as browser:
                upgrade(browser,'http://127.0.0.1:8000')
                browser.sendall(frame(json.dumps(request('system.info')).encode()))
                op,data=ws_read(browser);self.assertEqual(op,1)
                self.assertEqual(json.loads(data)['result']['application'],'SerialUART')
    @unittest.skipIf(os.name=='nt','serial transfer uses a POSIX PTY, never an arbitrary COM port')
    def test_serial_session_binary_events_filters_and_client_ownership(self):
        import pty
        for transport in ('stdio','tcp','websocket'):
            wire,port=pty.openpty();path=os.ttyname(port)
            try:
                with service(transport) as (c,p,endpoint):
                    self.assertIn('error',c.call('port.open',{'path':path,'baud':0}))
                    opened=c.call('port.open',{'path':path,'baud':9600});self.assertIn('result',opened,opened)
                    self.assertEqual(c.call('port.open',{'path':path})['error']['code'],-32001)
                    for bad in ('!', 'A===', 'AA=A', 'Zh==', base64.b64encode(b'x'*4097).decode()):
                        self.assertEqual(c.call('port.write',{'data':bad})['error']['code'],-32602)
                    self.assertEqual(c.call('port.read',{'max_bytes':4097})['error']['code'],-32602)
                    self.assertEqual(c.call('port.set_signal',{'line':'dtr'})['error']['code'],-32602)
                    payload=bytes(range(256))*4
                    sent=c.call('port.write',{'data':base64.b64encode(payload).decode()})
                    self.assertEqual(sent['result']['written'],len(payload));got=b'';end=time.monotonic()+3
                    while len(got)<len(payload) and time.monotonic()<end:
                        if select.select([wire],[],[],.1)[0]: got+=os.read(wire,4096)
                    self.assertEqual(got,payload)
                    os.write(wire,payload);got=b'';end=time.monotonic()+3
                    while len(got)<len(payload) and time.monotonic()<end:
                        reply=c.call('port.read',{'max_bytes':4096,'timeout_ms':1000})
                        got+=base64.b64decode(reply['result']['data'])
                    self.assertEqual(got,payload)
                    c.call('port.subscribe');os.write(wire,b'event\0\xff')
                    event=c.receive();self.assertEqual(event['method'],'port.received')
                    self.assertEqual(base64.b64decode(event['params']['data']),b'event\0\xff')
                    self.assertEqual(c.call('port.read')['error']['code'],-32003)
                    if transport!='stdio':
                        c.close();time.sleep(.05);new=Client(p,transport,endpoint)
                        try: self.assertFalse(new.call('port.status')['result']['connected']);new.call('server.shutdown')
                        finally: new.close()
                    else: c.call('port.close');c.call('server.shutdown')
                    self.assertEqual(p.wait(timeout=5),0)
                with service('stdio',('--block','port='+path)) as (c,_,__):
                    self.assertIn('error',c.call('port.open',{'path':path}))
            finally: os.close(wire);os.close(port)
if __name__=='__main__':
    faulthandler.dump_traceback_later(20,repeat=True)
    unittest.main(verbosity=2)
