#!/usr/bin/env python3
"""Extract the media resources of a PE as standalone files.

    extract_assets.py <exe> <out_dir>               bitmaps, icons, sounds, manifest
    extract_assets.py <exe> <out_dir> --rc <file>   also decompile a resource script

configure.py runs the first form per module; the build compiles that module's
.rc with rc.exe, which picks the extracted files up through /i <out_dir>. The
second form regenerates the resource script from the original (menus, dialogs,
strings, accelerators, version info as text; media as references to the files
above).

Files that already hold the right bytes are not rewritten, so their mtimes do
not trigger a rebuild.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.pe_rsrc import (  # noqa: E402
    RT_ACCELERATOR,
    RT_BITMAP,
    RT_DIALOG,
    RT_GROUP_ICON,
    RT_ICON,
    RT_MANIFEST,
    RT_MENU,
    RT_STRING,
    RT_VERSION,
    Key,
    Resource,
    ResourceSection,
)

LANG_EN_US = 0x0409

# Primary language -> the ANSI code page rc decodes this script's \ooo escapes with.
ANSI_CODE_PAGES = {
    0x01: 1256, 0x02: 1251, 0x05: 1250, 0x06: 1252, 0x07: 1252, 0x08: 1253,
    0x09: 1252, 0x0A: 1252, 0x0B: 1252, 0x0C: 1252, 0x0D: 1255, 0x0E: 1250,
    0x0F: 1252, 0x10: 1252, 0x11: 932, 0x12: 949, 0x13: 1252, 0x14: 1252,
    0x15: 1250, 0x16: 1252, 0x18: 1250, 0x19: 1251, 0x1A: 1250, 0x1B: 1250,
    0x1D: 1252, 0x1E: 874, 0x1F: 1254, 0x22: 1251, 0x24: 1250, 0x25: 1257,
    0x26: 1257, 0x27: 1257,
}


def code_page(lang: int) -> int:
    primary, sub = lang & 0x3FF, lang >> 10
    if primary == 0x04:
        return 950 if sub == 1 else 936
    if primary not in ANSI_CODE_PAGES:
        raise ValueError(f"no ANSI code page known for language {lang:#06x}")
    return ANSI_CODE_PAGES[primary]


def language_statement(lang: int) -> str:
    if lang == LANG_EN_US:
        return "LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US"
    return f"LANGUAGE 0x{lang & 0x3FF:X}, 0x{lang >> 10:X}"


# Media files ----------------------------------------------------------------


def bmp_file(dib: bytes) -> bytes:
    """Prefix a resource DIB with the BITMAPFILEHEADER that rc strips off."""
    hsize = struct.unpack_from("<I", dib, 0)[0]
    if hsize == 12:
        bits = struct.unpack_from("<H", dib, 10)[0]
        colors, entry, masks = (1 << bits if bits <= 8 else 0), 3, 0
    else:
        bits, compression = struct.unpack_from("<HI", dib, 14)
        used = struct.unpack_from("<I", dib, 32)[0]
        colors = used or (1 << bits if bits <= 8 else 0)
        entry = 4
        masks = 12 if compression == 3 and hsize == 40 else 0
    bits_off = 14 + hsize + masks + colors * entry
    return struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, bits_off) + dib


def ico_file(group: bytes, images: dict[int, bytes]) -> bytes:
    """Rebuild an .ico from a GROUP_ICON directory and its RT_ICON images."""
    _, kind, count = struct.unpack_from("<HHH", group, 0)
    head = struct.pack("<HHH", 0, kind, count)
    dirs, blobs = b"", b""
    offset = 6 + 16 * count
    for i in range(count):
        fields = group[6 + 14 * i : 6 + 14 * i + 12]
        nid = struct.unpack_from("<H", group, 6 + 14 * i + 12)[0]
        img = images[nid]
        dirs += fields + struct.pack("<I", offset + len(blobs))
        blobs += img
    return head + dirs + blobs


# resource type -> (rc type keyword, asset subdirectory, file extension)
MEDIA_TYPES: dict[Key, tuple[str, str, str]] = {
    RT_MANIFEST: ("24", "manifest", "manifest"),
    RT_GROUP_ICON: ("ICON", "icons", "ico"),
    RT_BITMAP: ("BITMAP", "bitmaps", "bmp"),
    "WAVE": ("WAVE", "sounds", "wav"),
}


def media_path(r: Resource) -> str:
    _, subdir, ext = MEDIA_TYPES[r.type]
    return f"{subdir}/{r.name}.{ext}"


def media_files(rs: ResourceSection) -> dict[str, bytes]:
    """Relative path -> bytes for every resource that has a standalone file format."""
    icons = {r.name: r.data for r in rs.of_type(RT_ICON)}
    out: dict[str, bytes] = {}
    for r in rs.resources:
        if r.type == RT_BITMAP:
            out[media_path(r)] = bmp_file(r.data)
        elif r.type == RT_GROUP_ICON:
            out[media_path(r)] = ico_file(r.data, icons)
        elif r.type in MEDIA_TYPES:
            out[media_path(r)] = r.data
    return out


def write_if_changed(path: Path, data: bytes) -> bool:
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def extract_assets(exe: Path, out_dir: Path) -> list[Path]:
    """Write every media file under out_dir and return their paths."""
    rs = ResourceSection(exe)
    paths = []
    for rel, data in media_files(rs).items():
        path = out_dir / rel
        write_if_changed(path, data)
        paths.append(path)
    return paths


# Resource script --------------------------------------------------------------


def rc_string(s: str, cp: int = 1252) -> str:
    out = []
    for ch in s:
        c = ord(ch)
        if ch == '"':
            out.append('""')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\n":
            out.append("\\n")
        elif 0x20 <= c < 0x7F:
            out.append(ch)
        else:
            try:
                raw = ch.encode(f"cp{cp}")
            except UnicodeEncodeError:
                raise ValueError(f"U+{c:04X} in {s!r} is not in code page {cp}") from None
            # rc reads each \ooo as one character of this code page. A DBCS
            # character is several source bytes, so it has to be written as
            # those bytes. Callers encode the script with `cp`.
            if len(raw) == 1:
                out.append(f"\\{raw[0]:03o}")
            else:
                out.append(ch)
    return '"' + "".join(out) + '"'


def sz_or_ord(b: bytes, p: int) -> tuple[Key, int]:
    if b[p : p + 2] == b"\xff\xff":
        return struct.unpack_from("<H", b, p + 2)[0], p + 4
    e = p
    while b[e : e + 2] != b"\0\0":
        e += 2
    return b[p:e].decode("utf-16-le"), e + 2


def flag_expr(value: int, names: list[tuple[str, int]]) -> list[str]:
    parts = []
    for name, bit in names:
        if value & bit == bit:
            parts.append(name)
            value &= ~bit
    if value:
        parts.append(f"0x{value:X}")
    return parts


WS_FLAGS = [
    ("WS_POPUP", 0x80000000), ("WS_CHILD", 0x40000000), ("WS_MINIMIZE", 0x20000000),
    ("WS_VISIBLE", 0x10000000), ("WS_DISABLED", 0x08000000), ("WS_CLIPSIBLINGS", 0x04000000),
    ("WS_CLIPCHILDREN", 0x02000000), ("WS_MAXIMIZE", 0x01000000), ("WS_CAPTION", 0x00C00000),
    ("WS_BORDER", 0x00800000), ("WS_DLGFRAME", 0x00400000), ("WS_VSCROLL", 0x00200000),
    ("WS_HSCROLL", 0x00100000), ("WS_SYSMENU", 0x00080000), ("WS_THICKFRAME", 0x00040000),
    ("WS_GROUP", 0x00020000), ("WS_TABSTOP", 0x00010000),
]
DS_FLAGS = [
    ("DS_CONTEXTHELP", 0x2000), ("DS_CENTERMOUSE", 0x1000), ("DS_CENTER", 0x0800),
    ("DS_CONTROL", 0x0400), ("DS_SETFOREGROUND", 0x0200), ("DS_NOIDLEMSG", 0x0100),
    ("DS_MODALFRAME", 0x0080), ("DS_SETFONT", 0x0040), ("DS_LOCALEDIT", 0x0020),
    ("DS_NOFAILCREATE", 0x0010), ("DS_FIXEDSYS", 0x0008), ("DS_3DLOOK", 0x0004),
    ("DS_SYSMODAL", 0x0002), ("DS_ABSALIGN", 0x0001),
]
CLASS_ORDINALS = {0x80: "Button", 0x81: "Edit", 0x82: "Static", 0x83: "ListBox",
                  0x84: "ScrollBar", 0x85: "ComboBox"}

WS_CHILD_VISIBLE = 0x50000000
WS_GROUP = 0x00020000
WS_TABSTOP = 0x00010000
# rc keyword -> (class ordinal, type bits, type mask, default WS_ bits beyond WS_CHILD|WS_VISIBLE)
CONTROL_KEYWORDS = [
    ("DEFPUSHBUTTON", 0x80, 0x1, 0xF, WS_TABSTOP),
    ("PUSHBUTTON", 0x80, 0x0, 0xF, WS_TABSTOP),
    ("AUTOCHECKBOX", 0x80, 0x3, 0xF, WS_TABSTOP),
    ("CHECKBOX", 0x80, 0x2, 0xF, WS_TABSTOP),
    ("AUTORADIOBUTTON", 0x80, 0x9, 0xF, 0),
    ("RADIOBUTTON", 0x80, 0x4, 0xF, 0),
    ("GROUPBOX", 0x80, 0x7, 0xF, 0),
    ("LTEXT", 0x82, 0x0, 0x1F, WS_GROUP),
    ("CTEXT", 0x82, 0x1, 0x1F, WS_GROUP),
    ("RTEXT", 0x82, 0x2, 0x1F, WS_GROUP),
    ("ICON", 0x82, 0x3, 0x1F, 0),
]


def style_delta(style: int, default: int) -> str:
    """rc style operand that turns `default` into `style`."""
    extra = style & ~default
    removed = default & ~style
    parts = flag_expr(extra & 0xFFFF0000, WS_FLAGS)
    if extra & 0xFFFF:
        parts.append(f"0x{extra & 0xFFFF:X}")
    parts += [f"NOT {n}" for n in flag_expr(removed, WS_FLAGS)]
    return " | ".join(parts)


def control_line(cls: Key, text: Key, cid: int, style: int, ex: int, rect: tuple, cp: int, wide_id: bool = False) -> str:
    x, y, cx, cy = rect
    # A classic DIALOG id is a WORD, and rc writes -1 as 0xFFFF. DIALOGEX ids are
    # DWORDs, and -1 becomes 0xFFFFFFFF, so the static id has to be written as 65535.
    if cid == 0xFFFF and not wide_id:
        id_s = "-1"
    else:
        id_s = str(cid)
    geom = f"{x}, {y}, {cx}, {cy}"
    text_s = str(text) if isinstance(text, int) else rc_string(text, cp)
    if isinstance(cls, int):
        for kw, ordinal, kind, mask, ws in CONTROL_KEYWORDS:
            if ordinal != cls or style & mask != kind or style & WS_CHILD_VISIBLE == 0:
                continue
            if isinstance(text, int) != (kw == "ICON"):
                continue
            delta = style_delta(style & ~mask, WS_CHILD_VISIBLE | ws)
            tail = f", {delta}" if delta or ex else ""
            if ex:
                tail += f", 0x{ex:X}"
            return f"    {kw:<16}{text_s}, {id_s}, {geom}{tail}"
        cls_s = rc_string(CLASS_ORDINALS[cls], cp)
    else:
        cls_s = rc_string(cls, cp)
    delta = style_delta(style, WS_CHILD_VISIBLE) or "0"
    tail = f", 0x{ex:X}" if ex else ""
    return f"    {'CONTROL':<16}{text_s}, {id_s}, {cls_s}, {delta}, {geom}{tail}"


def dialog_rc(r: Resource, cp: int) -> list[str]:
    b = r.data
    if struct.unpack_from("<HH", b, 0) == (1, 0xFFFF):
        return dialogex_rc(r, cp)
    style, ex, n, x, y, cx, cy = struct.unpack_from("<IIHhhhh", b, 0)
    p = 18
    menu, p = sz_or_ord(b, p)
    cls, p = sz_or_ord(b, p)
    title, p = sz_or_ord(b, p)
    lines = [f"{r.name} DIALOG {x}, {y}, {cx}, {cy}"]
    implied = 0x00C00000 if title else 0
    font = None
    if style & 0x40:
        pt = struct.unpack_from("<H", b, p)[0]
        face, p = sz_or_ord(b, p + 2)
        font = (pt, face)
        implied |= 0x40
    ws = flag_expr(style & 0xFFFF0000, WS_FLAGS)
    ds = flag_expr(style & 0xFFFF, DS_FLAGS)
    lines.append("STYLE " + " | ".join(ds + ws))
    if ex:
        lines.append(f"EXSTYLE 0x{ex:X}")
    if title:
        lines.append(f"CAPTION {rc_string(title, cp)}")
    if menu:
        lines.append(f"MENU {menu}")
    if cls:
        lines.append(f"CLASS {rc_string(cls, cp) if isinstance(cls, str) else cls}")
    if font:
        lines.append(f"FONT {font[0]}, {rc_string(font[1], cp)}")
    lines.append("BEGIN")
    for _ in range(n):
        p = (p + 3) & ~3
        s, e, x, y, cx, cy, cid = struct.unpack_from("<IIhhhhH", b, p)
        p += 18
        c, p = sz_or_ord(b, p)
        t, p = sz_or_ord(b, p)
        extra = struct.unpack_from("<H", b, p)[0]
        p += 2
        if extra:
            raise ValueError(f"dialog {r.name}: control creation data is not handled")
        lines.append(control_line(c, t, cid, s, e, (x, y, cx, cy), cp))
    lines.append("END")
    return lines


def dialogex_rc(r: Resource, cp: int) -> list[str]:
    """DLGTEMPLATEEX. The font record adds weight, italic, and charset, and control ids are 32-bit."""
    b = r.data
    help_id, ex, style, n, x, y, cx, cy = struct.unpack_from("<IIIHhhhh", b, 4)
    p = 26
    menu, p = sz_or_ord(b, p)
    cls, p = sz_or_ord(b, p)
    title, p = sz_or_ord(b, p)
    font = None
    if style & 0x40:
        pt, weight, italic, charset = struct.unpack_from("<HHBB", b, p)
        face, p = sz_or_ord(b, p + 6)
        font = (pt, face, weight, italic, charset)
    head = f"{r.name} DIALOGEX {x}, {y}, {cx}, {cy}"
    if help_id:
        head += f", {help_id}"
    lines = [head]
    ws = flag_expr(style & 0xFFFF0000, WS_FLAGS)
    ds = flag_expr(style & 0xFFFF, DS_FLAGS)
    lines.append("STYLE " + " | ".join(ds + ws))
    if ex:
        lines.append(f"EXSTYLE 0x{ex:X}")
    if title:
        lines.append(f"CAPTION {rc_string(title, cp)}")
    if menu:
        lines.append(f"MENU {menu}")
    if cls:
        lines.append(f"CLASS {rc_string(cls, cp) if isinstance(cls, str) else cls}")
    if font:
        pt, face, weight, italic, charset = font
        lines.append(f"FONT {pt}, {rc_string(face, cp)}, {weight}, {italic}, {charset}")
    lines.append("BEGIN")
    for _ in range(n):
        p = (p + 3) & ~3
        item_help, item_ex, item_style = struct.unpack_from("<III", b, p)
        ix, iy, icx, icy = struct.unpack_from("<hhhh", b, p + 12)
        cid = struct.unpack_from("<I", b, p + 20)[0]
        p += 24
        c, p = sz_or_ord(b, p)
        t, p = sz_or_ord(b, p)
        extra = struct.unpack_from("<H", b, p)[0]
        p += 2
        if extra:
            raise ValueError(f"dialog {r.name}: control creation data is not handled")
        if item_help:
            raise ValueError(f"dialog {r.name}: control help ids are not handled")
        lines.append(control_line(c, t, cid, item_style, item_ex, (ix, iy, icx, icy), cp, wide_id=True))
    lines.append("END")
    return lines


MENU_FLAGS = [("GRAYED", 0x1), ("INACTIVE", 0x2), ("CHECKED", 0x8),
              ("MENUBARBREAK", 0x20), ("MENUBREAK", 0x40), ("HELP", 0x4000)]


def menu_rc(r: Resource, cp: int) -> list[str]:
    b = r.data
    version, hdr = struct.unpack_from("<HH", b, 0)
    if version != 0:
        raise ValueError("MENUEX templates are not handled")
    p = 4 + hdr
    lines = [f"{r.name} MENU", "BEGIN"]

    def level(p: int, indent: str) -> int:
        while True:
            flags = struct.unpack_from("<H", b, p)[0]
            p += 2
            mid = None
            if not flags & 0x10:
                mid = struct.unpack_from("<H", b, p)[0]
                p += 2
            text, p = sz_or_ord(b, p)
            opts = "".join(f", {n}" for n, bit in MENU_FLAGS if flags & bit)
            if flags & 0x10:
                lines.append(f"{indent}POPUP {rc_string(text, cp)}{opts}")
                lines.append(f"{indent}BEGIN")
                p = level(p, indent + "    ")
                lines.append(f"{indent}END")
            elif flags & ~0x80 == 0 and mid == 0 and text == "":
                lines.append(f"{indent}MENUITEM SEPARATOR")
            else:
                lines.append(f"{indent}MENUITEM {rc_string(text, cp)}, {mid}{opts}")
            if flags & 0x80:
                return p

    level(p, "    ")
    lines.append("END")
    return lines


def accel_rc(r: Resource, cp: int) -> list[str]:
    lines = [f"{r.name} ACCELERATORS", "BEGIN"]
    for p in range(0, len(r.data), 8):
        fvirt, key, cmd, _ = struct.unpack_from("<HHHH", r.data, p)
        if fvirt & 0x1:
            if 0x70 <= key <= 0x87:
                key_s = f"VK_F{key - 0x6F}"
            elif chr(key).isalnum() and chr(key).upper() == chr(key):
                key_s = f'"{chr(key)}"'
            else:
                key_s = f"0x{key:X}"
        else:
            key_s = f'"^{chr(key + 0x40)}"' if key < 0x20 else rc_string(chr(key), cp)
        opts = [n for n, bit in (("VIRTKEY", 0x1), ("NOINVERT", 0x2), ("SHIFT", 0x4),
                                 ("CONTROL", 0x8), ("ALT", 0x10)) if fvirt & bit]
        lines.append(f"    {key_s + ',':<8}{cmd}, {', '.join(opts)}")
        if fvirt & 0x80:
            break
    lines.append("END")
    return lines


def string_rc(blocks: list[Resource], cp: int) -> list[str]:
    lines = ["STRINGTABLE", "BEGIN"]
    for r in sorted(blocks, key=lambda r: r.name):
        p = 0
        for i in range(16):
            n = struct.unpack_from("<H", r.data, p)[0]
            p += 2
            if n:
                text = r.data[p : p + 2 * n].decode("utf-16-le")
                lines.append(f"    {(int(r.name) - 1) * 16 + i:<6}{rc_string(text, cp)}")
            p += 2 * n
    lines.append("END")
    return lines


def version_rc(r: Resource, cp: int) -> list[str]:
    b = r.data

    def node(p: int):
        length, vlen, vtype = struct.unpack_from("<HHH", b, p)
        e = p + 6
        while b[e : e + 2] != b"\0\0":
            e += 2
        key = b[p + 6 : e].decode("utf-16-le")
        q = (e + 2 + 3) & ~3
        value = b[q : q + (vlen * 2 if vtype == 1 else vlen)]
        kids = []
        c = (q + len(value) + 3) & ~3
        while c < p + length:
            kid, c = node(c)
            kids.append(kid)
            c = (c + 3) & ~3
        return (key, vtype, value, kids), p + length

    (key, _, fixed, kids), _ = node(0)
    if key != "VS_VERSION_INFO":
        raise ValueError("unexpected version root")
    f = struct.unpack_from("<13I", fixed)
    fv = (f[2] >> 16, f[2] & 0xFFFF, f[3] >> 16, f[3] & 0xFFFF)
    pv = (f[4] >> 16, f[4] & 0xFFFF, f[5] >> 16, f[5] & 0xFFFF)
    lines = [
        f"{r.name} VERSIONINFO",
        f" FILEVERSION {','.join(map(str, fv))}",
        f" PRODUCTVERSION {','.join(map(str, pv))}",
        f" FILEFLAGSMASK 0x{f[6]:X}",
        f" FILEFLAGS 0x{f[7]:X}",
        f" FILEOS 0x{f[8]:X}",
        f" FILETYPE 0x{f[9]:X}",
        f" FILESUBTYPE 0x{f[10]:X}",
        "BEGIN",
    ]

    def emit(n, indent):
        key, vtype, value, kids = n
        if kids or (vtype == 1 and not value):
            lines.append(f"{indent}BLOCK {rc_string(key, cp)}")
            lines.append(f"{indent}BEGIN")
            for k in kids:
                emit(k, indent + "    ")
            lines.append(f"{indent}END")
        elif vtype == 1:
            text = value.decode("utf-16-le").rstrip("\0")
            lines.append(f"{indent}VALUE {rc_string(key, cp)}, {rc_string(text, cp)}")
        else:
            words = struct.unpack_from(f"<{len(value) // 2}H", value)
            lines.append(f"{indent}VALUE {rc_string(key, cp)}, " + ", ".join(f"0x{w:X}" for w in words))

    for k in kids:
        emit(k, "    ")
    lines.append("END")
    return lines


def resource_script(rs: ResourceSection, orig: Path | None = None, assets: Path | None = None) -> tuple[str, int]:
    """Resource script whose rc output lays the data out in the original order.

    The script is Unicode text. Encode it with the returned code page before rc reads it.
    """
    langs = rs.languages()
    if len(langs) != 1:
        raise ValueError(f"resources in more than one language: {sorted(hex(l) for l in langs)}")
    lang = langs.pop()
    cp = code_page(lang)
    src = orig or Path("orig/spider.exe")
    include = assets or Path("build/XPSP1/spider/assets")
    out = [
        f"// Decompiled from {src.as_posix()} by tools/extract_assets.py --rc.",
        "// rc writes resource data in statement order (STRINGTABLE always last), and",
        "// the linker keeps that order in .rsrc, so the order below is part of the match.",
        "// Media files are extracted from the original at configure time and found",
        f"// through /i {include.as_posix()}.",
        "",
        "#include <winresrc.h>",
        f"// Octal escapes are code page {cp} bytes. Pin the page so a UTF-8 host reads them the same way.",
        f"#pragma code_page({cp})",
        "",
        language_statement(lang),
        "",
    ]
    strings: list[Resource] = []
    prev_type: Key | None = None
    for r in sorted(rs.resources, key=lambda r: r.offset):
        if r.type == RT_STRING:
            strings.append(r)
            continue
        if r.type == RT_ICON:
            continue
        if r.type in MEDIA_TYPES:
            block = [f'{r.name} {MEDIA_TYPES[r.type][0]} "{media_path(r)}"']
        elif r.type == RT_DIALOG:
            block = dialog_rc(r, cp)
        elif r.type == RT_MENU:
            block = menu_rc(r, cp)
        elif r.type == RT_ACCELERATOR:
            block = accel_rc(r, cp)
        elif r.type == RT_VERSION:
            block = version_rc(r, cp)
        else:
            raise ValueError(f"no rc form for {r.label()}")
        if prev_type is not None and not (len(block) == 1 and r.type == prev_type):
            out.append("")
        out += block
        prev_type = r.type
    if strings:
        out.append("")
        out += string_rc(strings, cp)
    return "\n".join(out) + "\n", cp


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", type=Path)
    ap.add_argument("out_dir", type=Path)
    ap.add_argument("--rc", type=Path, help="also write a decompiled resource script")
    args = ap.parse_args()
    paths = extract_assets(args.exe, args.out_dir)
    print(f"{len(paths)} asset file(s) in {args.out_dir}")
    if args.rc:
        script, cp = resource_script(ResourceSection(args.exe), orig=args.exe, assets=args.out_dir)
        args.rc.parent.mkdir(parents=True, exist_ok=True)
        args.rc.write_bytes(script.encode(f"cp{cp}"))
        print(f"wrote {args.rc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
