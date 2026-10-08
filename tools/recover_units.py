#!/usr/bin/env python3
"""Recover function bounds, write symbols/splits/units, and stub game sources."""

from __future__ import annotations

import argparse
import base64
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.coffsym import defined_function_symbols
from tools.names import load_names
TEXT_START = 0x01001000
IAT_END = 0x01001280
GAME_START = 0x01002660
CRT_START = 0x0100870E
TEXT_END = 0x0100FAF8
DATA_START = 0x01010000
DATA_END = 0x010125B4

# Compiler-generated scalar deleting destructor; keep as game C++.
SKIP_CRT_NAMES = {
    "??_Gexception@@UAEPAXI@Z",
    "??_G__non_rtti_object@@UAEPAXI@Z",
    "??_Gbad_cast@@UAEPAXI@Z",
    "??_Gbad_typeid@@UAEPAXI@Z",
}


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def load_pe_text(exe_path: Path) -> tuple[bytes, bytes, int]:
    exe = exe_path.read_bytes()
    e_lfanew = u32(exe, 0x3C)
    coff = e_lfanew + 4
    nsect = u16(exe, coff + 2)
    opt_size = u16(exe, coff + 16)
    opt = coff + 20
    image_base = u32(exe, opt + 28)
    sect0 = opt + opt_size
    sections: dict[str, tuple[int, int, int, int]] = {}
    for i in range(nsect):
        o = sect0 + i * 40
        name = exe[o : o + 8].split(b"\0", 1)[0].decode()
        sections[name] = (
            u32(exe, o + 8),
            u32(exe, o + 12),
            u32(exe, o + 20),
            u32(exe, o + 16),
        )
    vsize, va, ptr, raw = sections[".text"]
    text = exe[ptr : ptr + raw]
    data_raw = b""
    if ".data" in sections:
        dv, dva, dptr, draw = sections[".data"]
        data_raw = exe[dptr : dptr + draw]
    return text, data_raw, image_base + va


def parse_existing_symbols(path: Path) -> tuple[list[tuple[int, int, str]], list[tuple[int, int, str]]]:
    imps: list[tuple[int, int, str]] = []
    funcs: list[tuple[int, int, str]] = []
    if not path.is_file():
        return imps, funcs
    for line in path.read_text().splitlines():
        if " = " not in line:
            continue
        name, rest = line.split(" = ", 1)
        addr = int(rest.split(":")[1].split(";")[0], 16)
        size = 0
        if "size:" in rest:
            size = int(rest.split("size:")[1].split()[0], 16)
        if "type:function" in rest:
            funcs.append((addr, size, name))
        else:
            imps.append((addr, size, name))
    return imps, funcs


def parse_sigs(sigs_dir: Path) -> list[tuple[str, int, bytes, bytes]]:
    out: list[tuple[str, int, bytes, bytes]] = []
    if not sigs_dir.is_dir():
        return out
    for p in sigs_dir.glob("*.yml"):
        txt = p.read_text()
        for block in re.split(r"(?m)^- symbol:", txt)[1:]:
            sm = re.search(r"signature: (\S+)", block)
            nm = re.search(r"name: (\S+)", block)
            sz = re.search(r"size: (\d+)", block)
            if not sm or not nm or not sz:
                continue
            size = int(sz.group(1))
            raw = base64.b64decode(sm.group(1))
            if size <= 0 or len(raw) < size * 2:
                continue
            out.append((nm.group(1), size, bytes(raw[0::2][:size]), bytes(raw[1::2][:size])))
    return out


def find_pattern(code: bytes, pat: bytes, mask: bytes) -> list[int]:
    n = len(pat)
    exact = [i for i, m in enumerate(mask) if m == 0xFF]
    if not exact:
        return []
    i0 = exact[0]
    b0 = pat[i0]
    addrs: list[int] = []
    start = 0
    code_len = len(code)
    while True:
        i = code.find(b0, start + i0)
        if i < 0:
            break
        pos = i - i0
        start = i + 1
        if pos < 0 or pos + n > code_len:
            continue
        chunk = code[pos : pos + n]
        ok = True
        for p, m, c in zip(pat, mask, chunk):
            if m == 0xFF and p != c:
                ok = False
                break
        if ok:
            addrs.append(pos)
    return addrs


def call_targets(text: bytes, text_va: int, lo: int, hi: int) -> set[int]:
    tgts: set[int] = set()
    off0 = lo - text_va
    region = text[off0 : hi - text_va]
    i = 0
    while i < len(region) - 4:
        if region[i] == 0xE8:
            rel = struct.unpack_from("<i", region, i + 1)[0]
            t = lo + i + 5 + rel
            if TEXT_START <= t < TEXT_END:
                tgts.add(t)
            i += 5
            continue
        i += 1
    return tgts


