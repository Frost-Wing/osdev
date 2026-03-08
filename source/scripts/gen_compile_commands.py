#!/usr/bin/env python3
import json
import os
import shlex
import sys
from pathlib import Path


def main() -> int:
    src_root = Path(os.environ.get("SRC_ROOT", ".")).resolve()
    out_path = Path(os.environ.get("OUT_PATH", src_root / "compile_commands.json"))

    cfiles_raw = os.environ.get("CFILES", "").strip()
    if not cfiles_raw:
        print("No CFILES were provided; nothing to write.", file=sys.stderr)
        return 1

    cflags = shlex.split(os.environ.get("CFLAGS", ""))
    cppflags = shlex.split(os.environ.get("CPPFLAGS", ""))

    entries = []
    for rel in cfiles_raw.split():
        file_path = (src_root / rel).resolve()
        cmd = [
            "clang",
            *cflags,
            *cppflags,
            "-c",
            str(file_path),
            "-o",
            os.devnull,
        ]
        entries.append(
            {
                "directory": str(src_root),
                "file": str(file_path),
                "command": " ".join(shlex.quote(x) for x in cmd),
            }
        )

    out_path.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    print(f"[+] Wrote {len(entries)} entries to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())