#!/usr/bin/env python3
"""Compile a unit with Wine MSVC /O1 and compare masked .text to gold spider.exe."""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "orig/XPSP1/spider.exe"


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
    if len(sys.argv) < 4:
        print("usage: cmp_fn.py src dest_obj va [gold_size]")
        return 2
    src = Path(sys.argv[1])
    obj = Path(sys.argv[2])
    va = int(sys.argv[3], 16)
    gold_size = int(sys.argv[4], 0) if len(sys.argv) > 4 else None
    obj.parent.mkdir(parents=True, exist_ok=True)
    cflags = [
        str(ROOT / "tools/wine_msvc.sh"),
        "cl",
        "/nologo",
        "/W3",
        "/wd4234",
        "/MT",
        "/GR-",
        "/DUNICODE",
        "/D_UNICODE",
        "/DWIN32",
        "/D_WINDOWS",
        "/DNDEBUG",
        "/O1",
        "/I",
        "include",
        "/I",
        "orig/toolchain/inc",
        "/I",
        "orig/toolchain/inc/wxp",
        "/I",
        "orig/toolchain/inc/crt",
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
    exe = EXE.read_bytes()
    off = 0x400 + (va - 0x01001000)
    gold = exe[off : off + (gold_size if gold_size else len(text))]
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