def is_prologue(data: bytes, i: int) -> bool:
    if i >= len(data):
        return False
    rest = data[i : i + 8]
    if len(rest) < 2:
        return False
    if rest.startswith(b"\x55\x8b\xec"):
        return True
    if rest.startswith(b"\x56\x8b\xf1"):
        return True
    if rest.startswith(b"\x53\x56") or rest.startswith(b"\x56\x57") or rest.startswith(b"\x53\x57"):
        return True
    if rest[0] == 0x55 and len(rest) >= 4 and rest[1] == 0x8D:  # lea ebp, [esp+imm] SEH
        return True
    if rest.startswith(b"\x8b\x44\x24") or rest.startswith(b"\x8b\x54\x24") or rest.startswith(b"\x8b\x4c\x24"):
        return True
    if rest.startswith(b"\x8b\x01") or rest.startswith(b"\xff\x31") or rest.startswith(b"\x8b\x09"):
        return True
    if rest.startswith(b"\x83\xec") or rest.startswith(b"\x81\xec"):
        return True
    if rest.startswith(b"\xff\x74\x24"):  # push [esp+imm]
        return True
    if rest.startswith(b"\x6a") and rest[2:4] in (b"\x56", b"\x53", b"\x51"):
        return True
    return False


def after_ret_starts(text: bytes, text_va: int, lo: int, hi: int) -> set[int]:
    starts: set[int] = set()
    off0 = lo - text_va
    n = hi - lo
    i = 0
    data = text[off0 : off0 + n]

    def add_at(local: int) -> None:
        while local < n and data[local] in (0xCC, 0x90):
            local += 1
        if local < n and is_prologue(data, local):
            starts.add(lo + local)

    while i < n:
        b = data[i]
        if b == 0xC3:
            add_at(i + 1)
            i += 1
        elif b == 0xC2 and i + 2 < n:
            add_at(i + 3)
            i += 3
        else:
            i += 1
    return starts


def prologue_starts(text: bytes, text_va: int, lo: int, hi: int) -> set[int]:
    starts: set[int] = set()
    off0 = lo - text_va
    data = text[off0 : hi - text_va]
    for i in range(0, len(data) - 3):
        if data[i : i + 3] == b"\x55\x8b\xec":
            starts.add(lo + i)
        elif data[i : i + 2] == b"\x56\x8b" and i + 2 < len(data) and data[i + 2] == 0xF1:
            starts.add(lo + i)
    return starts


def pointer_starts(text: bytes, data: bytes, text_va: int, lo: int, hi: int) -> set[int]:
    starts: set[int] = set()
    for blob, base in ((text, text_va), (data, DATA_START)):
        for i in range(0, max(0, len(blob) - 3), 4):
            val = struct.unpack_from("<I", blob, i)[0]
            if lo <= val < hi:
                starts.add(val)
    return starts


def looks_like_code(text: bytes, text_va: int, addr: int) -> bool:
    off = addr - text_va
    if off < 0 or off >= len(text):
        return False
    b = text[off : off + 8]
    if not b:
        return False
    if b[0] in (0x00, 0xCC, 0x90) and b[:4] == b"\x00\x00\x00\x00":
        return False
    # ASCII-ish
    if all(32 <= x < 127 or x in (0, 10, 13) for x in b[:8]) and b[:4].isascii():
        if b[:4].isalnum() or b[:1] in b"RrTtMmCc":
            # still could be code; require at least one opcode-looking byte
            pass
    return True


def sizes_from_starts(starts: list[int], end: int, text: bytes, text_va: int) -> list[tuple[int, int]]:
    starts = sorted(set(starts))
    out: list[tuple[int, int]] = []
    for i, a in enumerate(starts):
        nxt = starts[i + 1] if i + 1 < len(starts) else end
        off_a = a - text_va
        off_end = nxt - text_va
        while off_end > off_a and text[off_end - 1] in (0xCC, 0x90):
            off_end -= 1
        size = off_end - off_a
        if size > 0:
            out.append((a, size))
    return out


def safe_unit_name(sym: str, category: str) -> str:
    s = re.sub(r"[^A-Za-z0-9_]+", "_", sym).strip("_")
    if not s:
        s = "anon"
    if category == "crt" and not s.startswith("crt_"):
        return f"crt_{s}.c"
    return f"{s}.c"


