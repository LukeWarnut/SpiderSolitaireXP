"""Turn split `.bss` pieces of gold .data back into uninitialized sections.

Gold .data has raw size 0x800 (file alignment) but virtual size 0x25B4; the
uninitialized tail starts at 0x010107C0 (g_strbuf). dtk only knows the raw
size, so a `rename:.bss` split that starts before 0x01010800 comes out as a
truncated initialized section. Its bytes are file padding and must be zero.
This rewrites the section header in place to an uninitialized section of the
full split size.
"""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

IMAGE_SCN_CNT_INITIALIZED_DATA = 0x40
IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x80

SPLIT_RE = re.compile(r"^\s+(\S+)\s+start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)(.*)$")


def bss_splits(splits_path: Path) -> dict[str, int]:
    sizes: dict[str, int] = {}
    unit = None
    for line in splits_path.read_text(encoding="utf-8").splitlines():
        if line and not line[0].isspace() and line.endswith(":"):
            unit = line[:-1]
            continue
        m = SPLIT_RE.match(line)
        if unit and m and "rename:.bss" in m.group(4):
            sizes[unit] = int(m.group(3), 16) - int(m.group(2), 16)
    return sizes


def fix_object(path: Path, size: int) -> bool:
    data = bytearray(path.read_bytes())
    nsect = struct.unpack_from("<H", data, 2)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    changed = False
    for i in range(nsect):
        off = 20 + optsz + i * 40
        if data[off : off + 8].rstrip(b"\0") != b".bss":
            continue
        raw_size, raw_ptr = struct.unpack_from("<II", data, off + 16)
        chars = struct.unpack_from("<I", data, off + 36)[0]
        if not chars & IMAGE_SCN_CNT_INITIALIZED_DATA:
            continue
        if any(data[raw_ptr : raw_ptr + raw_size]):
            sys.exit(f"{path}: .bss split has non-zero bytes")
        chars = (chars & ~IMAGE_SCN_CNT_INITIALIZED_DATA) | IMAGE_SCN_CNT_UNINITIALIZED_DATA
        struct.pack_into("<II", data, off + 16, size, 0)
        struct.pack_into("<I", data, off + 36, chars)
        changed = True
    if changed:
        path.write_bytes(bytes(data))
    return changed


def main() -> None:
    build_dir = Path(sys.argv[1])
    sizes = bss_splits(Path(sys.argv[2]))
    config = json.loads((build_dir / "config.json").read_text(encoding="utf-8"))
    for unit in config["units"]:
        size = sizes.get(unit["name"])
        if size is not None:
            fix_object(Path(unit["object"]), size)


if __name__ == "__main__":
    main()
