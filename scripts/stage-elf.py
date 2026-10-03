#!/usr/bin/env python3
"""Stage an x86-64 ELF and its interpreter/shared-library dependencies in fs_root."""

import argparse
import filecmp
from pathlib import Path
import re
import shutil
import subprocess
import sys


HOST_LIBRARY_DIRS = (
    Path("/usr/lib/musl/lib"),
    Path("/lib/x86_64-linux-gnu"),
    Path("/usr/lib/x86_64-linux-gnu"),
    Path("/lib64/x86_64"),
    Path("/lib"),
    Path("/lib64"),
    Path("/usr/lib"),
    Path("/usr/lib64"),
    Path("/usr/local/lib"),
)


def readelf(path, option):
    result = subprocess.run(
        ["readelf", option, "-W", str(path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def interpreter_for(path):
    output = readelf(path, "-l")
    match = re.search(r"Requesting program interpreter:\s*([^\]]+)", output)
    return match.group(1) if match else None


def dynamic_info(path):
    output = readelf(path, "-d")
    needed = re.findall(r"\(NEEDED\).*Shared library: \[([^\]]+)\]", output)
    search_paths = []
    for value in re.findall(r"\((?:RPATH|RUNPATH)\).*Library (?:rpath|runpath): \[([^\]]*)\]", output):
        for item in value.split(":"):
            item = item.replace("${ORIGIN}", str(path.parent)).replace("$ORIGIN", str(path.parent))
            if item:
                search_paths.append(Path(item))
    return needed, search_paths


def ldconfig_libraries():
    try:
        result = subprocess.run(
            ["ldconfig", "-p"],
            check=True,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return {}

    libraries = {}
    for line in result.stdout.splitlines():
        match = re.match(r"\s*(\S+)\s+\([^)]*\)\s+=>\s+(\S+)", line)
        if match:
            libraries.setdefault(match.group(1), []).append(Path(match.group(2)))
    return libraries


def find_library(name, search_paths, ldconfig, root):
    if "/" in name and Path(name).is_absolute():
        candidates = [root / name.lstrip("/"), Path(name)]
    elif "/" in name:
        candidates = [directory / name for directory in search_paths]
        candidates.append(Path(name))
    else:
        candidates = [directory / name for directory in search_paths]
        candidates.extend(
            root / directory / name
            for directory in ("lib", "lib64", "usr/lib", "usr/lib64")
        )
        candidates.extend(directory / name for directory in HOST_LIBRARY_DIRS)
        candidates.extend(ldconfig.get(name, []))

        for directory in HOST_LIBRARY_DIRS:
            if directory.is_dir():
                candidates.extend(directory.glob(f"*/{name}"))

    for candidate in candidates:
        try:
            resolved = candidate.resolve(strict=True)
        except (OSError, RuntimeError):
            continue
        if resolved.is_file():
            with resolved.open("rb") as library:
                if library.read(4) == b"\x7fELF":
                    return resolved
    raise FileNotFoundError(f"cannot locate shared library {name!r}")


def copy_into(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        if destination.is_file() and filecmp.cmp(source, destination, shallow=False):
            return
        raise FileExistsError(f"refusing to replace different file: {destination}")
    shutil.copy2(source, destination)


def rooted_path(root, path):
    destination = (root / path.lstrip("/")).resolve()
    try:
        destination.relative_to(root.resolve())
    except ValueError as exc:
        raise ValueError(f"ELF path escapes root filesystem: {path}") from exc
    return destination


def stage_one(executable, root, ldconfig):
    executable = executable.resolve(strict=True)
    header = readelf(executable, "-h")
    if "ELF64" not in header or "X86-64" not in header:
        raise ValueError(f"{executable} is not an x86-64 ELF64 file")

    root.mkdir(parents=True, exist_ok=True)
    staged_executable = root / "bin" / executable.name
    copy_into(executable, staged_executable)
    print(f"{executable} -> {staged_executable}")

    interpreter = interpreter_for(executable)
    runtime_dir = Path(interpreter).parent if interpreter else Path("/lib")
    if interpreter and not Path(interpreter).is_absolute():
        raise ValueError(f"ELF interpreter path is not absolute: {interpreter}")
    pending = [executable]
    visited = set()

    if interpreter:
        interpreter_source = find_library(interpreter, (), ldconfig, root)
        interpreter_destination = rooted_path(root, interpreter)
        copy_into(interpreter_source, interpreter_destination)
        print(f"{interpreter_source} -> {interpreter_destination}")

    while pending:
        image = pending.pop()
        image_key = image.resolve()
        if image_key in visited:
            continue
        visited.add(image_key)

        needed, image_paths = dynamic_info(image)
        for name in needed:
            dependency = find_library(name, image_paths, ldconfig, root)
            if "/" in name:
                destination = rooted_path(root, name)
            else:
                destination = rooted_path(root, str(runtime_dir / name))
            copy_into(dependency, destination)
            print(f"{dependency} -> {destination}")
            pending.append(dependency)


def main():
    parser = argparse.ArgumentParser(
        description="Copy x86-64 ELF executables and their shared-library closure into fs_root."
    )
    parser.add_argument("--root", type=Path, default=Path("fs_root"), help="root filesystem directory")
    parser.add_argument("executables", nargs="+", type=Path)
    args = parser.parse_args()

    ldconfig = ldconfig_libraries()
    try:
        for executable in args.executables:
            stage_one(executable, args.root, ldconfig)
    except (FileNotFoundError, FileExistsError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"stage-elf: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