def stub_for(sym: str) -> str:
    ident = re.sub(r"[^A-Za-z0-9_]", "_", sym)
    if ident[0].isdigit():
        ident = "_" + ident
    return (
        f"/* Stub for {sym}. Replace with matching C; do not mark complete until objdiff is clean. */\n"
        f"void {ident}(void)\n"
        "{\n"
        "}\n"
    )


def matching_canaries() -> dict[str, str]:
    """Hand-written matching bodies for the smallest thiscall leaves."""
    return {
        "fn_01007836": """\
/* matching: thiscall int slot_is_empty(int i) — this+0x28 is int[] */
struct Fn01007836 {
    int pad[10];
    int slots[1];
    int fn_01007836(int i);
};

int Fn01007836::fn_01007836(int i)
{
    return slots[i] == 0;
}
""",
        "fn_01006F9E": """\
/* matching: thiscall set_triple(int i, int value) at this->ptr[i*3+2] */
struct Fn01006F9E {
    int pad[3];
    int *ptr;
    void fn_01006F9E(int i, int value);
};

void Fn01006F9E::fn_01006F9E(int i, int value)
{
    ptr[i * 3 + 2] = value;
}
""",
        "fn_01006FB3": """\
struct Fn01006FB3 {
    int pad[3];
    int *ptr;
    int fn_01006FB3(int i);
};

int Fn01006FB3::fn_01006FB3(int i)
{
    return ptr[i * 3 + 2];
}
""",
        "fn_01006FC4": """\
struct Fn01006FC4 {
    int pad[3];
    int *ptr;
    int fn_01006FC4(int i);
};

int Fn01006FC4::fn_01006FC4(int i)
{
    return ptr[i * 3];
}
""",
        "fn_01006FD4": """\
struct Fn01006FD4 {
    int pad[3];
    int *ptr;
    int fn_01006FD4(int i);
};

int Fn01006FD4::fn_01006FD4(int i)
{
    return ptr[i * 3 + 1];
}
""",
        "fn_01002AD0": """\
struct Fn01002AD0 {
    int pad[5];
    int a;
    int b;
    int fn_01002AD0(int x);
};

int Fn01002AD0::fn_01002AD0(int x)
{
    int q;
    int r;
    if (x < b) {
        return -2;
    }
    q = (x - b) / (a + 0x47);
    r = (x - b) % (a + 0x47);
    if (r > 0x47) {
        return -2;
    }
    if (q < 10) {
        return q;
    }
    return -2;
}
""",
        "fn_01003478": """\
struct Fn01003478 {
    char pad[0xF10];
    int vals[4];
    int fn_01003478();
};

int Fn01003478::fn_01003478()
{
    int sum = 0;
    int i;
    for (i = 0; i < 4; i++) {
        sum += vals[i];
    }
    return sum;
}
""",
        "fn_01007846": """\
struct Fn01007846 {
    int pad[10];
    int slots[10];
    int fn_01007846();
};

int Fn01007846::fn_01007846()
{
    int i;
    for (i = 0; i < 10; i++) {
        if (slots[i] == 0) {
            return 1;
        }
    }
    return 0;
}
""",
        "fn_01002A43": """\
struct Fn01002A43 {
    int pad[5];
    int a;
    int b;
    int fn_01002A43(int i);
};

int Fn01002A43::fn_01002A43(int i)
{
    return (a + 0x47) * 9 - i * 12 + b;
}
""",
        "fn_010070B5": """\
struct Fn010070B5 {
    int pad[2];
    int limit;
    int *ptr;
    int idx;
    int *fn_010070B5();
};

int *Fn010070B5::fn_010070B5()
{
    int i = idx;
    if (i >= limit) {
        return 0;
    }
    idx = i + 1;
    return &ptr[i * 3];
}
""",
        "fn_01007B78": """\
struct Vec01007B78 {
    float x;
    float y;
    float z;
};

struct Fn01007B78 {
    float fn_01007B78(Vec01007B78 *v);
};

float Fn01007B78::fn_01007B78(Vec01007B78 *v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}
""",
        "fn_01007B00": """\
struct Vec01007B00 {
    float x;
    float y;
    float z;
};

void __stdcall fn_01007B00(Vec01007B00 *out, Vec01007B00 *a, Vec01007B00 *b)
{
    float x;
    float y;
    float z;

    z = a->z + b->z;
    y = a->y + b->y;
    x = a->x + b->x;
    out->x = x;
    out->y = y;
    out->z = z;
}
""",
        "fn_01007860": """\
/* matching: walk i nodes from this->head via +8.
 * Counter must be initialized before loading head so /O1 keeps n in edx. */
struct Node01007860 {
    Node01007860 *next;
    int pad;
    Node01007860 *link;
};

struct Fn01007860 {
    Node01007860 *head;
    Node01007860 *fn_01007860(int i);
};

Node01007860 *Fn01007860::fn_01007860(int i)
{
    int n = 0;
    Node01007860 *node = head;

    for (; n < i; n++) {
        if (node == 0) {
            break;
        }
        node = node->link;
    }
    return node;
}
""",
        "fn_01002A5E": """\
#include "spider.h"
struct Fn01002A5E {
    HWND hwnd;
    int fn_01002A5E();
};

int Fn01002A5E::fn_01002A5E()
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    return rc.bottom + (-0x6a);
}
""",
    }


