#!/usr/bin/env python3
"""Write a PE's resource tree as a Win32 .res file (input for cvtres)."""
import struct
import sys


def main(src: str, dst: str) -> None:
    d = open(src, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    opt = pe + 24
    secs = []
    for i in range(nsec):
        o = opt + optsz + 40 * i
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        secs.append((va, max(vs, rs), rp))

    def off(rva: int) -> int:
        for va, size, rp in secs:
            if va <= rva < va + size:
                return rva - va + rp
        raise ValueError(hex(rva))

    rsrc_rva = struct.unpack_from("<I", d, opt + 96 + 2 * 8)[0]
    base = off(rsrc_rva)

    def entries(dir_off: int):
        n_named, n_id = struct.unpack_from("<HH", d, base + dir_off + 12)
        for k in range(n_named + n_id):
            name, target = struct.unpack_from("<II", d, base + dir_off + 16 + 8 * k)
            if name & 0x80000000:
                p = base + (name & 0x7FFFFFFF)
                ln = struct.unpack_from("<H", d, p)[0]
                key = d[p + 2 : p + 2 + 2 * ln].decode("utf-16-le")
            else:
                key = name
            yield key, target

    def id_bytes(key) -> bytes:
        if isinstance(key, int):
            return struct.pack("<HH", 0xFFFF, key)
        return key.upper().encode("utf-16-le") + b"\0\0"

    out = bytearray()
    out += struct.pack("<IIHHHHIHHII", 0, 0x20, 0xFFFF, 0, 0xFFFF, 0, 0, 0, 0, 0, 0)
    for tkey, t in entries(0):
        for nkey, n in entries(t & 0x7FFFFFFF):
            for lkey, leaf in entries(n & 0x7FFFFFFF):
                rva, size, cp, _ = struct.unpack_from("<IIII", d, base + leaf)
                data = d[off(rva) : off(rva) + size]
                hdr = id_bytes(tkey) + id_bytes(nkey)
                if len(hdr) % 4:
                    hdr += b"\0" * (4 - len(hdr) % 4)
                hdr += struct.pack("<IHHII", 0, 0x1030, lkey, 0, 0)
                out += struct.pack("<II", size, 8 + len(hdr)) + hdr + data
                if len(out) % 4:
                    out += b"\0" * (4 - len(out) % 4)
    open(dst, "wb").write(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
