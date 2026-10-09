#!/usr/bin/env python3
"""Compile a unit with VC7 /O1 and compare masked .text to gold."""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.modules import gold_bytes, infer_module


def coff_text(data: bytes) -> bytes:
    """Largest .text section.

    An inline helper or a same-TU callee (any_empty) is often the first .text.
    The function under test is the larger one.
    """
    machine, nsect, _, ptrsym, nsym, optsz, _ = struct.unpack_from("<HHIIIHH", data, 0)
    off = 20 + optsz
    best = b""
    for _ in range(nsect):
        name = data[off : off + 8].split(b"\0", 1)[0]
        vsize, va, rawsz, ptr, prel, pline, nrel, nline, ch = struct.unpack_from(
            "<IIIIIIHHI", data, off + 8
        )
        if name.startswith(b".text") and ptr and rawsz > len(best):
            best = data[ptr : ptr + rawsz]
        off += 40
    return best


def mask_bytes(code: bytes) -> bytearray:
    out = bytearray(code)
    i = 0
    n = len(out)
    while i < n:
        b = out[i]
        if b in (0xE8, 0xE9) and i + 5 <= n:
            out[i + 1 : i + 5] = b"\0\0\0\0"
            i += 5
            continue
        if b == 0x68 and i + 5 <= n:
            out[i + 1 : i + 5] = b"\0\0\0\0"
            i += 5
            continue
        if b == 0xFF and i + 6 <= n and out[i + 1] in (0x15, 0x35):
            out[i + 2 : i + 6] = b"\0\0\0\0"
            i += 6
            continue
        # abs32 memory: modrm mod=0 rm=5
        if i + 6 <= n:
            modrm = out[i + 1]
            if (modrm & 0xC7) == 0x05 and b in (
                0xA1, 0xA3, 0x8B, 0x89, 0x8D, 0x3B, 0x39, 0x03, 0x2B,
                0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
            ):
                # A1/A3 are opcode+abs32 without modrm
                pass
        if b in (0xA1, 0xA3) and i + 5 <= n:
            out[i + 1 : i + 5] = b"\0\0\0\0"
            i += 5
            continue
        if i + 6 <= n and (out[i + 1] & 0xC7) == 0x05:
            if b in (
                0x8B, 0x89, 0x8D, 0x3B, 0x39, 0x03, 0x2B, 0x33, 0x23, 0x0B, 0x1B,
                0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
                0xF6, 0xF7, 0x80, 0x81, 0x83, 0xC6, 0xC7, 0xFF, 0x8B,
            ):
                out[i + 2 : i + 6] = b"\0\0\0\0"
                i += 6
                continue
        i += 1
    return out


def first_diff(a: bytes, b: bytes) -> int | None:
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return i
    if len(a) != len(b):
        return n
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=Path)
    ap.add_argument("dest_obj", type=Path)
    ap.add_argument("va")
    ap.add_argument("gold_size", nargs="?")
    ap.add_argument("--module", help="spider or cards (inferred from src path)")
    args = ap.parse_args()
    src = args.src
    obj = args.dest_obj
    va = int(args.va, 16)
    gold_size = int(args.gold_size, 0) if args.gold_size else None
    if args.module:
        from tools.modules import get_module
        mod = get_module(args.module)
    else:
        mod = infer_module(src)
    obj.parent.mkdir(parents=True, exist_ok=True)
    cflags = [
        sys.executable,
        str(ROOT / "tools" / "msvc.py"),
        "cl",
        "/nologo",
        *mod.cflags,
        "/c",
        str(src),
        f"/Fo{obj}",
    ]
    r = subprocess.run(cflags, cwd=ROOT, capture_output=True)
    sys.stderr.write(r.stdout.decode("latin1", "replace"))
    sys.stderr.write(r.stderr.decode("latin1", "replace"))
    if r.returncode != 0:
        return r.returncode
    text = coff_text(obj.read_bytes())
    gold = gold_bytes(mod, va, gold_size if gold_size else len(text))
    mt, mg = mask_bytes(text), mask_bytes(gold)
    d = first_diff(mt, mg)
    same = sum(1 for i in range(min(len(mt), len(mg))) if mt[i] == mg[i])
    denom = max(len(mt), len(mg), 1)
    print(f"our={len(text)} gold={len(gold)} masked_match={same}/{denom} ({100.0*same/denom:.1f}%)")
    if d is None:
        print("MASKED MATCH")
        return 0
    print(f"first_diff +{d:X}")
    lo = max(0, d - 12)
    hi = min(max(len(text), len(gold)), d + 24)
    print("our ", text[lo:hi].hex())
    print("gold", gold[lo:hi].hex())
    print("m_our ", mt[lo:hi].hex())
    print("m_gold", mg[lo:hi].hex())
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
