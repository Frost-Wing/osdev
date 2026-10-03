#!/usr/bin/env python3
# flatten.py - replace every symlink in a rootfs with a real file/dir
# usage: sudo ./flatten.py ./fs_root [--hardlink]
import os, shutil, sys

root = os.path.realpath(sys.argv[1])
hard = "--hardlink" in sys.argv

def resolve(path):
    """Resolve all symlinks in path, treating root as '/'. None if loop."""
    todo = os.path.relpath(path, root).split(os.sep)[::-1]
    cur, hops = root, 0
    while todo:
        p = todo.pop()
        if p in ("", "."):
            continue
        if p == "..":
            if cur != root:
                cur = os.path.dirname(cur)
            continue
        nxt = os.path.join(cur, p)
        if os.path.islink(nxt):
            hops += 1
            if hops > 40:
                return None
            t = os.readlink(nxt)
            if os.path.isabs(t):
                cur = root
            todo.extend(t.split("/")[::-1])
        else:
            cur = nxt
    return cur

for _ in range(10):  # nested links need several passes
    links = []
    for d, dirs, files in os.walk(root):
        for n in dirs + files:
            p = os.path.join(d, n)
            if os.path.islink(p):
                links.append(p)
    if not links:
        break
    for p in links:
        if not os.path.islink(p):
            continue
        t = resolve(p)
        if t is None or not os.path.lexists(t):
            print("dangling/loop, removing:", p)
            os.unlink(p)
            continue
        if os.path.isdir(t) and p.startswith(t + os.sep):
            print("points at its own parent, removing:", p)
            os.unlink(p)
            continue
        os.unlink(p)
        try:
            if os.path.isdir(t):
                shutil.copytree(t, p, symlinks=True)
            elif hard:
                os.link(t, p)
            else:
                shutil.copy2(t, p)
        except OSError as e:
            print("failed:", p, e)

left = sum(1 for d, ds, fs in os.walk(root) for n in ds + fs
           if os.path.islink(os.path.join(d, n)))
print("symlinks remaining:", left)