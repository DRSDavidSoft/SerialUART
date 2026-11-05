"""Inspect SerialUART through child-process IPC without opening a serial port."""
import argparse
import json
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable', default='SerialUART')
a = p.parse_args()
with subprocess.Popen([a.executable, '--api', 'stdio'], stdin=subprocess.PIPE,
                      stdout=subprocess.PIPE, text=True, encoding='utf-8') as child:
    def call(method, params=None, identifier=1):
        request = dict(jsonrpc='2.0', id=identifier, method=method)
        if params is not None:
            request['params'] = params
        child.stdin.write(json.dumps(request, separators=(',', ':')) + '\n')
        child.stdin.flush()
        result = json.loads(child.stdout.readline())
        if 'error' in result:
            raise RuntimeError(result['error'])
        return result['result']
    try:
        print(json.dumps(call('system.info'), indent=2, ensure_ascii=False))
        offset = 0
        while offset is not None:
            page = call('devices.list', {'offset': offset, 'limit': 16})
            for device in page['devices']:
                print(json.dumps(device, ensure_ascii=False))
            offset = page['next_offset']
        call('server.shutdown')
    finally:
        child.stdin.close()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
