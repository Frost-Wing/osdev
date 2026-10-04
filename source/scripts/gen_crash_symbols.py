#!/usr/bin/env python3
import argparse, bisect, os, re, subprocess, sys
from functools import lru_cache
from pathlib import Path

ROOT_MARKERS = ("kernel/", "drivers/", "includes/")

def q(s):
    return '"' + s.replace('\\','\\\\').replace('"','\\"').replace('\t','    ').rstrip('\n') + '"'

@lru_cache(maxsize=None)
def rel(path, root):
    """Resolve a path as it appears in DWARF/nm output to a path relative to `root`.

    Relative paths recorded in debug info are relative to the compiler's
    working directory (normally the project root for this build) - NOT
    necessarily this script's own CWD. Resolve against `root` explicitly.

    A resolved candidate is only accepted if it actually exists on disk.
    DWARF `decodedline` rows are frequently a bare basename (e.g. "vfs.c")
    with no directory component - the directory only lives on the CU
    header line. Without an existence check, `root / "vfs.c"` would
    "resolve" successfully even when no such file exists at the project
    root, silently returning the wrong path.

    Memoized: the same few hundred paths repeat across tens of thousands
    of DWARF rows, so the filesystem is only touched once per unique path.
    """
    if not path or path == '??':
        return None
    p = Path(path)
    root_r = root.resolve()
    try:
        resolved = p if p.is_absolute() else (root / p)
        resolved = resolved.resolve()
        if resolved.is_file():
            return resolved.relative_to(root_r).as_posix()
    except Exception:
        pass
    s = path.replace('\\', '/')
    for m in ROOT_MARKERS:
        i = s.find(m)
        if i >= 0:
            return s[i:]
    return None

_basename_index = None

def _build_basename_index(root):
    """One-time full walk of the tree, building basename -> [paths]."""
    index = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            index.setdefault(fn, []).append(Path(dirpath) / fn)
    return index

def rglob_basename(root, name, verbose=False):
    """Last-resort fallback: search the tree for a file with this basename."""
    if not name:
        return None
    global _basename_index
    if _basename_index is None:
        _basename_index = _build_basename_index(root)
    matches = _basename_index.get(Path(name).name)
    if not matches:
        return None
    if len(matches) > 1 and verbose:
        print(f"[gen_crash_symbols] warning: multiple files named "
              f"'{Path(name).name}', picking {matches[0]}", file=sys.stderr)
    return matches[0].relative_to(root).as_posix()

# Each source file is read and split once; each (file, line) snippet is built once.
_file_cache = {}
_snip_cache = {}

def file_lines(root, rp):
    lines = _file_cache.get(rp)
    if lines is None:
        try:
            lines = (root / rp).read_text(errors='replace').splitlines()
        except Exception:
            lines = []
        _file_cache[rp] = lines
    return lines

