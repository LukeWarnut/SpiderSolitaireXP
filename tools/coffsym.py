"""Read the defined function symbol from an MSVC COFF object."""

from __future__ import annotations

import struct
from pathlib import Path


def defined_function_symbols(path: Path) -> list[str]:
    if not path.is_file():
        return []
    data = path.read_bytes()
    if len(data) < 20:
        return []
    _machine, nsect, _timedate, ptrsym, nsym, optsz, _chars = struct.unpack_from(
        "<HHIIIHH", data, 0
    )
    if ptrsym <= 0 or nsym <= 0 or ptrsym + nsym * 18 > len(data):
        return []
    str_off = ptrsym + nsym * 18

    def name_at(index: int) -> str:
        ent = ptrsym + index * 18
        raw = data[ent : ent + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            return data[str_off + off :].split(b"\0", 1)[0].decode("latin1")
        return raw.split(b"\0", 1)[0].decode("latin1")

    text_sections: set[int] = set()
    off = 20 + optsz
    for section in range(nsect):
        o = off + section * 40
        name = data[o : o + 8].split(b"\0", 1)[0].decode("latin1")
        if name.startswith(".text"):
            text_sections.add(section + 1)

    found: list[tuple[int, str]] = []
    index = 0
    while index < nsym:
        ent = ptrsym + index * 18
        name = name_at(index)
        value, scnum, _typ, sclass, naux = struct.unpack_from("<IhHBB", data, ent + 8)
        if (
            sclass == 2
            and scnum in text_sections
            and name
            and not name.startswith(".")
            and not name.startswith("$")
            and not name.startswith("@")
        ):
            found.append((value, name))
        index += 1 + naux
    found.sort()
    return [name for _value, name in found]


def defined_function_symbol(path: Path) -> str | None:
    names = defined_function_symbols(path)
    if not names:
        return None
    return names[0]

    if not path.is_file():
        return None
    data = path.read_bytes()
    if len(data) < 20:
        return None
    _machine, nsect, _timedate, ptrsym, nsym, optsz, _chars = struct.unpack_from(
        "<HHIIIHH", data, 0
    )
    if ptrsym <= 0 or nsym <= 0 or ptrsym + nsym * 18 > len(data):
        return None
    str_off = ptrsym + nsym * 18

    def name_at(index: int) -> str:
        ent = ptrsym + index * 18
        raw = data[ent : ent + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            return data[str_off + off :].split(b"\0", 1)[0].decode("latin1")
        return raw.split(b"\0", 1)[0].decode("latin1")

    text_sections: set[int] = set()
    off = 20 + optsz
    for section in range(nsect):
        o = off + section * 40
        name = data[o : o + 8].split(b"\0", 1)[0].decode("latin1")
        if name.startswith(".text"):
            text_sections.add(section + 1)

    found: list[tuple[int, str]] = []
    index = 0
    while index < nsym:
        ent = ptrsym + index * 18
        name = name_at(index)
        value, scnum, _typ, sclass, naux = struct.unpack_from("<IhHBB", data, ent + 8)
        if (
            sclass == 2
            and scnum in text_sections
            and name
            and not name.startswith(".")
            and not name.startswith("$")
            and not name.startswith("@")
        ):
            found.append((value, name))
        index += 1 + naux
    if not found:
        return None
    found.sort()
    return found[0][1]
