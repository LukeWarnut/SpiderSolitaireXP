"""Version-first matching modules (spider.exe, cards.dll)."""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

ROOT = Path(__file__).resolve().parents[1]

TOOLCHAIN_INCLUDES = [
    "/I",
    "orig/toolchain/inc",
    "/I",
    "orig/toolchain/inc/wxp",
    "/I",
    "orig/toolchain/inc/crt",
]


@dataclass
class Module:
    name: str
    orig: Path
    image_base: int
    config_dir: Path
    src_dir: Path
    include_dir: Path
    build_dir: Path
    kind: str
    cflags: List[str]
    ldflags: List[str]
    res_script: Path
    output_name: str
    def_file: Optional[Path] = None
    runnable_base: Optional[int] = None
    progress_category: Optional[str] = None
    extra_sources: List[tuple] = field(default_factory=list)

    @property
    def config_yml(self) -> Path:
        return self.config_dir / "config.yml"

    @property
    def splits(self) -> Path:
        return self.config_dir / "splits.txt"

    @property
    def symbols(self) -> Path:
        return self.config_dir / "symbols.txt"

    @property
    def names(self) -> Path:
        return self.config_dir / "names.txt"

    @property
    def units_json(self) -> Path:
        return self.config_dir / "units.json"

    @property
    def objdiff_json(self) -> Path:
        return self.build_dir / "objdiff.json"

    @property
    def report(self) -> Path:
        return self.build_dir / "report.json"

    @property
    def assets_dir(self) -> Path:
        return self.build_dir / "assets"

    @property
    def output(self) -> Path:
        return self.build_dir / self.output_name

    @property
    def src_obj_dir(self) -> Path:
        return self.build_dir / "src"

    @property
    def build_config(self) -> Path:
        return self.build_dir / "config.json"

    @property
    def res_obj(self) -> Path:
        return self.build_dir / f"{self.name}_res.obj"

    @property
    def res_path(self) -> Path:
        return self.build_dir / f"{self.name}.res"


def spider_module(version: str, build_dir: Path, map_file: bool = False) -> Module:
    includes = ["/I", "include/spider", *TOOLCHAIN_INCLUDES]
    ldflags = [
        "/nologo",
        "/MACHINE:I386",
        "/SUBSYSTEM:WINDOWS,4.0",
        "/OSVERSION:5.1",
        "/VERSION:5.1",
        "/BASE:0x01000000",
        "/FIXED",
        "/RELEASE",
        "/INCREMENTAL:NO",
        "/OPT:REF",
        "/OPT:ICF",
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "advapi32.lib",
        "shell32.lib",
        "winmm.lib",
        "comctl32.lib",
        "htmlhelp.lib",
        "libcmt.lib",
    ]
    if map_file:
        ldflags.append(f"/MAP:{build_dir / version / 'spider' / 'spider.map'}")
    return Module(
        name="spider",
        orig=Path("orig") / version / "spider.exe",
        image_base=0x01000000,
        config_dir=Path("config") / version / "spider",
        src_dir=Path("src/spider"),
        include_dir=Path("include/spider"),
        build_dir=build_dir / version / "spider",
        kind="exe",
        cflags=[
            "/W3",
            "/wd4234",
            "/MT",
            "/GR-",
            "/DUNICODE",
            "/D_UNICODE",
            "/DWIN32",
            "/D_WINDOWS",
            "/DNDEBUG",
            "/O1",
            *includes,
        ],
        ldflags=ldflags,
        res_script=Path("src/spider/spider.rc"),
        output_name="spider.exe",
        runnable_base=0x00400000,
        progress_category="game",
        extra_sources=[("winmain.c", "winmain.cpp", None)],
    )


def cards_module(version: str, build_dir: Path, map_file: bool = False) -> Module:
    includes = ["/I", "include/cards", *TOOLCHAIN_INCLUDES]
    ldflags = [
        "/nologo",
        "/DLL",
        "/MACHINE:I386",
        "/SUBSYSTEM:WINDOWS,4.0",
        "/OSVERSION:5.1",
        "/VERSION:5.1",
        "/BASE:0x6FC10000",
        "/NODEFAULTLIB",
        "/ENTRY:DllMain@12",
        "/DEF:src/cards/cards.def",
        "/RELEASE",
        "/INCREMENTAL:NO",
        "user32.lib",
        "gdi32.lib",
    ]
    if map_file:
        ldflags.append(f"/MAP:{build_dir / version / 'cards' / 'cards.map'}")
    return Module(
        name="cards",
        orig=Path("orig") / version / "cards.dll",
        image_base=0x6FC10000,
        config_dir=Path("config") / version / "cards",
        src_dir=Path("src/cards"),
        include_dir=Path("include/cards"),
        build_dir=build_dir / version / "cards",
        kind="dll",
        cflags=[
            "/W3",
            "/wd4234",
            "/TC",
            "/Zl",
            "/GR-",
            "/DWIN32",
            "/D_WINDOWS",
            "/DNDEBUG",
            "/O1",
            *includes,
        ],
        ldflags=ldflags,
        res_script=Path("src/cards/cards.rc"),
        output_name="cards.dll",
        def_file=Path("src/cards/cards.def"),
        progress_category="cards",
    )


def all_modules(version: str, build_dir: Path, map_file: bool = False) -> List[Module]:
    return [
        spider_module(version, build_dir, map_file),
        cards_module(version, build_dir, map_file),
    ]


def get_module(name: str, version: str = "XPSP1", build_dir: Path = Path("build")) -> Module:
    for mod in all_modules(version, build_dir):
        if mod.name == name:
            return mod
    raise KeyError(name)


def infer_module(path: Path, version: str = "XPSP1", build_dir: Path = Path("build")) -> Module:
    text = str(path).replace("\\", "/")
    for mod in all_modules(version, build_dir):
        markers = (
            f"src/{mod.name}/",
            f"include/{mod.name}/",
            f"config/{version}/{mod.name}/",
            f"build/{version}/{mod.name}/",
        )
        if any(m in text for m in markers):
            return mod
    return spider_module(version, build_dir)


def gold_bytes(mod: Module, addr: int, size: int) -> bytes:
    d = mod.orig.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    rva = addr - mod.image_base
    for i in range(nsec):
        o = pe + 24 + optsz + 40 * i
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        if va <= rva < va + max(vs, rs):
            off = rp + rva - va
            return d[off : off + size]
    raise SystemExit(f"{addr:#x} not in {mod.orig}")
