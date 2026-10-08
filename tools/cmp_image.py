#!/usr/bin/env python3
"""Byte-compare a rebuilt PE image with the original, ignoring bind and link time.

Both gold images were processed by bind.exe, which overwrites the IAT with
resolved addresses, stamps each import descriptor, adds a bound-import
directory to the header, and recomputes the checksum. The linker also writes
the build time into the file header, the export directory, and the debug
directory. None of that comes from source, so both images are normalized the
same way before comparing:

- each IAT is rewritten from its import name table (what the linker emitted);
- import descriptor TimeDateStamp / ForwarderChain are zeroed;
- the bound-import directory entry and its bytes are zeroed;
- CheckSum, the file header TimeDateStamp, the export TimeDateStamp, and the
  debug directory timestamps plus the CodeView signature/age are zeroed.

Every other byte must match. Prints IMAGE MATCH and exits 0, or reports where
the images differ and exits 1.

usage: cmp_image.py <gold> <ours>
       cmp_image.py --module spider|cards
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

DIR_EXPORT, DIR_IMPORT, DIR_DEBUG, DIR_BOUND = 0, 1, 6, 11

OPT_FIELDS = [
    (0, 2, "Magic"), (2, 1, "MajorLinkerVersion"), (3, 1, "MinorLinkerVersion"),
    (4, 4, "SizeOfCode"), (8, 4, "SizeOfInitializedData"), (12, 4, "SizeOfUninitializedData"),
    (16, 4, "AddressOfEntryPoint"), (20, 4, "BaseOfCode"), (24, 4, "BaseOfData"),
    (28, 4, "ImageBase"), (32, 4, "SectionAlignment"), (36, 4, "FileAlignment"),
    (40, 2, "MajorOSVersion"), (42, 2, "MinorOSVersion"), (44, 2, "MajorImageVersion"),
    (46, 2, "MinorImageVersion"), (48, 2, "MajorSubsystemVersion"), (50, 2, "MinorSubsystemVersion"),
    (52, 4, "Win32VersionValue"), (56, 4, "SizeOfImage"), (60, 4, "SizeOfHeaders"),
    (64, 4, "CheckSum"), (68, 2, "Subsystem"), (70, 2, "DllCharacteristics"),
    (72, 4, "SizeOfStackReserve"), (76, 4, "SizeOfStackCommit"), (80, 4, "SizeOfHeapReserve"),
    (84, 4, "SizeOfHeapCommit"), (88, 4, "LoaderFlags"), (92, 4, "NumberOfRvaAndSizes"),
]
DIR_NAMES = [
    "Export", "Import", "Resource", "Exception", "Security", "BaseReloc", "Debug",
    "Architecture", "GlobalPtr", "TLS", "LoadConfig", "BoundImport", "IAT",
    "DelayImport", "COMDescriptor", "Reserved",
]


class Image:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = bytearray(path.read_bytes())
        d = self.data
        self.pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[self.pe : self.pe + 4] != b"PE\0\0":
            sys.exit(f"{path}: not a PE image")
        self.nsec = struct.unpack_from("<H", d, self.pe + 6)[0]
        self.optsz = struct.unpack_from("<H", d, self.pe + 20)[0]
        self.opt = self.pe + 24
        self.sectab = self.opt + self.optsz
        self.sections = []
        for i in range(self.nsec):
            o = self.sectab + 40 * i
            name = bytes(d[o : o + 8]).rstrip(b"\0").decode("latin1")
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
            self.sections.append((name, vsize, va, rawsize, rawptr))
        self.image_base = struct.unpack_from("<I", d, self.opt + 28)[0]
        self.headers_end = struct.unpack_from("<I", d, self.opt + 60)[0]

    def directory(self, index: int) -> tuple[int, int]:
        return struct.unpack_from("<II", self.data, self.opt + 96 + 8 * index)

    def offset(self, rva: int) -> int | None:
        if rva < self.headers_end:
            return rva
        for _, vsize, va, rawsize, rawptr in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                if rva - va >= rawsize:
                    return None
                return rawptr + rva - va
        return None

    def zero(self, off: int | None, size: int) -> None:
        if off is not None:
            self.data[off : off + size] = b"\0" * size

    def normalize(self) -> None:
        d = self.data
        struct.pack_into("<I", d, self.pe + 8, 0)
        struct.pack_into("<I", d, self.opt + 64, 0)

        bound_rva, bound_size = self.directory(DIR_BOUND)
        if bound_rva:
            self.zero(self.offset(bound_rva), bound_size)
        struct.pack_into("<II", d, self.opt + 96 + 8 * DIR_BOUND, 0, 0)

        imp_rva, _ = self.directory(DIR_IMPORT)
        desc = self.offset(imp_rva) if imp_rva else None
        while desc is not None:
            oft, _, _, name, ft = struct.unpack_from("<IIIII", d, desc)
            if not (oft or name or ft):
                break
            struct.pack_into("<II", d, desc + 4, 0, 0)
            src, dst = self.offset(oft), self.offset(ft)
            if oft and src is not None and dst is not None:
                while True:
                    thunk = struct.unpack_from("<I", d, src)[0]
                    struct.pack_into("<I", d, dst, thunk)
                    if not thunk:
                        break
                    src += 4
                    dst += 4
            desc += 20

        exp_rva, _ = self.directory(DIR_EXPORT)
        if exp_rva:
            self.zero(self.offset(exp_rva + 4), 4)

        dbg_rva, dbg_size = self.directory(DIR_DEBUG)
        dbg = self.offset(dbg_rva) if dbg_rva else None
        for i in range(dbg_size // 28 if dbg is not None else 0):
            entry = dbg + 28 * i
            struct.pack_into("<I", d, entry + 4, 0)
            kind, size, _, rawptr = struct.unpack_from("<IIII", d, entry + 12)
            if kind == 2 and rawptr and size >= 8:
                magic = bytes(d[rawptr : rawptr + 4])
                if magic == b"RSDS":
                    self.zero(rawptr + 4, 20)
                elif magic == b"NB10":
                    self.zero(rawptr + 8, 8)


def header_label(img: Image, off: int) -> str:
    if off < img.pe:
        return "DOS header/stub/Rich"
    if off < img.opt:
        return "file header"
    if off < img.opt + 96:
        rel = off - img.opt
        for start, size, name in OPT_FIELDS:
            if start <= rel < start + size:
                return name
        return "optional header"
    if off < img.sectab:
        return f"DataDirectory[{DIR_NAMES[(off - img.opt - 96) // 8]}]"
    if off < img.sectab + 40 * img.nsec:
        return f"section header {img.sections[(off - img.sectab) // 40][0]}"
    return "header padding"


def runs(diffs: list[int]) -> list[tuple[int, int]]:
    out: list[list[int]] = []
    for i in diffs:
        if out and i == out[-1][1] + 1:
            out[-1][1] = i
        else:
            out.append([i, i])
    return [(a, b - a + 1) for a, b in out]


def compare(gold: Image, ours: Image) -> bool:
    gold.normalize()
    ours.normalize()
    if gold.data == ours.data:
        print(f"IMAGE MATCH ({len(gold.data):#x} bytes)")
        return True

    print(f"MISMATCH: gold {len(gold.data):#x} bytes, ours {len(ours.data):#x} bytes")
    gs = [(s[0], s[1], s[2], s[3], s[4]) for s in gold.sections]
    os_ = [(s[0], s[1], s[2], s[3], s[4]) for s in ours.sections]
    if gs != os_:
        print("section table differs (name vsize va rawsize rawptr):")
        for side, secs in (("gold", gs), ("ours", os_)):
            print(f"  {side}: " + ", ".join(f"{n} {v:#x}@{a:#x} {r:#x}@{p:#x}" for n, v, a, r, p in secs))

    hdr_end = min(gold.headers_end, ours.headers_end, len(gold.data), len(ours.data))
    hdr_diffs = [i for i in range(hdr_end) if gold.data[i] != ours.data[i]]
    if hdr_diffs:
        labels: dict[str, None] = {}
        for i in hdr_diffs:
            labels.setdefault(header_label(gold, i), None)
        print(f"headers: {len(hdr_diffs)} byte(s) differ in: {', '.join(labels)}")

    ours_by_name = {s[0]: s for s in ours.sections}
    for name, vsize, va, rawsize, rawptr in gold.sections:
        other = ours_by_name.get(name)
        if other is None:
            print(f"{name}: missing from ours")
            continue
        a = gold.data[rawptr : rawptr + rawsize]
        b = ours.data[other[4] : other[4] + other[3]]
        n = min(len(a), len(b), max(vsize, other[1]))
        diffs = [i for i in range(n) if a[i] != b[i]]
        size_note = "" if vsize == other[1] else f" (vsize gold {vsize:#x}, ours {other[1]:#x})"
        if not diffs and not size_note:
            print(f"{name}: match")
            continue
        first = ", ".join(f"{gold.image_base + va + o:#x}+{k}" for o, k in runs(diffs)[:6])
        print(f"{name}: {len(diffs)} byte(s) differ{size_note}" + (f"; first runs {first}" if diffs else ""))
    for name in ours_by_name:
        if name not in {s[0] for s in gold.sections}:
            print(f"{name}: only in ours")
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("gold", nargs="?", type=Path)
    ap.add_argument("ours", nargs="?", type=Path)
    ap.add_argument("--module", help="compare orig and build output of this module")
    args = ap.parse_args()
    if args.module:
        from tools.modules import get_module

        mod = get_module(args.module)
        gold_path, ours_path = mod.orig, mod.output
    elif args.gold and args.ours:
        gold_path, ours_path = args.gold, args.ours
    else:
        ap.error("give <gold> <ours> or --module")
    return 0 if compare(Image(gold_path), Image(ours_path)) else 1


if __name__ == "__main__":
    sys.exit(main())
