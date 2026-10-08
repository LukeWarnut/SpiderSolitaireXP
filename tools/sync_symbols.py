#!/usr/bin/env python3
"""Copy compiled COFF symbols for renamed game functions into symbols.txt.

MSVC encodes the C++ declaration in the object symbol, not in the instruction
bytes. dtk names the split function from symbols.txt. Those strings have to
match or objdiff will not pair the function or its calls.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.coffsym import defined_function_symbols
from tools.modules import Module, get_module
from tools.names import load_names


def sync_symbols(
    module: Module | None = None,
    root: Path = ROOT,
    symbols_path: Path | None = None,
    names_path: Path | None = None,
    obj_dir: Path | None = None,
) -> int:
    module = module or get_module("spider")
    symbols_path = symbols_path or module.symbols
    names_path = names_path or module.names
    obj_dir = obj_dir or module.src_obj_dir
    if not symbols_path.is_file():
        return 0
    named = set(load_names(names_path))
    unit_symbol = {}
    units_path = symbols_path.parent / "units.json"
    if units_path.is_file():
        for unit in json.loads(units_path.read_text(encoding="utf-8")):
            if "addr" in unit and "symbol" in unit:
                unit_symbol[unit["addr"]] = unit["symbol"]
    lines = symbols_path.read_text(encoding="utf-8").splitlines()
    used = set()
    for line in lines:
        if " = .text:" in line and "type:function" in line:
            used.add(line.split(" = ", 1)[0])
    changed = 0
    for index, line in enumerate(lines):
        if " = .text:" not in line or "type:function" not in line:
            continue
        _old, rest = line.split(" = ", 1)
        addr = int(rest.split(":")[1].split(";")[0], 16)
        stub = f"fn_{addr:08X}"
        obj = obj_dir / f"{stub}.obj"
        names = defined_function_symbols(obj)
        if not names:
            continue
        preferred = unit_symbol.get(addr)
        if preferred in names and preferred != _old:
            lines[index] = f"{preferred} = {line.split(' = ', 1)[1]}"
            used.discard(_old)
            used.add(preferred)
            changed += 1
            print(f"{stub}: {_old} -> {preferred}")
            continue
        if _old in names:
            continue
        unused = [name for name in names if name not in used]
        symbol = unused[0] if unused else names[0]
        stub_symbol = symbol.startswith("fn_") or symbol.startswith("_fn_")
        if _old.startswith("?") and stub_symbol:
            continue
        if stub not in named and not symbol.startswith("?"):
            continue
        if symbol == _old:
            continue
        rhs = line.split(" = ", 1)[1]
        lines[index] = f"{symbol} = {rhs}"
        used.discard(_old)
        used.add(symbol)
        changed += 1
        print(f"{stub}: {_old} -> {symbol}")
    if changed:
        symbols_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return changed


def main() -> int:
    name = "spider"
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        name = sys.argv[1]
    n = sync_symbols(get_module(name))
    print(f"updated {n} symbol(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