def snippet_for(root, rp, line, ctx=3):
    key = (rp, line)
    s = _snip_cache.get(key)
    if s is None:
        lines = file_lines(root, rp)
        start = max(1, line - ctx)
        end = min(len(lines), line + ctx)
        s = [(n, lines[n-1][:160]) for n in range(start, end + 1)]
        _snip_cache[key] = s
    return s

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--kernel', required=True); ap.add_argument('--root', required=True); ap.add_argument('--out', required=True)
    ap.add_argument('-v', '--verbose', action='store_true', help='print resolution stats/warnings to stderr')
    a=ap.parse_args(); root=Path(a.root)
    nm=subprocess.check_output(['nm','-n',a.kernel], text=True, errors='replace')
    funcs=[]
    for line in nm.splitlines():
        parts=line.split()
        if len(parts)>=3 and parts[1].lower()=='t': funcs.append((int(parts[0],16), parts[2]))
    addrs=[x[0] for x in funcs]
    entries=[]; snippets={}; snip_names={}

    # Prefer DWARF decoded line rows so a saved RIP resolves to the exact C line,
    # not merely the function entry line. Each row covers [address,next_address).
    decoded = subprocess.check_output(['objdump','--dwarf=decodedline',a.kernel], text=True, errors='replace')
    rows=[]; current_file=None
    row_re=re.compile(r'^(.+?)\s+(\d+)\s+(0x[0-9a-fA-F]+)')
    total_rows=0; dropped_rows=0

    for raw in decoded.splitlines():
        line=raw.rstrip()

        # CU headers look like "CU: ./path/to/file.c:".
        if line.startswith('CU:') and line.endswith(':'):
            current_file = line[len('CU:'):-1].strip()
            continue
        if line.endswith(':') and not line.startswith('Contents') and not line.startswith(a.kernel):
            current_file=line[:-1].strip()
            continue

        m=row_re.match(line.strip())
        if not m: continue
        file_name, line_s, addr_s = m.groups()
        try:
            lno=int(line_s); addr=int(addr_s,16)
        except Exception:
            continue
        if lno<=0: continue

        total_rows += 1
        rp = rel(file_name, root) or rel(current_file, root)
        if not rp and current_file and not Path(file_name).is_absolute() and '/' not in file_name.replace('\\', '/'):
            # `file_name` is a bare basename that didn't resolve directly and
            # didn't match the CU's primary file either. Try it joined with
            # the CU's own directory before giving up to rglob.
            candidate = Path(current_file).parent / Path(file_name).name
            rp = rel(str(candidate), root)
        if not rp:
            rp = rglob_basename(root, current_file, a.verbose) or rglob_basename(root, file_name, a.verbose)
        if not rp:
            dropped_rows += 1
            continue
        rows.append((addr,rp,lno))

    if a.verbose:
        print(f"[gen_crash_symbols] decoded {total_rows} line rows, "
              f"dropped {dropped_rows} (path unresolved)", file=sys.stderr)

    rows=sorted(set(rows))
    for i,(addr,rp,lno) in enumerate(rows):
        end_addr=rows[i+1][0] if i+1<len(rows) else addr+1
        if end_addr <= addr: continue
        idx=bisect.bisect_right(addrs, addr)-1
        func=funcs[idx][1] if idx>=0 else 'unknown'
        snip=snippet_for(root,rp,lno)
        key=(rp,lno)
        if key not in snip_names:
            snip_names[key]=f'snippet_{len(snip_names)}'
            snippets[snip_names[key]]=snip
        entries.append((addr,end_addr,func,rp,lno,snip_names[key],len(snip)))

    # Coalesce contiguous entries that map to the same function, file and line.
    # Lookup results are identical for every address; the table just gets smaller.
    merged=[]
    for e in entries:
        if merged:
            p=merged[-1]
            if p[2]==e[2] and p[3]==e[3] and p[4]==e[4] and p[1]==e[0]:
                merged[-1]=(p[0],e[1],p[2],p[3],p[4],p[5],p[6])
                continue
        merged.append(e)
    entries=merged

    # If DWARF line decoding produced nothing, fall back to one entry per text
    # symbol. The panic screen will still make clear when only limited mapping
    # exists instead of inventing data.
    if not entries:
        if a.verbose:
            print("[gen_crash_symbols] DWARF decode produced 0 usable rows, "
                  "falling back to nm/addr2line", file=sys.stderr)
        for i,(start,name) in enumerate(funcs):
            if name.startswith(('isr_stub_','irq_stub_')): continue
            end=funcs[i+1][0] if i+1<len(funcs) else start+1
            try:
                out=subprocess.check_output(['addr2line','-e',a.kernel,'-f','-C',hex(start)], text=True, errors='replace').splitlines()
            except Exception: continue
            func=out[0].strip() if out else name
            loc=out[1].strip() if len(out)>1 else '??:0'
            if loc.startswith('??'): continue
            filepart, _, linepart = loc.rpartition(':')
            try: lno=int(linepart.split()[0])
            except Exception: continue
            rp=rel(filepart, root) or rglob_basename(root, filepart, a.verbose)
            if not rp or lno<=0: continue
            snip=snippet_for(root,rp,lno)
            key=(rp,lno)
            if key not in snip_names:
                snip_names[key]=f'snippet_{len(snip_names)}'
                snippets[snip_names[key]]=snip
            entries.append((start,end,func,rp,lno,snip_names[key],len(snip)))

    if a.verbose:
        print(f"[gen_crash_symbols] wrote {len(entries)} symbol entries "
              f"covering the address space", file=sys.stderr)

    with open(a.out,'w') as f:
        f.write('/* Generated by scripts/gen_crash_symbols.py. */\n#include <crash_symbols.h>\n\n')
        for name,snip in snippets.items():
            f.write(f'static const CrashSourceLine {name}[] = {{\n')
            for n,t in snip: f.write(f'    {{{n}, {q(t)}}},\n')
            f.write('};\n')
        f.write('const CrashSymbol wing_crash_symbols[] = {\n')
        for st,en,fn,rp,lno,sn,ct in entries:
            f.write(f'    {{0x{st:x}ULL, 0x{en:x}ULL, {q(fn)}, {q(rp)}, {lno}, {sn}, {ct}}},\n')
        f.write('};\n')
        f.write(f'const uint32 wing_crash_symbol_count = {len(entries)};\n')
if __name__=='__main__': main()