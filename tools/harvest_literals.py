#!/usr/bin/env python3
"""Name gold rdata constants after the MSVC literal symbols our objects reference.

dtk splits gold .rdata as one `text_rdata` blob, so a gold `push text_rdata+0x378`
never pairs with our `push ??_C@_15...@` (string) or `fld [__real@...]` (float).
This walks objdiff's per-function diff for every unit below 100%, collects
instructions where the gold side targets `text_rdata+X` and ours targets a
compiler literal, and prints `symbols.txt` lines placing that literal at X.

usage: harvest_literals.py [--write]
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RDATA_BASE = 0x01001280
RDATA_END = 0x01002660
OBJDIFF = ROOT / "build/tools/objdiff-cli"
SYMBOLS = ROOT / "config/XPSP1/symbols.txt"
LITERAL_PREFIXES = ("??_C@", "__real@", "__xmm@")


def reloc_target(side: dict, ins: dict):
    rel = ins["instruction"].get("relocation")
    if not rel:
        return None
    sym = side["symbols"][rel["target_symbol"]]
    return sym["name"], int(rel.get("addend", "0"))


def unit_pairs(unit: str, fn: str):
    out = subprocess.run(
        [str(OBJDIFF), "diff", "-p", ".", "-u", unit, "-o", "-", fn],
        cwd=ROOT, capture_output=True, text=True,
    )
    if out.returncode != 0 or not out.stdout.strip():
        return
    d = json.loads(out.stdout)
    if "left" not in d or "right" not in d:
        return
    left = next((s for s in d["left"]["symbols"] if s.get("name") == fn), None)
    right = next((s for s in d["right"]["symbols"] if s.get("name") == fn), None)
    if not left or not right:
        return
    for li, ri in zip(left.get("instructions", []), right.get("instructions", [])):
        if "instruction" not in li or "instruction" not in ri:
            continue
        lt = reloc_target(d["left"], li)
        rt = reloc_target(d["right"], ri)
        if not rt or not rt[0].startswith(LITERAL_PREFIXES):
            continue
        if lt and lt[0] == "text_rdata":
            yield RDATA_BASE + lt[1] - rt[1], rt[0], unit
        elif not lt:
            # Once a literal inside text_rdata is named, dtk leaves later
            # references as bare immediates.
            for m in re.findall(r"0x[0-9a-f]+", li["instruction"]["formatted"]):
                v = int(m, 16)
                if RDATA_BASE <= v < RDATA_END:
                    yield v - rt[1], rt[0], unit


def gold_text() -> tuple[bytes, int]:
    d = (ROOT / "orig/XPSP1/spider.exe").read_bytes()
    pe = int.from_bytes(d[0x3C:0x40], "little")
    sec = pe + 24 + int.from_bytes(d[pe + 20 : pe + 22], "little")
    va = int.from_bytes(d[sec + 12 : sec + 16], "little")
    raw = int.from_bytes(d[sec + 20 : sec + 24], "little")
    return d, raw - va - 0x01000000


def literal_size(name: str, addr: int) -> int:
    """dtk drops the relocation when the target symbol has no size."""
    if name.startswith("__real@"):
        return (len(name) - len("__real@")) // 2
    if name.startswith("__xmm@"):
        return 16
    d, delta = gold_text()
    off = addr + delta
    wide = name.startswith("??_C@_1")
    step = 2 if wide else 1
    end = off
    while d[end : end + step] != b"\0" * step:
        end += step
    return end - off + step


def main() -> int:
    report = json.loads((ROOT / "build/XPSP1/report.json").read_text())
    found = defaultdict(set)
    for u in report["units"]:
        for f in u.get("functions", []):
            if True:
                for addr, name, unit in unit_pairs(u["name"], f["name"]):
                    found[(addr, name)].add(unit)
    existing = SYMBOLS.read_text().splitlines()
    have = {l.split(" = ", 1)[0] for l in existing}
    by_name = defaultdict(set)
    for addr, name in found:
        by_name[name].add(addr)
    new = []
    for (addr, name), units in sorted(found.items()):
        if len(by_name[name]) > 1:
            print(f"# conflict {name} at {sorted(hex(a) for a in by_name[name])}", file=sys.stderr)
            continue
        if name in have:
            continue
        line = f"{name} = .text:0x{addr:08x}; // type:object size:{literal_size(name, addr):#x}"
        print(line, "#", ",".join(sorted(units)))
        new.append(line)
    if "--write" in sys.argv and new:
        idx = next(i for i, l in enumerate(existing) if l.startswith("text_rdata ="))
        existing[idx + 1 : idx + 1] = new
        SYMBOLS.write_text("\n".join(existing) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
