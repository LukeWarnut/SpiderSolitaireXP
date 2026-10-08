#!/usr/bin/env python3
"""Identify unnamed gold CRT functions against a static CRT library.

usage: crt_ident.py [lib] [unit-regex]

Every external code symbol in every member of ``lib`` (default libc.lib) is
compared with each ``crt_fn_*`` unit in units.json. Bytes covered by the
member's own relocations are masked; everything else must be identical.
"""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.modules import get_module, gold_bytes

DEFAULT_LIB = ROOT / "orig/toolchain/lib/wxp/i386/libc.lib"


def ar_members(data: bytes):
    assert data[:8] == b"!<arch>\n"
    pos = 8
    longnames = b""
    while pos + 60 <= len(data):
        hdr = data[pos : pos + 60]
        name = hdr[:16].rstrip()
        size = int(hdr[48:58])
        body = data[pos + 60 : pos + 60 + size]
        if name == b"//":
            longnames = body
        elif name != b"/":
            if name.startswith(b"/") and name[1:].isdigit():
                off = int(name[1:])
                name = longnames[off:].split(b"\0", 1)[0].split(b"\n", 1)[0]
            yield name.decode("latin1").rstrip("/"), body
        pos += 60 + size + (size & 1)


def coff_functions(obj: bytes):
    if len(obj) < 20:
        return
    machine, nsect, _, ptrsym, nsym, optsz, _ = struct.unpack_from("<HHIIIHH", obj, 0)
    if machine != 0x14C:
        return
    sects = []
    for i in range(nsect):
        o = 20 + optsz + i * 40
        name = obj[o : o + 8].rstrip(b"\0")
        size, raw, rel, _, nrel, _, chars = struct.unpack_from("<IIIIHHI", obj, o + 16)
        masked = set()
        for r in range(nrel):
            va, _, typ = struct.unpack_from("<IIH", obj, rel + r * 10)
            if typ in (6, 7, 0x14):
                masked.update(range(va, va + 4))
        sects.append((name, obj[raw : raw + size] if raw else b"", masked, chars))
    str_off = ptrsym + nsym * 18

    def sym_name(ent: int) -> str:
        raw = obj[ent : ent + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            return obj[str_off + off :].split(b"\0", 1)[0].decode("latin1")
        return raw.split(b"\0", 1)[0].decode("latin1")

    by_sect: dict[int, list[tuple[int, str, bool]]] = {}
    i = 0
    while i < nsym:
        ent = ptrsym + i * 18
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", obj, ent + 8)
        if secnum > 0 and cls in (2, 3, 6) and not sym_name(ent).startswith("."):
            by_sect.setdefault(secnum, []).append((value, sym_name(ent), cls == 2))
        i += 1 + naux
    for secnum, syms in by_sect.items():
        name, data, masked, chars = sects[secnum - 1]
        if not chars & 0x20:
            continue
        syms.sort()
        for k, (value, sname, ext) in enumerate(syms):
            if not ext and sname.startswith("$"):
                continue
            end = len(data)
            for v2, _, _ in syms[k + 1 :]:
                if v2 > value:
                    end = v2
                    break
            body = data[value:end]
            mask = {m - value for m in masked if value <= m < end}
            yield sname, body, mask
        if len(syms) > 1 and syms[0][0] == 0:
            yield "+".join(s for _, s, e in syms if e), data, masked


def score(gold: bytes, body: bytes, mask: set[int]) -> float:
    body = body.rstrip(b"\xcc") or body
    if len(body) > len(gold):
        return 0.0
    same = sum(1 for i, b in enumerate(body) if i in mask or gold[i] == b)
    return same / len(body) if body else 0.0


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("lib", nargs="?", type=Path, default=DEFAULT_LIB)
    ap.add_argument("unit_regex", nargs="?", default=r"^crt_fn_")
    ap.add_argument("--module", default="spider")
    args = ap.parse_args()
    lib = args.lib
    pat = re.compile(args.unit_regex)
    mod = get_module(args.module)
    units = json.loads(mod.units_json.read_text())
    funcs = []
    for member, obj in ar_members(lib.read_bytes()):
        for sname, body, mask in coff_functions(obj):
            if len(body) >= 3:
                funcs.append((Path(member.replace("\\", "/")).name, sname, body, mask))
    for u in units:
        if not pat.search(u["name"]) or "addr" not in u:
            continue
        gold = gold_bytes(mod, u["addr"], u["size"])
        best = max(funcs, key=lambda f: (score(gold, f[2], f[3]), len(f[2])))
        s = score(gold, best[2], best[3])
        blen = len(best[2].rstrip(b"\xcc"))
        exact = s == 1.0 and set(gold[blen:]) <= {0xCC}
        print(f"{u['name']:24} 0x{u['addr']:08x} size {u['size']:5}  "
              f"{s*100:6.2f}%  {'EXACT' if exact else '     '} "
              f"{best[1]} ({best[0]}, {blen} bytes)")


if __name__ == "__main__":
    main()
