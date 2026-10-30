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
if len(archives) != 11:
    raise SystemExit(f"Expected 11 packages, got {len(archives)}")
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
