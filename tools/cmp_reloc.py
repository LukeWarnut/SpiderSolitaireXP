#!/usr/bin/env python3
"""Compare a compiled .text section with gold bytes, masking only at COFF relocation sites.

cmp_fn.py masks by opcode pattern, so a 0xE8 or 0x68 inside a ModRM/displacement
(`lea eax, [ebp-0x18]` is 8D 45 E8) shifts the mask onto the wrong bytes. Here the
mask comes from the object's own relocation table, so a clean result means the only
differences are link-time addresses.

usage: cmp_reloc.py <obj> <gold_addr> <size> [symbol]
"""
import struct
import sys
from pathlib import Path

GOLD = Path("orig/XPSP1/spider.exe")
IMAGE_BASE = 0x01000000


def gold_bytes(addr: int, size: int) -> bytes:
    d = GOLD.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    rva = addr - IMAGE_BASE
    for i in range(nsec):
        o = pe + 24 + optsz + 40 * i
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        if va <= rva < va + vs:
            return d[rp + rva - va : rp + rva - va + size]
    raise SystemExit(f"{addr:#x} not in gold image")


def text_sections(obj: bytes):
    nsec = struct.unpack_from("<H", obj, 2)[0]
    symptr, nsym = struct.unpack_from("<II", obj, 8)
    optsz = struct.unpack_from("<H", obj, 16)[0]
    strtab = symptr + nsym * 18

    def sym_name(i: int) -> str:
        o = symptr + i * 18
        if obj[o : o + 4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", obj, o + 4)[0]
            end = obj.index(b"\0", strtab + off)
            return obj[strtab + off : end].decode()
        return obj[o : o + 8].rstrip(b"\0").decode()

    names = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        secnum = struct.unpack_from("<h", obj, o + 12)[0]
        sclass = obj[o + 16]
        naux = obj[o + 17]
        is_func = struct.unpack_from("<H", obj, o + 14)[0] == 0x20
        if (sclass == 2 or (sclass == 3 and is_func)) and secnum > 0 and struct.unpack_from("<I", obj, o + 8)[0] == 0:
            names.setdefault(secnum, sym_name(i))
        i += 1 + naux
    for s in range(nsec):
        o = 20 + optsz + 40 * s
        name = obj[o : o + 8].rstrip(b"\0")
        if not name.startswith(b".text"):
            continue
        rs, rp, rlp = struct.unpack_from("<III", obj, o + 16)
        nrel = struct.unpack_from("<H", obj, o + 32)[0]
        relocs = [struct.unpack_from("<I", obj, rlp + 10 * k)[0] for k in range(nrel)]
        yield names.get(s + 1, "?"), obj[rp : rp + rs], relocs


def main() -> int:
    obj = Path(sys.argv[1]).read_bytes()
    addr = int(sys.argv[2], 0)
    size = int(sys.argv[3], 0)
    want = sys.argv[4] if len(sys.argv) > 4 else None
    gold = gold_bytes(addr, size)
    secs = list(text_sections(obj))
    if want:
        secs = [s for s in secs if s[0] == want]
    else:
        secs.sort(key=lambda s: (len(s[1]) != size, -len(s[1])))
    if not secs:
        print("no matching .text section")
        return 1
    name, ours, relocs = secs[0]
    mask = set()
    for r in relocs:
        mask.update(range(r, r + 4))
    diffs = [i for i in range(min(len(ours), len(gold))) if i not in mask and ours[i] != gold[i]]
    print(f"{name}: ours={len(ours)} gold={len(gold)} relocs={len(relocs)} unmasked_diffs={len(diffs)}")
    if diffs:
        print("first diff at", hex(diffs[0]))
    ok = len(ours) == len(gold) and not diffs
    print("RELOC MATCH" if ok else "MISMATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