# Units whose objdiff function match is 100%.
MATCHED = {
    "fn_01007836",
    "fn_01006F9E",
    "fn_01006FB3",
    "fn_01006FC4",
    "fn_01006FD4",
    "fn_01003478",
    "fn_01007846",
    "fn_01002A43",
    "fn_010070B5",
    "fn_01007B78",
    "fn_01002AD0",
    "fn_01002A5E",
    "fn_01007860",
    "fn_01003015",
    "fn_01003138",
    "fn_0100314F",
    "fn_01003166",
    "fn_01007BF4",
    "fn_01007B00",
    "fn_01007B27",
    "fn_01007B4D",
    "fn_010081E2",
    "fn_01006F87",
    "fn_01002660",
    "fn_0100267C",
    "fn_01002698",
    "fn_01003634",
    "fn_0100364B",
    "fn_01007ADF",
    "fn_01007AC3",
    "fn_010079E8",
    "fn_01002C17",
    "fn_01007A62",
    "fn_0100478C",
    "fn_01004766",
    "fn_010085AB",
    "fn_01006134",
    "fn_0100348C",
    "fn_0100311A",
    "fn_01007915",
    "fn_010079B6",
    "fn_010078E1",
    "fn_0100376A",
    "fn_01003662",
    "fn_01002CEB",
    "fn_01002D22",
    "fn_01002D59",
    "fn_0100302F",
    "fn_01002C35",
    "fn_01004732",
    "fn_01007BC2",
    "fn_0100787B",
    "fn_01002A78",
    "fn_01007C18",
    "fn_01002C8C",
    "fn_01007A82",
    "fn_01004C22",
    "fn_01008438",
    "fn_010081A9",
    "fn_01002FAD",
    "fn_01007B9D",
    "fn_01003A01",
    "fn_01006F57",
    "fn_01004C6A",
    "fn_01006336",
    "fn_0100467E",
    "fn_010034ED",
    "fn_010085BB",
    "fn_010075D4",
    "fn_01005E7F",
    "fn_0100863E",
    "fn_01004B2C",
    "fn_01002815",
    "fn_01008463",
    "fn_01008085",
    "fn_01007C97",
}

# Mid-function `push esi; mov esi, ecx` hits that split a real prologue off.
IGNORE_STARTS = {
    0x01005E81,
    0x01002C3A,
    0x01006F5B,
    0x01006FEB,
    0x0100793D,
    0x0100846A,
}

CRT_RENAME = {
    0x0100ED49: "_HtmlHelpW@16",
}

