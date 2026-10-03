#!/usr/bin/env python3
"""
update_copyright.py

Scans *.c and *.h files under a given root directory and normalizes
@copyright lines to the form:

    @copyright Copyright (c) Pradosh 2023
        -> @copyright Copyright (c) Pradosh 2023-2026

    @copyright Copyright (c) Pradosh          (no year)
        -> @copyright Copyright (c) Pradosh 2026

    @copyright Copyright (c) Pradosh 2023-2025
        -> @copyright Copyright (c) Pradosh 2023-2026   (end year bumped)

    @copyright Copyright (c) Pradosh 2026
        -> left untouched (already current, single year)

    @copyright Copyright (c) Pradosh 2023-2026
        -> left untouched (already up to date)

Usage:
    python3 update_copyright.py [root_dir] [--year YYYY] [--dry-run]

    root_dir     Directory to scan recursively (default: current directory)
    --year       Override the "current" year (default: system current year)
    --dry-run    Show what would change without writing any files

Files/dirs skipped: .git, build, cmake-build*, out, bin, obj, node_modules
"""

import argparse
import datetime
import os
import re
import sys

# Matches: @copyright Copyright (c) Pradosh [YEAR[-YEAR]]
COPYRIGHT_RE = re.compile(
    r'(@copyright\s+Copyright\s*\(c\)\s*Pradosh)(\s+(\d{4})(?:\s*-\s*(\d{4}))?)?',
    re.IGNORECASE
)

SKIP_DIRS = {'.git', 'build', 'out', 'bin', 'obj', 'node_modules'}


def should_skip_dir(dirname: str) -> bool:
    return dirname in SKIP_DIRS or dirname.startswith('cmake-build')


def normalize_line(line: str, current_year: int):
    """Return (new_line, changed, note) for a line containing @copyright, or
    (line, False, None) if no @copyright tag is found on this line."""
    m = COPYRIGHT_RE.search(line)
    if not m:
        return line, False, None

    prefix = m.group(1)          # "@copyright Copyright (c) Pradosh"
    start_year = m.group(3)      # first year, or None
    end_year = m.group(4)        # second year (range end), or None

    if start_year is None:
        # No year at all -> just current year
        new_tag = f"{prefix} {current_year}"
        note = "added current year (no year present)"
    elif end_year is not None:
        # Already a range -> bump end year to current, keep start
        if int(end_year) == current_year:
            return line, False, None  # already up to date
        new_tag = f"{prefix} {start_year}-{current_year}"
        note = f"updated range end {end_year} -> {current_year}"
    else:
        # Single year present
        if int(start_year) == current_year:
            return line, False, None  # single current year, leave alone
        new_tag = f"{prefix} {start_year}-{current_year}"
        note = f"converted single year {start_year} -> range {start_year}-{current_year}"

    new_line = line[:m.start()] + new_tag + line[m.end():]
    return new_line, True, note


def process_file(path: str, current_year: int, dry_run: bool):
    try:
        with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
            lines = f.readlines()
    except OSError as e:
        print(f"  [ERROR] could not read {path}: {e}")
        return False, False

    changed = False
    found_tag = False
    new_lines = []
    for line in lines:
        new_line, line_changed, note = normalize_line(line, current_year)
        if '@copyright' in line.lower():
            found_tag = True
        if line_changed:
            changed = True
            print(f"  {os.path.relpath(path)}: {note}")
        new_lines.append(new_line)

    if changed and not dry_run:
        try:
            with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
                f.writelines(new_lines)
        except OSError as e:
            print(f"  [ERROR] could not write {path}: {e}")
            return changed, found_tag

    return changed, found_tag


def main():
    parser = argparse.ArgumentParser(description="Normalize @copyright year tags in .c/.h files.")
    parser.add_argument('root', nargs='?', default='.', help="Root directory to scan (default: .)")
    parser.add_argument('--year', type=int, default=datetime.date.today().year,
                         help="Current year to use (default: system year)")
    parser.add_argument('--dry-run', action='store_true', help="Preview changes without writing files")
    args = parser.parse_args()

    root = args.root
    current_year = args.year

    print(f"Scanning '{root}' for *.c/*.h files (current year = {current_year})"
          f"{' [DRY RUN]' if args.dry_run else ''}\n")

    total_files = 0
    changed_files = 0
    missing_tag_files = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not should_skip_dir(d)]
        for fname in filenames:
            if fname.endswith(('.c', '.h')):
                total_files += 1
                fpath = os.path.join(dirpath, fname)
                changed, found_tag = process_file(fpath, current_year, args.dry_run)
                if changed:
                    changed_files += 1
                if not found_tag:
                    missing_tag_files.append(fpath)

    print(f"\nScanned {total_files} files, updated {changed_files}.")
    if missing_tag_files:
        print(f"\n{len(missing_tag_files)} file(s) had NO @copyright tag at all "
              f"(not touched, review manually):")
        for f in missing_tag_files:
            print(f"  - {os.path.relpath(f)}")


if __name__ == '__main__':
    sys.exit(main())