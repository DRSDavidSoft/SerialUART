"""Build tooling only; no nonstandard Python packages."""
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
archives = sorted(root.glob("*.zip"))
if not archives:
    raise SystemExit("No packages produced")
for archive in archives:
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + ".sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
