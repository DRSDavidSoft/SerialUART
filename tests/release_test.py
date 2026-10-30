"""Verify release fail-closed behavior without publishing anything."""
import hashlib
import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent


class Release(unittest.TestCase):
    def test_complete_set_and_tamper_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            incoming = tmp / "incoming"
            targets = ["linux-x64-gcc", "linux-x64-clang", "linux-arm64-gcc", "macos-arm64-clang", "windows-x64-msvc"]
            names = [f"SerialUART-{target}-{linkage}" for target in targets for linkage in ("static", "shared")]
            names.append("SerialUART-linux-armv7-gcc-static")
            archives = []
            for name in names:
                folder = incoming / name
                folder.mkdir(parents=True)
                archive = folder / (name + ".zip")
                with zipfile.ZipFile(archive, "w") as z:
                    z.writestr("BUILD-INFO.txt", "fixture")
                digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n")
                archives.append(archive)

            def run(output, tag="v2.2.0"):
                return subprocess.run([sys.executable, str(ROOT / "tools/release.py"), str(incoming), str(tmp / output), tag], cwd=ROOT, capture_output=True)

            self.assertNotEqual(run("bad-tag", "v0.0.0").returncode, 0)
            result = run("valid")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(len(list((tmp / "valid").glob("*.zip"))), 11)
            self.assertEqual(len((tmp / "valid" / "SHA256SUMS").read_text().splitlines()), 11)
            archives[0].write_bytes(b"tampered")
            self.assertNotEqual(run("tampered").returncode, 0)
            archives[0].unlink()
            self.assertNotEqual(run("missing").returncode, 0)


if __name__ == "__main__":
    unittest.main()
