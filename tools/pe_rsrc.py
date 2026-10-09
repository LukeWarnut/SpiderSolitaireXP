"""Read the resource tree of a PE image."""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Union

Key = Union[int, str]

RT_CURSOR = 1
RT_BITMAP = 2
RT_ICON = 3
RT_MENU = 4
RT_DIALOG = 5
RT_STRING = 6
RT_ACCELERATOR = 9
RT_RCDATA = 10
RT_GROUP_CURSOR = 12
RT_GROUP_ICON = 14
RT_VERSION = 16
RT_MANIFEST = 24

TYPE_NAMES = {
    RT_CURSOR: "CURSOR", RT_BITMAP: "BITMAP", RT_ICON: "ICON", RT_MENU: "MENU",
    RT_DIALOG: "DIALOG", RT_STRING: "STRING", RT_ACCELERATOR: "ACCELERATOR",
    RT_RCDATA: "RCDATA", RT_GROUP_CURSOR: "GROUP_CURSOR", RT_GROUP_ICON: "GROUP_ICON",
    RT_VERSION: "VERSION", RT_MANIFEST: "MANIFEST",
}


@dataclass
class Resource:
    type: Key
    name: Key
    lang: int
    offset: int  # data offset from the start of the section
    data: bytes

    @property
    def key(self) -> tuple[Key, Key, int]:
        return (self.type, self.name, self.lang)

    def label(self) -> str:
        return f"{TYPE_NAMES.get(self.type, self.type)} {self.name} lang={self.lang:#06x}"


class ResourceSection:
    """The .rsrc section of a PE and every leaf of its resource directory.

    `resources` is in directory order (type, then name, then language, each
    sorted with named entries first). Sort by `offset` for the order the data
    was laid out in, which is the order of the .res the linker consumed.
    """

    def __init__(self, path: Path) -> None:
        d = Path(path).read_bytes()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        # PE32+ (x64) widens ImageBase and the stack/heap sizes, moving the
        # data directories 16 bytes further in.
        dirs = opt + (112 if struct.unpack_from("<H", d, opt)[0] == 0x20B else 96)
        self.dir_rva, self.dir_size = struct.unpack_from("<II", d, dirs + 2 * 8)
        for i in range(nsec):
            o = opt + optsz + 40 * i
            vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
            if va <= self.dir_rva < va + max(vs, rs):
                self.va, self.vsize = va, vs
                self.raw = bytes(d[rp : rp + min(vs, rs)])
                break
        else:
            raise ValueError(f"{path}: no resource section")
        self.resources: list[Resource] = []
        self._leaf_offsets: list[int] = []
        self._walk(0, ())

    def _walk(self, off: int, path: tuple) -> None:
        nn, ni = struct.unpack_from("<HH", self.raw, off + 12)
        for k in range(nn + ni):
            name, target = struct.unpack_from("<II", self.raw, off + 16 + 8 * k)
            if name & 0x80000000:
                p = name & 0x7FFFFFFF
                ln = struct.unpack_from("<H", self.raw, p)[0]
                key: Key = self.raw[p + 2 : p + 2 + 2 * ln].decode("utf-16-le")
            else:
                key = name
            if target & 0x80000000:
                self._walk(target & 0x7FFFFFFF, path + (key,))
                continue
            rva, size = struct.unpack_from("<II", self.raw, target)
            data_off = rva - self.va
            t, n = path
            self.resources.append(
                Resource(t, n, key, data_off, self.raw[data_off : data_off + size])
            )
            self._leaf_offsets.append(target)

    def find(self, type_: Key, name: Key) -> Resource:
        for r in self.resources:
            if r.type == type_ and r.name == name:
                return r
        raise KeyError((type_, name))

    def of_type(self, type_: Key) -> list[Resource]:
        return [r for r in self.resources if r.type == type_]

    def file_version(self) -> str | None:
        """FILEVERSION from VS_FIXEDFILEINFO, e.g. "5.1.2600.1106"."""
        for r in self.of_type(RT_VERSION):
            p = r.data.find(b"\xbd\x04\xef\xfe")
            if p >= 0:
                ms, ls = struct.unpack_from("<II", r.data, p + 8)
                return f"{ms >> 16}.{ms & 0xFFFF}.{ls >> 16}.{ls & 0xFFFF}"
        return None

    def languages(self) -> set[int]:
        return {r.lang for r in self.resources}

    def normalized(self) -> bytes:
        """Section bytes with each data-entry RVA made section-relative."""
        out = bytearray(self.raw)
        for leaf in self._leaf_offsets:
            rva = struct.unpack_from("<I", out, leaf)[0]
            struct.pack_into("<I", out, leaf, rva - self.va)
        return bytes(out)
