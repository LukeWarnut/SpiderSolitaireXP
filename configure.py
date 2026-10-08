#!/usr/bin/env python3
"""Generate build.ninja and objdiff.json for English XP SP1 spider.exe."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from tools.extract_assets import extract_assets
from tools.sync_symbols import sync_symbols
from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
    load_unit_info,
)

VERSIONS = ["XPSP1"]
DEFAULT_VERSION = "XPSP1"

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    default=DEFAULT_VERSION,
)
parser.add_argument("--build-dir", type=Path, default=Path("build"))
parser.add_argument("--dtk", type=Path)
parser.add_argument("--objdiff", type=Path)
parser.add_argument("--ninja", type=Path)
if not is_windows():
    parser.add_argument("--wrapper", type=Path, help="Wine wrapper (default tools/wine_msvc.sh)")
parser.add_argument("--map", action="store_true")
args = parser.parse_args()

config = ProjectConfig()
config.version = args.version
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.ninja_path = args.ninja
if not is_windows() and args.wrapper:
    config.wrapper = args.wrapper

config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.orig_exe = Path("orig") / config.version / "spider.exe"
config.units_path = Path("config") / config.version / "units.json"
config.unit_info = load_unit_info(config.units_path)
config.res_script = Path("src") / "spider.rc"
config.assets_dir = config.out_path() / "assets"
config.dtk_tag = "v0.0.29"
config.objdiff_tag = "v3.8.2"

includes = [
    "/I",
    "include",
    "/I",
    "orig/toolchain/inc",
    "/I",
    "orig/toolchain/inc/wxp",
    "/I",
    "orig/toolchain/inc/crt",
]

config.cflags = [
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
]

config.ldflags = [
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
if args.map:
    config.ldflags.append(f"/MAP:build/{config.version}/spider.map")

Matching = True
NonMatching = False

config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("crt", "C Runtime"),
]

game_objects = []
for unit in config.unit_info.values():
    if unit.get("kind") != "game" or not unit.get("source"):
        continue
    game_objects.append(
        Object(
            Matching if unit.get("complete") else NonMatching,
            unit["name"],
            source=unit["source"],
            progress_category="game",
        )
    )

config.libs = [
    {
        "lib": "spider",
        "cflags": config.cflags,
        "progress_category": "game",
        "objects": game_objects
        + [
            Object(
                NonMatching,
                "winmain.c",
                source="winmain.cpp",
                progress_category=None,
            )
        ],
    }
]

if args.mode == "configure":
    updated = sync_symbols()
    if updated:
        print(f"Synced {updated} COFF symbol(s) into config/{config.version}/symbols.txt")
    if config.orig_exe.is_file():
        config.asset_files = extract_assets(config.orig_exe, config.assets_dir)
        print(f"Extracted {len(config.asset_files)} asset file(s) into {config.assets_dir}")
    else:
        print(f"warning: {config.orig_exe} missing; resources will not build")
    generate_build(config)
elif args.mode == "progress":
    calculate_progress(config)
else:
    sys.exit(f"Unknown mode: {args.mode}")
