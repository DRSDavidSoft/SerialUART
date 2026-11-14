"""Compile both applications without CMake (Python is build tooling only)."""
import argparse
import os
import pathlib
import platform
import re
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
p.add_argument('--output', default='build-direct')
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
out = pathlib.Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=True)
cc = shlex.split(a.compiler)
msvc = pathlib.Path(cc[0]).stem.lower() == 'cl'
windows = platform.system() == 'Windows'
common = ['SendCommand.c', 'cli/discovery.c', 'cli/session.c', 'cli/options.c',
          'cli/profiles.c', 'cli/devices.c', 'cli/keys.c', 'cli/windows_args.c',
          'lib/SerialUART.c', 'examples/protocol.c']
if platform.system() == 'Linux':
    common.append('lib/linux_native.c')
if windows:
    version = re.search(r'project\(SerialUART VERSION (\d+)\.(\d+)\.(\d+)',
                        (root / 'CMakeLists.txt').read_text(encoding='utf-8'))
for full in (False, True):
    name = 'SerialUART' if full else 'SendCommand'
    sources = list(common)
    defines = []
    if full:
        sources += ['cli/tui.c', 'cli/rpc.c', 'cli/json.c', 'examples/network.c',
                    'vendor/portable-shell/src/portable_shell.c']
        defines = ['SUART_FULL_APP', 'PS_EDITOR_CAPACITY=33792', 'PS_LINE=1024']
    includes = ['lib', 'cli', 'examples', 'vendor/portable-shell/include']
    exe = out / (name + ('.exe' if windows else ''))
    resource = []
    if windows:
        resource_dir = root / 'platform/windows'
        resource_defines = defines + [
            f'SUART_RESOURCE_{part}={value}'
            for part, value in zip(('MAJOR', 'MINOR', 'PATCH'), version.groups())
        ]
        if msvc:
            resource = [str(out / (name + '.res'))]
            rc = ['rc', '/nologo', '/I', str(resource_dir), '/fo', resource[0]]
            rc += ['/d' + item for item in resource_defines]
        else:
            resource = [str(out / (name + '-resources.o'))]
            rc = ['windres', '-I', str(resource_dir), '-O', 'coff', '-o', resource[0]]
            rc += ['-D' + item for item in resource_defines]
        subprocess.run(rc + [str(resource_dir / 'application.rc')], cwd=out, check=True)
    if msvc:
        flags = ['/nologo', '/std:c11', '/utf-8', '/O2', '/D_CRT_SECURE_NO_WARNINGS']
        flags += ['/I' + str(root / x) for x in includes] + ['/D' + x for x in defines]
        libs = ['setupapi.lib', 'advapi32.lib', 'cfgmgr32.lib', 'shell32.lib']
        if full: libs += ['ws2_32.lib']
        tail = ['/Fe:' + str(exe), '/link'] + libs
    else:
        flags = ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror']
        flags += ['-I' + str(root / x) for x in includes] + ['-D' + x for x in defines]
        libs = []
        if windows:
            libs = ['-lsetupapi', '-ladvapi32', '-lcfgmgr32', '-lshell32']
            if full: libs += ['-lws2_32']
        elif platform.system() == 'Darwin':
            libs = ['-framework', 'IOKit', '-framework', 'CoreFoundation']
        tail = libs + ['-o', str(exe)]
    command = cc + flags + [str(root / x) for x in sources] + resource + tail
    print(shlex.join(command), flush=True)
    subprocess.run(command, cwd=out, check=True)
    subprocess.run([str(exe), '--version'], check=True)
subprocess.run([os.sys.executable, str(root / 'tests/rpc_test.py'),
                str(out / ('SerialUART.exe' if windows else 'SerialUART')),
                str(out / ('SendCommand.exe' if windows else 'SendCommand'))], check=True)

if windows:
    subprocess.run([os.sys.executable, str(root / 'tests/windows_resources.py'),
                    '.'.join(version.groups()), str(out / 'SerialUART.exe'),
                    str(out / 'SendCommand.exe')], check=True)
