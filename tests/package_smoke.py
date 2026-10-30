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
    (root / "main.c").write_text('#include <SerialUART.h>\nint main(void) { suart_config c; suart_config_init(&c); return c.baud!=57600; }\n')
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
    if version != b"2.1.0":
        raise SystemExit("Installed executable version differs from expected package")
    manifest = list(prefix.glob("share/doc/SerialUART/BUILD-INFO.txt"))
    if not manifest or "Source commit:" not in manifest[0].read_text():
        raise SystemExit("Package is missing source provenance")
    loopback = prefix / "bin" / ("SerialLoopback.exe" if os.name == "nt" else "SerialLoopback")
    subprocess.run([str(loopback), "--help"], check=True, env=env)
