#!/usr/bin/env python3
"""Compare the .rsrc section of a rebuilt PE with the original.

The section can sit at a different RVA in the rebuild (other sections differ
in size), so every data-entry RVA is taken relative to the section start
before comparing. Reports, per resource, whether the data bytes and the
data placement match, then the first differing byte of the whole section.

usage: cmp_rsrc.py <gold.exe> <ours.exe>
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.pe_rsrc import ResourceSection  # noqa: E402


def main() -> int:
    gold = ResourceSection(Path(sys.argv[1]))
    ours = ResourceSection(Path(sys.argv[2]))
    theirs = {r.key: r for r in ours.resources}
    bad = 0
    for g in gold.resources:
        o = theirs.pop(g.key, None)
        if o is None:
            print(f"MISSING  {g.label()}")
            bad += 1
            continue
        what = [] if g.data == o.data else ["data"]
        if g.offset != o.offset:
            what.append(f"offset {g.offset:#x} vs {o.offset:#x}")
        if what:
            bad += 1
            print(f"DIFF     {g.label()}: {', '.join(what)}")
    for o in theirs.values():
        print(f"EXTRA    {o.label()}")
        bad += 1
    if gold.dir_size != ours.dir_size:
        print(f"directory size {gold.dir_size:#x} vs {ours.dir_size:#x}")
    if gold.vsize != ours.vsize:
        print(f"section size {gold.vsize:#x} vs {ours.vsize:#x}")
    g, o = gold.normalized(), ours.normalized()
    n = min(len(g), len(o))
    first = next((i for i in range(n) if g[i] != o[i]), None)
    if first is None and len(g) == len(o):
        print(f"RSRC MATCH ({len(gold.resources)} resources, {len(g):#x} bytes)")
        return 0
    if first is None:
        first = n
    same = sum(1 for i in range(n) if g[i] == o[i])
    print(f"{bad} resource(s) differ; section bytes equal {same}/{max(len(g), len(o))}, "
          f"first difference at +{first:#x}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
