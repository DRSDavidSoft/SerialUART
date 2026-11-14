"""Check Windows executable branding through native Windows APIs."""
import ctypes
import json
import pathlib
import subprocess
import sys


def verify(executables, version):
    paths = [pathlib.Path(exe).resolve() for exe in executables]
    extract = ctypes.WinDLL("shell32").ExtractIconExW
    extract.argtypes = [ctypes.c_wchar_p, ctypes.c_int, ctypes.c_void_p,
                        ctypes.c_void_p, ctypes.c_uint]
    extract.restype = ctypes.c_uint
    for path in paths:
        if extract(str(path), -1, None, None, 0) != 1:
            raise RuntimeError(f"{path} should contain one application icon group")
        # Quote the path as a literal PowerShell string.
        quoted_path = "'" + str(path).replace("'", "''") + "'"
        command = (
            f"$v = (Get-Item -LiteralPath {quoted_path}).VersionInfo\n"
            "$v | Select-Object FileVersion,ProductVersion,OriginalFilename,InternalName,"
            "FileDescription,ProductName,CompanyName,FileMajorPart,FileMinorPart,"
            "FileBuildPart,FilePrivatePart | ConvertTo-Json -Compress"
        )
        info = json.loads(subprocess.check_output(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command]))
        fixed_version = tuple(info[key] for key in
                              ("FileMajorPart", "FileMinorPart", "FileBuildPart", "FilePrivatePart"))
        expected = {
            "FileVersion": version, "ProductVersion": version,
            "OriginalFilename": path.name, "InternalName": path.stem,
            "ProductName": "SerialUART", "CompanyName": "DRSDavidSoft",
        }
        if (any(info[key] != value for key, value in expected.items())
                or not info["FileDescription"]
                or fixed_version != (*map(int, version.split(".")), 0)):
            raise RuntimeError(f"Incorrect version information in {path}: {info}")
        print(f"{path.name}: embedded icon and Windows version information verified")


if __name__ == "__main__":
    verify(sys.argv[2:], sys.argv[1])
