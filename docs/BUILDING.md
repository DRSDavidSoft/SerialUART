# Build and installation

## Requirements

- C99 compiler and CMake 3.16+. C++ is used for the C++ consumer test.
- GCC, Clang, Apple Clang, modern MSVC (Visual Studio 2022), or MinGW-w64.
- Native SDK headers/libraries supplied by the OS/compiler.
- Python 3 for integration/package tooling only. No pip packages are needed.

Use `-DBUILD_TESTING=OFF` for a C-only production build without a C++ compiler or
Python. Both the library and executable are written entirely in C.

## Windows

From a Visual Studio developer terminal:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\SendCommand.exe --help
cmake --install build --config Release --prefix C:/Tools/SerialUART
```

For MinGW-w64, with GCC and Ninja on PATH:

```powershell
cmake -S . -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
.\build-mingw\SendCommand.exe --list
```

The NMake entry point uses the same targets and native link dependencies:

```bat
nmake /nologo /f Makefile.msvc test
nmake /nologo /f Makefile.msvc install PREFIX=C:\Tools\SerialUART
```

Run this in an **x64 Visual Studio developer terminal**, which supplies `cl`,
`link`, `nmake` and SDK environment variables. It creates `build-nmake` using
CMake's `NMake Makefiles` generator. Override `BUILD_DIR` for another build
directory and `CMAKE_FLAGS=-DBUILD_SHARED_LIBS=ON` for a shared build. NMake
and GNU Make syntax are different; use the matching entry point.

Default builds link the serial library statically. MSVC's runtime is supplied by
Windows/Visual C++ runtime installation; MinGW installations may supply runtime
DLLs depending on their toolchain. Do not mix import libraries from incompatible
compiler toolchains. Shared builds install the serial DLL beside the executable.

## Linux and Raspberry Pi OS

On Debian, Ubuntu or Raspberry Pi OS:

```sh
sudo apt-get install build-essential cmake python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/bin/SendCommand" --list
```

Use the distro's serial access group, often `dialout` (`uucp` on some distros):

```sh
sudo usermod -aG dialout "$USER"
```

Sign out and back in for group membership to apply. Avoid world-writable device
nodes. For the Pi GPIO UART, enable the UART and disable the serial login console
using the OS configuration tool; the active port is often `/dev/serial0`.
GPIO UART is 3.3 V logic, not an RS-232 voltage interface. USB Arduino devices
usually appear as `/dev/ttyACM*` or `/dev/ttyUSB*`.

GNU Make users can run `make test` or `make install PREFIX="$HOME/.local"`.
The wrapper uses CMake; override `BUILD_DIR` and `CMAKE_FLAGS` as needed.
`make clean` cleans compiler outputs through the build tool without deleting
the build directory. These are build-time dependencies, not runtime requirements.

UART names and routing vary by Pi model and overlays. `/dev/serial0` may point
to `ttyS0`, `ttyAMA0`, or a Pi 5 debug UART such as `ttyAMA10`; it is not a promise
that pins 8/10 are currently routed to that device. Inspect the active alias,
UART overlays and login-console configuration using the
[current Raspberry Pi UART guide](https://www.raspberrypi.com/documentation/computers/configuration.html#configuring-uarts).
Bluetooth and a serial console may already own an interface. Check wiring,
clock/baud behavior and flow-control availability on the actual board before
interpreting a loopback test.

### ARMv7 cross build

On a Debian/Ubuntu x64 host:

```sh
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf qemu-user
cmake -S . -B build-pi -DCMAKE_TOOLCHAIN_FILE=cmake/armv7-linux.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-pi --parallel 2
SUART_EMULATOR='qemu-arm -L /usr/arm-linux-gnueabihf' ctest --test-dir build-pi --output-on-failure
```

These binaries target ARMv7 hard-float Linux. They do not target ARMv6 Pi 1/Zero.
Native compilation is recommended when the target OS has an older glibc than
the CI runner. ARM64 CI runs natively on Linux ARM64, separately from QEMU.

## macOS

Install Xcode Command Line Tools (`xcode-select --install`) and CMake, then use
the same configure/build/test commands as Linux. Prefer `/dev/cu.*` callout
devices. IOKit and CoreFoundation come from the macOS SDK.

## Shared library and CMake consumers

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-shared --config Release
cmake --install build-shared --config Release --prefix /chosen/prefix
```

