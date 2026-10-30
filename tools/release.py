import hashlib
import pathlib
import re
import shutil
import sys
import zipfile

incoming, output = map(pathlib.Path, sys.argv[1:3])
version = re.search(r'project\(SerialUART VERSION (\d+\.\d+\.\d+)', pathlib.Path("CMakeLists.txt").read_text())[1]
if sys.argv[3] != "v" + version:
    raise SystemExit("Tag and source version disagree")
archives = sorted(incoming.glob("*/*.zip"))
targets = ["linux-x64-gcc", "linux-x64-clang", "linux-arm64-gcc", "macos-arm64-clang", "windows-x64-msvc"]
expected_names = {f"SerialUART-{target}-{linkage}.zip" for target in targets for linkage in ("static", "shared")}
expected_names.add("SerialUART-linux-armv7-gcc-static.zip")
if len(archives) != 11 or {a.name for a in archives} != expected_names:
    raise SystemExit("Release must contain the complete expected platform/linkage package set")
output.mkdir()
lines = []
for archive in archives:
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    expected = archive.with_suffix(".zip.sha256").read_text().strip()
    if expected != f"{digest}  {archive.name}":
        raise SystemExit(f"Checksum mismatch: {archive}")
    with zipfile.ZipFile(archive) as z:
        if z.testzip():
            raise SystemExit(f"Corrupt package: {archive}")
    target = output / archive.name
    if target.exists():
        raise SystemExit("Duplicate package name")
    shutil.copyfile(archive, target)
    lines.append(f"{digest}  {archive.name}\n")
(output / "SHA256SUMS").write_text("".join(lines), encoding="ascii")
