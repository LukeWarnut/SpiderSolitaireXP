"""Version-first matching modules (spider.exe, cards.dll)."""
from __future__ import annotations

import hashlib
import re
import struct
from dataclasses import dataclass
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
    # Link input order is recorded in the Rich header: gold links the .exp that
    # `lib /DEF` built first, then the resource object, then the code objects.
    res_first: bool = False
    progress_category: Optional[str] = None
    # The binary configure.py took resources and assets from. When it is not
    # gold (matching False), the build uses its resources and cannot split,
    # report, or check against gold.
    source: Optional[Path] = None
    matching: bool = True

    @property
    def config_yml(self) -> Path:
        return self.config_dir / "config.yml"

    @property
    def gold_sha1(self) -> Optional[str]:
        if not self.config_yml.is_file():
            return None
        m = re.search(r"(?m)^hash:\s*([0-9a-fA-F]{40})\s*$", self.config_yml.read_text(encoding="utf-8"))
        return m.group(1).lower() if m else None

    @property
    def resource_binary(self) -> Path:
        return self.source or self.orig

    @property
    def resource_script(self) -> Path:
        """src/<module>.rc for gold; otherwise a script decompiled from the source binary."""
        return self.res_script if self.matching else self.build_dir / f"{self.name}.rc"

    @property
    def split_yml(self) -> Path:
        """dtk config. A copy pointing at the source binary when gold lives outside orig/."""
        if self.source is None or self.source == self.orig:
            return self.config_yml
        return self.build_dir / "split.yml"

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

    @property
    def exp_path(self) -> Path:
        return self.build_dir / f"{self.name}.exp"

    @property
    def implib_path(self) -> Path:
        return self.build_dir / f"{self.name}.lib"

    @property
    def pdb_path(self) -> Path:
        return self.build_dir / f"{self.name}.pdb"


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
        "/MERGE:.rdata=.text",
        "/FILEALIGN:0x200",
        "/STACK:0x40000",
        "/SECTION:.rsrc,R",
        "/TSAWARE",
        "/DEBUG",
        "/DEBUGTYPE:VC6",
        f"/PDB:{(build_dir / version / 'spider' / 'spider.pdb').as_posix()}",
        "/PDBALTPATH:spider.pdb",
        # Import descriptors follow library order.
        "advapi32.lib",
        "kernel32.lib",
        "gdi32.lib",
        "user32.lib",
        "shell32.lib",
        "winmm.lib",
        "comctl32.lib",
        "htmlhelp.lib",
        "libc.lib",
    ]
    if map_file:
        ldflags.append(f"/MAP:{(build_dir / version / 'spider' / 'spider.map').as_posix()}")
    return Module(
        name="spider",
        orig=Path("orig/spider.exe"),
        image_base=0x01000000,
        config_dir=Path("config") / version / "spider",
        src_dir=Path("src/spider"),
        include_dir=Path("include/spider"),
        build_dir=build_dir / version / "spider",
        kind="exe",
        cflags=[
            "/W3",
            "/wd4234",
            "/ML",
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
        progress_category="game",
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
        "/RELEASE",
        "/INCREMENTAL:NO",
        "/MERGE:.rdata=.text",
        "/FILEALIGN:0x200",
        "/STACK:0x40000",
        "/SECTION:.rsrc,R",
        "/OPT:REF",
        "/DEBUG",
        "/DEBUGTYPE:VC6",
        f"/PDB:{(build_dir / version / 'cards' / 'cards.pdb').as_posix()}",
        "/PDBALTPATH:cards.pdb",
        "user32.lib",
        "gdi32.lib",
    ]
    if map_file:
        ldflags.append(f"/MAP:{(build_dir / version / 'cards' / 'cards.map').as_posix()}")
    return Module(
        name="cards",
        orig=Path("orig/cards.dll"),
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
        res_first=True,
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


def file_sha1(path: Path) -> str:
    return hashlib.sha1(path.read_bytes()).hexdigest()


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