Installed CMake consumers use:

```cmake
find_package(SerialUART 2 REQUIRED)
target_link_libraries(your_program PRIVATE SerialUART::serialuart)
```

Pass `-DCMAKE_PREFIX_PATH=/chosen/prefix` to the consumer. The imported target
propagates Windows DLL import definitions and static native library dependencies.
Build-tree consumers can use `add_subdirectory(SerialUART)` and the same target.

## Without CMake

Linux:

```sh
cc -std=c99 -O2 -Ilib -Iexamples -Icli SendCommand.c cli/*.c lib/SerialUART.c lib/linux_native.c examples/protocol.c examples/network.c -o SendCommand
```

MinGW-w64:

```sh
gcc -std=c99 -O2 -Ilib -Iexamples -Icli SendCommand.c cli/*.c lib/SerialUART.c examples/protocol.c examples/network.c -lsetupapi -ladvapi32 -lws2_32 -lshell32 -o SendCommand.exe
```

MSVC developer terminal:

```bat
cl /nologo /std:c11 /utf-8 /O2 /Ilib /Iexamples /Icli SendCommand.c cli\session.c cli\options.c cli\profiles.c cli\devices.c cli\keys.c cli\windows_args.c lib\SerialUART.c examples\protocol.c examples\network.c /Fe:SendCommand.exe /link setupapi.lib advapi32.lib ws2_32.lib shell32.lib
```

For macOS omit `lib/linux_native.c` and append
`-framework IOKit -framework CoreFoundation` to the Unix command.
For the serial library alone, compile `lib/SerialUART.c` and, on Linux only,
`lib/linux_native.c`; protocol/network adapters introduce no serial library
dependency. Linux's native helper keeps kernel `termios2` separate from libc's
incompatible `termios` definition; do not concatenate the source files.

Build the standalone loopback tool on Linux with:

```sh
cc -std=c99 -O2 -Ilib examples/SerialLoopback.c lib/SerialUART.c lib/linux_native.c -o SerialLoopback
```

## CI/CD

[GitHub Actions](https://github.com/DRSDavidSoft/SerialUART/blob/master/.github/workflows/build.yml) builds and tests ten native
combinations (five compiler/platform targets × static/shared), plus ARMv7/QEMU.
Every native build installs to a staging prefix and builds a separate CMake
consumer against it. Packages include the executable, library/import library,
public header, `SerialLoopback`, CMake config, source/compiler manifest, guides and host adapter sources. Each ZIP has a
SHA-256 sidecar. CI packages are retained as downloadable run artifacts.

The `SerialUART complete` job reports overall build health and is suitable as a
required branch check. Tag `v2.2.0` (matching the source version) to run the same
checks and create a **draft** release with all 11 ZIPs and `SHA256SUMS`. Existing
releases are not overwritten. Packages receive GitHub build-provenance attestations.
Review the draft and publish it manually.
No workflow flashes hardware or installs into a running user's system.

GitHub Actions is the sole CI/CD provider. Its MSVC static/shared jobs cover
Windows compatibility alongside the Linux, macOS and ARM jobs. Runtime code
has no dependency downloader.

Packages include the offline board snapshot and its MIT notice, plus example
terminal/AT-command profiles. Board recognition is compiled in; catalog refresh
uses the optional maintenance tool described in [the feature guide](SIMPLYSERIAL.md).
GNU Make/NMake and direct compiler paths are checked alongside CMake builds.

ASan/UBSan runs exercise the protocol and serial tests. An optional CI-only
Socket.IO 4.8.1 client verifies actual compatibility and a 46-second heartbeat
session; npm dependencies are test tooling and are not linked or shipped.

Verify a downloaded package with `sha256sum -c SHA256SUMS` on Linux or
`Get-FileHash package.zip -Algorithm SHA256` on PowerShell. Extract it and add
the extracted `bin` directory to PATH, or use CMake installation from source.