MANGLED = {
    "fn_01007836": "?slot_empty@CardColumn@@QAEHH@Z",
    "fn_01007846": "?any_empty@CardColumn@@QAEHXZ",
    "fn_01007860": "?node_at@CardList@@QAEPAUCardNode@@H@Z",
    "fn_0100787B": "?tail@CardList@@QAEPAUCardNode@@PAH@Z",
    "fn_010078A2": "?unlink@CardList@@QAEHH@Z",
    "fn_01007A07": "?append@CardList@@QAEHPAH0@Z",
    "fn_010079E8": "?drain@CardList@@QAEXXZ",
    "fn_01007AC3": "?release@CardList@@QAEPAXE@Z",
    "fn_01007915": "?card_value@PileTable@@QAEHHH@Z",
    "fn_01007937": "?splice@PileTable@@QAEXHHHH@Z",
    "fn_010079B6": "?trim_pile@PileTable@@QAEXHH@Z",
    "fn_01006F9E": "?set_suit@TripleTable@@QAEXHH@Z",
    "fn_01006FB3": "?suit@TripleTable@@QAEHH@Z",
    "fn_01006FC4": "?rank@TripleTable@@QAEHH@Z",
    "fn_01006FD4": "?face@TripleTable@@QAEHH@Z",
    "fn_010070B5": "?next@TripleTable@@QAEPAHXZ",
    "fn_01006FE5": "?shuffle@TripleTable@@QAEXH@Z",
    "fn_01002D90": "?fit_piles@ColumnOp@@QAEHXZ",
    "fn_01006F87": "?clear_buf@AttrTable@@QAEXXZ",
    "fn_01006F57": "??0AttrTable@@QAE@H@Z",
    "fn_0100267C": "?release@AttrTable@@QAEPAXE@Z",
    "fn_01002A43": "?card_x@LayoutBox@@QAEHH@Z",
    "fn_01002AD0": "?pile_at@LayoutBox@@QAEHH@Z",
    "fn_01002A78": "?fill_rect@LayoutBox@@QAEXPAUtagRECT@@@Z",
    "fn_01002A5E": "?deal_top@LayoutWin@@QAEHXZ",
    "fn_01003662": "?center_rect@LayoutWin@@QAEXPAUtagRECT@@@Z",
    "fn_01002A07": "?score@Layout@@QAEHHH@Z",
    "fn_01003478": "?total@ClearedSets@@QAEHXZ",
    "fn_0100348C": "_load_string@4",
    "fn_010034AE": "_alert_box@16",
    "fn_01006336": "?deal_prompt@GameWin@@QAEXXZ",
    "fn_01003434": "?add_score@GameWin@@QAEXH@Z",
    "fn_01002CEB": "?suit_of@DealView@@QAEHHH@Z",
    "fn_01002C35": "?card_code@DealView@@QAEHHH@Z",
    "fn_0100302F": "?rank_kind@DealView@@QAEHHHHH@Z",
    "fn_01002D22": "?rank_of@DealView@@QAEHHH@Z",
    "fn_01002D59": "?face_of@DealView@@QAEHHH@Z",
    "fn_0100376A": "?fill@DealBox@@QAEXPAUtagRECT@@@Z",
    "fn_010078E1": "??0TenListBoard@@QAE@XZ",
    "fn_01007A62": "?clear_lists@TenListBoard@@QAEXXZ",
    "fn_01007ADF": "?destroy_lists@TenListBoard@@QAEXXZ",
    "fn_01002698": "?release@TenListBoard@@QAEPAXE@Z",
    "fn_01003634": "?drop_attrs@GameBoard@@QAEXXZ",
    "fn_0100364B": "?drop_board@GameBoard@@QAEXXZ",
    "fn_01004766": "?reset_attrs@GameBoard@@QAEXXZ",
    "fn_0100478C": "?reset_board@GameBoard@@QAEXXZ",
    "fn_01004732": "??1GameBoard@@QAE@XZ",
    "fn_01007B00": "?vec_add@@YGXPAUVec3@@00@Z",
    "fn_01007B27": "?vec_scale@@YGXPAUVec3@@M0@Z",
    "fn_01007B4D": "?vec_div@@YGXPAUVec3@@0M@Z",
    "fn_01007B78": "?length_sq@VecMath@@QAEMPAUVec3@@@Z",
    "fn_010081A9": "?normalize@VecMath@@QAEPAUVec3@@PAU2@0@Z",
    "fn_01007BC2": "?blit@BlitBoard@@QAEXXZ",
    "fn_01007BF4": "?blit_to@SurfaceBlit@@QAEXPAUHDC__@@@Z",
    "fn_01007B9D": "?apply@StrideCall@@QAEXPADHHP6IXPAX@Z@Z",
    "fn_010081E2": "?stop@AnimState@@QAEXXZ",
    "fn_01002660": "?release@AnimState@@QAEPAXE@Z",
    "fn_010085AB": "?shutdown@AnimState@@QAEXXZ",
    "fn_01007C18": "?clear_draw@AnimState@@QAEXXZ",
    "fn_01008463": "?reset_draw@AnimState@@QAEXXZ",
    "fn_01003015": "?next@UndoRing@@QAEPAUUndoRec@@XZ",
    "fn_010030E9": "?pop_row@UndoBuf@@QAEXXZ",
    "fn_0100311A": "?disable_undo_menu@GameWin@@QAEXXZ",
    "fn_01003138": "?show_dialog_107@GameWin@@QAEXXZ",
    "fn_0100314F": "?show_dialog_118@GameWin@@QAEXXZ",
    "fn_01003166": "?show_dialog_117@GameWin@@QAEXXZ",
    "fn_01002C17": "?place_on_top@ColumnOp@@QAEXHHH@Z",
    "fn_01002B7C": "?place@ColumnOp@@QAEXHHHH@Z",
    "fn_01002AF6": "?hit_card@ColumnOp@@QAEHHH@Z",
    "fn_01006134": "?seed_now@GameWin@@QAEXXZ",
    "fn_010057F8": "?new_game@GameWin@@QAEXH@Z",
    "fn_0100302F": "?rank_kind@DealView@@QAEHHHHH@Z",
    "fn_01002C8C": "?reset_suit_slots@GameWin@@QAEXXZ",
    "fn_01002FAD": "?push@RecBank@@QAEHPAURec24@@@Z",
    "fn_01003078": "?sort_desc@RecBank@@QAEXXZ",
    "fn_01004C6A": "?check_saved@GameWin@@QAEXXZ",
    "fn_01007A82": "?add_card@TenListBoard@@QAEXHHH@Z",
    "fn_0100379D": "?run_ok@DealView@@QAEHHH@Z",
    "fn_0100382E": "?can_drop@DealView@@QAEHHHH@Z",
    "fn_01003A01": "?push_undo@GameWin@@QAEHPAH@Z",
    "fn_0100467E": "?tally@GameWin@@QAEXH@Z",
    "fn_010034ED": "?on_mouse_move@GameBoard@@QAEHPAUHWND__@@IIJ@Z",
    "fn_010085BB": "?tick_fx@AnimState@@QAEXPAUFxBank@@@Z",
    "fn_010075D4": "_fn_010075D4@16",
    "fn_01005E7F": "?won_game@GameWin@@QAEXXZ",
    "fn_0100317D": "?refresh_drag@GameBoard@@QAEXHHHHPAUtagRECT@@0@Z",
    "fn_01008085": "?step_fx@AnimState@@QAEXPAUFxItem@@@Z",
    "fn_010081EE": "?burst_fx@AnimState@@QAEXPAUFxBank@@@Z",
    "fn_0100598E": "?paint_hdc@GameWin@@QAEXPAUHDC__@@@Z",
    "fn_0100863E": "?paint@AnimState@@QAEXXZ",
    "fn_01007C97": "?prep_blit@AnimState@@QAEXXZ",
    "fn_01007E48": "?run_fx@AnimState@@QAEXPAUFxBank@@@Z",
    "fn_01002E68": "_fn_01002E68@16",
    "fn_010036A6": "?draw_stock@LayoutBox@@QAEXPAXHH@Z",
    "fn_01003F0C": "@fn_01003F0C@4",
    "fn_010042B6": "?open_saved@GameWin@@QAEPAXKK@Z",
    "fn_01004C22": "?pop_undo@GameWin@@QAEXXZ",
    "fn_01004B2C": "?blink_move@GameWin@@QAEXXZ",
    "fn_010038B1": "?collect_moves@GameWin@@QAEXXZ",
    "fn_01008438": "?init_draw@AnimState@@QAEPAU1@PAUHWND__@@@Z",
    "fn_01002815": "_fn_01002815@24",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "orig/XPSP1/spider.exe")
    parser.add_argument("--symbols", type=Path, default=ROOT / "config/XPSP1/symbols.txt")
    parser.add_argument("--splits", type=Path, default=ROOT / "config/XPSP1/splits.txt")
    parser.add_argument("--units", type=Path, default=ROOT / "config/XPSP1/units.json")
    parser.add_argument("--sigs", type=Path, default=Path("/tmp/xpddk_libcmt_sigs"))
    parser.add_argument("--src-game", type=Path, default=ROOT / "src/game")
    args = parser.parse_args()

    text, data, text_va = load_pe_text(args.exe)
    code = text[: TEXT_END - text_va]
    imps, old_funcs = parse_existing_symbols(args.symbols)
    old_by_addr = {a: (s, n) for a, s, n in old_funcs}

    # --- game starts ---
    game_starts = {GAME_START}
    game_starts |= {t for t in call_targets(text, text_va, GAME_START, CRT_START) if GAME_START <= t < CRT_START}
    game_starts |= {t for t in after_ret_starts(text, text_va, GAME_START, CRT_START) if GAME_START <= t < CRT_START}
    game_starts |= {t for t in prologue_starts(text, text_va, GAME_START, CRT_START) if GAME_START <= t < CRT_START}
    for t in pointer_starts(text, data, text_va, GAME_START, CRT_START):
        off = t - text_va
        prev = text[off - 1] if off else 0xCC
        if prev in (0xCC, 0x90, 0xC3) or (off >= 3 and text[off - 3] == 0xC2) or is_prologue(text, off):
            game_starts.add(t)
    game_starts = {a for a in game_starts if looks_like_code(text, text_va, a)}
    game_starts -= IGNORE_STARTS
    game_funcs = sizes_from_starts(sorted(game_starts), CRT_START, text, text_va)

    # --- CRT via signatures + existing names + recovery ---
    sigs = parse_sigs(args.sigs)
    crt_at: dict[int, list[tuple[str, int]]] = defaultdict(list)
    strncpy_hit = None
    ftol_hit = None
    for name, size, pat, mask in sigs:
        if size < 8 or name in SKIP_CRT_NAMES:
            continue
        for off in find_pattern(code, pat, mask):
            addr = text_va + off
            if addr < CRT_START:
                continue
            crt_at[addr].append((name, size))
            if name == "_strncpy":
                strncpy_hit = (addr, size)
            if name == "__ftol":
                ftol_hit = (addr, size)

    crt_starts = {CRT_START}
    crt_starts |= set(crt_at)
    crt_starts |= {a for a, _, n in old_funcs if a >= CRT_START and not n.startswith("fn_")}
    # Fill remaining CRT holes with prologue/call starts only (not every ret).
    crt_starts |= {t for t in call_targets(text, text_va, CRT_START, TEXT_END) if CRT_START <= t < TEXT_END}
    crt_starts |= {t for t in prologue_starts(text, text_va, CRT_START, TEXT_END) if CRT_START <= t < TEXT_END}
    crt_funcs = sizes_from_starts(sorted(crt_starts), TEXT_END, text, text_va)

    def crt_name(addr: int, size: int) -> tuple[str, bool]:
        if addr in CRT_RENAME:
            return CRT_RENAME[addr], True
        if addr in old_by_addr:
            _, n = old_by_addr[addr]
            if not n.startswith("fn_"):
                return n, True
        hits = crt_at.get(addr, [])
        named = [h for h in hits if not h[0].startswith("fn_") and "$$$" not in h[0]]
        if named:
            named.sort(key=lambda x: -x[1])
            return named[0][0], True
        if hits:
            return hits[0][0], True
        if addr in old_by_addr:
            return old_by_addr[addr][1], False
        return f"fn_{addr:08X}", False

    # --- emit symbols ---
    lines: list[str] = []
    for addr, size, name in sorted(imps):
        lines.append(f"{name} = .text:{addr:#010x}; // type:object size:{size:#x}")
    lines.append(f"text_rdata = .text:{IAT_END:#010x}; // type:object size:{GAME_START - IAT_END:#x}")
    labels = load_names(ROOT / "config/XPSP1/names.txt")
    obj_dir = ROOT / "build" / "XPSP1" / "src"
    game_symbols: dict[str, str] = {}
    used_names: set[str] = set()
    for addr, size in game_funcs:
        stub_name = f"fn_{addr:08X}"
        compiled = defined_function_symbols(obj_dir / f"{stub_name}.obj")
        prev = old_by_addr.get(addr)
        mangled = MANGLED.get(stub_name)
        stub_prev = False
        if prev:
            stub_prev = prev[1].startswith("fn_") or prev[1].startswith("_fn_")
        if mangled and mangled in compiled and mangled not in used_names:
            name = mangled
        elif prev and prev[1] in compiled and prev[1] not in used_names and not stub_prev:
            name = prev[1]
        elif mangled and mangled not in used_names and (not compiled or not any(n.startswith("?") for n in compiled)):
            name = mangled
        else:
            unused = [n for n in compiled if n not in used_names]
            pick = unused[0] if unused else (compiled[0] if compiled else None)
            if pick and (stub_name in labels or pick.startswith("?")):
                name = pick
            elif prev:
                name = prev[1]
            else:
                name = mangled or stub_name
        used_names.add(name)
        game_symbols[stub_name] = name
        lines.append(f"{name} = .text:{addr:#010x}; // type:function size:{size:#x}")
    seen_crt: dict[str, int] = {}
    crt_named = 0
    crt_entries: list[tuple[int, int, str, bool]] = []
    for addr, size in crt_funcs:
        name, identified = crt_name(addr, size)
        if name in seen_crt:
            name = f"{name}_{addr:08X}"
            identified = False
        seen_crt[name] = addr
        crt_entries.append((addr, size, name, identified))
        scope = " scope:global" if identified and not name.startswith("fn_") else ""
        lines.append(f"{name} = .text:{addr:#010x}; // type:function size:{size:#x}{scope}")
        if identified:
            crt_named += 1

    args.symbols.parent.mkdir(parents=True, exist_ok=True)
    args.symbols.write_text("\n".join(lines) + "\n", encoding="utf-8")

    # --- splits: abutting ranges so .text is fully covered ---
    split_units: list[dict] = []
    split_lines = [
        "Sections:",
        "	.text       type:code align:4096",
        "	.data       type:data align:4096",
        "	.rsrc       type:rodata align:4096",
        "",
        "iat:",
        f"	.text       start:{TEXT_START:#010x} end:{IAT_END:#010x} align:1",
        "",
        "text_rdata:",
        f"	.text       start:{IAT_END:#010x} end:{GAME_START:#010x} align:1",
        "",
    ]
    split_units.append({"name": "iat", "category": None, "complete": True, "kind": "thunks"})
    split_units.append({"name": "text_rdata", "category": None, "complete": True, "kind": "rdata"})

    args.src_game.mkdir(parents=True, exist_ok=True)
    canaries = matching_canaries()
    prev_source = {}
    if args.units.is_file():
        for u in json.loads(args.units.read_text()):
            if u.get("kind") == "game" and u.get("source"):
                prev_source[u["name"]] = args.src_game.parent / u["source"]
    game_unit_names: list[str] = []
    keep_src = set()
    for i, (addr, size) in enumerate(game_funcs):
        nxt = game_funcs[i + 1][0] if i + 1 < len(game_funcs) else CRT_START
        stub_name = f"fn_{addr:08X}"
        symbol = game_symbols.get(stub_name) or MANGLED.get(stub_name, stub_name)
        unit = safe_unit_name(stub_name, "game")
        split_lines.append(f"{unit}:")
        split_lines.append(f"	.text       start:{addr:#010x} end:{nxt:#010x} align:1")
        split_lines.append("")
        body = canaries.get(stub_name)
        cpp_path = args.src_game / f"{stub_name}.cpp"
        c_path = args.src_game / f"{stub_name}.c"
        # Existing sources win. Canaries only seed a unit that has no file yet,
        # so a readable name in src/game is not overwritten on the next recover.
        if unit in prev_source and prev_source[unit].is_file():
            src_path = prev_source[unit]
        elif cpp_path.is_file():
            src_path = cpp_path
        elif c_path.is_file():
            src_path = c_path
        elif body is not None:
            src_path = cpp_path
            src_path.write_text(body, encoding="utf-8")
        else:
            src_path = c_path
            src_path.write_text(stub_for(stub_name), encoding="utf-8")
        src_name = src_path.relative_to(args.src_game).as_posix()
        keep_src.add(src_path)
        complete = stub_name in MATCHED
        split_units.append(
            {
                "name": unit,
                "category": "game",
                "complete": complete,
                "kind": "game",
                "source": f"game/{src_name}",
                "symbol": symbol,
                "addr": addr,
                "size": size,
            }
        )
        game_unit_names.append(unit)

    for stale in args.src_game.glob("fn_*.c"):
        if stale not in keep_src:
            stale.unlink()
    for stale in args.src_game.glob("fn_*.cpp"):
        if stale not in keep_src:
            stale.unlink()

    for i, (addr, size, name, identified) in enumerate(crt_entries):
        nxt = crt_entries[i + 1][0] if i + 1 < len(crt_entries) else TEXT_END
        unit = safe_unit_name(name, "crt")
        split_lines.append(f"{unit}:")
        split_lines.append(f"	.text       start:{addr:#010x} end:{nxt:#010x} align:1")
        split_lines.append("")
        split_units.append(
            {
                "name": unit,
                "category": "crt",
                "complete": identified,
                "kind": "crt",
                "symbol": name,
                "addr": addr,
                "size": size,
            }
        )

    args.splits.write_text("\n".join(split_lines) + "\n", encoding="utf-8")
    args.units.write_text(json.dumps(split_units, indent=2) + "\n", encoding="utf-8")

    game_bytes = sum(s for _, s in game_funcs)
    crt_bytes = sum(s for _, s, _, _ in crt_entries)
    print(f"game functions {len(game_funcs)} bytes {game_bytes:#x}")
    print(f"crt functions {len(crt_entries)} identified {crt_named} bytes {crt_bytes:#x}")
    print(f"wrote {args.symbols}")
    print(f"wrote {args.splits}")
    print(f"wrote {args.units}")
    if strncpy_hit:
        print(f"libcmt _strncpy matches exe at {strncpy_hit[0]:#x} size {strncpy_hit[1]:#x}")
    else:
        print("warning: _strncpy not found in exe", file=sys.stderr)
    if ftol_hit:
        print(f"libcmt __ftol matches exe at {ftol_hit[0]:#x} size {ftol_hit[1]:#x}")
    else:
        print("warning: __ftol not found in exe", file=sys.stderr)
    covered = (IAT_END - TEXT_START) + (GAME_START - IAT_END) + game_bytes + crt_bytes
    # sizes exclude padding; splits still cover TEXT_END
    print(f"function-sized coverage {covered:#x} of {TEXT_END - TEXT_START:#x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
