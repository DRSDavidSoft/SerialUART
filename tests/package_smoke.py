import json
import os
import pathlib
import subprocess
import sys
import tempfile

prefix = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.16)
project(Consumer LANGUAGES C)
find_package(SerialUART 2 REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE SerialUART::serialuart)
''')
    (root / "main.c").write_text('''#include <SerialUART.h>
int main(void) {
    suart_config c; suart_modem m; suart_counters counters;
    unsigned char b; size_t n;
    suart_device_info info; suart_selector_ex selector={0};
    suart_config_init(&c);
    return !suart_discovery_source()[0] || suart_resolve_ex(NULL,&info)!=SUART_INVALID ||
        suart_device_matches(&selector,NULL)!=0 || suart_enumerate_ex(NULL,NULL)!=SUART_INVALID ||
        c.baud!=57600 || suart_wait_tx(NULL,0)!=SUART_INVALID ||
        suart_get_modem(NULL,&m)!=SUART_INVALID ||
        suart_get_counters(NULL,&counters)!=SUART_INVALID ||
        suart_read_until(NULL,&b,1,&n,'\\n',0)!=SUART_INVALID;
}
''')
    subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"), f"-DCMAKE_PREFIX_PATH={prefix}"], check=True)
    subprocess.run(["cmake", "--build", str(root / "build"), "--config", "Release"], check=True)
    env = dict(os.environ)
    env["PATH"] = str(prefix / "bin") + os.pathsep + env.get("PATH", "")
    env["LD_LIBRARY_PATH"] = str(prefix / "lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    exe = root / "build" / ("Release/consumer.exe" if os.name == "nt" else "consumer")
    if not exe.exists():
        exe = root / "build" / "consumer.exe"
    subprocess.run([str(exe)], check=True, env=env)
    cli = prefix / "bin" / ("SendCommand.exe" if os.name == "nt" else "SendCommand")
    version = subprocess.check_output([str(cli), "--version"], env=env).strip()
    if version != b"2.4.0":
        raise SystemExit("Installed executable version differs from expected package")
    manifest = list(prefix.glob("share/doc/SerialUART/BUILD-INFO.txt"))
    if not manifest or "Source commit:" not in manifest[0].read_text():
        raise SystemExit("Package is missing source provenance")
    loopback = prefix / "bin" / ("SerialLoopback.exe" if os.name == "nt" else "SerialLoopback")
    subprocess.run([str(loopback), "--help"], check=True, env=env)
    docs = prefix / "share/doc/SerialUART"
    if "Edward Wright" not in (docs / "data/SIMPLYSERIAL-LICENSE.txt").read_text():
        raise SystemExit("Imported board-data license is missing")
    if "MIT License" not in (docs / "portable-shell/LICENSE").read_text():
        raise SystemExit("Embedded PortableShell license is missing")
    if not (docs / "examples/profiles/circuitpython.cfg").is_file():
        raise SystemExit("Example terminal profile is missing")

    app = prefix / "bin" / ("SerialUART.exe" if os.name == "nt" else "SerialUART")
    assert subprocess.check_output([str(app), "--version"], env=env).strip() == version
    request = b'{"jsonrpc":"2.0","id":1,"method":"system.info"}\n'
    result = subprocess.run([str(app), "--api", "stdio"], input=request,
                            capture_output=True, check=True, timeout=10, env=env)
    assert json.loads(result.stdout)["result"]["application"] == "SerialUART"
    assert (docs / "docs/RPC.md").is_file()
    assert (docs / "examples/rpc_client.py").is_file()
