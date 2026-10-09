#!/usr/bin/env python3
"""Generate build.ninja and objdiff.json for XP SP1 spider.exe and cards.dll."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from tools.extract_assets import extract_assets
from tools.modules import all_modules
from tools.project import (
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
    load_unit_info,
    objects_for_module,
)
from tools.sync_symbols import sync_symbols

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
config.dtk_tag = "v0.0.29"
config.objdiff_tag = "v3.8.2"

for name in ("spider.exe", "cards.dll"):
    old = Path("orig") / config.version / name
    new = Path("orig") / name
    if old.is_file() and not new.is_file():
        print(f"warning: {old} is the old path; move it to {new}")

config.modules = [m for m in all_modules(config.version, config.build_dir, args.map) if m.config_yml.is_file()]
if not config.modules:
    sys.exit(f"no modules under config/{config.version}/")

config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("crt", "C Runtime"),
    ProgressCategory("cards", "cards.dll"),
]

for module in config.modules:
    units = load_unit_info(module.units_json)
    config.module_units[module.name] = units
    config.module_objects[module.name] = objects_for_module(module, units)

if args.mode == "configure":
    for module in config.modules:
        updated = sync_symbols(module)
        if updated:
            print(f"Synced {updated} COFF symbol(s) into {module.symbols}")
        if module.orig.is_file():
            files = extract_assets(module.orig, module.assets_dir)
            config.module_assets[module.name] = files
            print(f"Extracted {len(files)} asset file(s) into {module.assets_dir}")
        else:
            print(f"warning: {module.orig} missing; {module.name} resources will not build")
    generate_build(config)
elif args.mode == "progress":
    calculate_progress(config)
else:
    sys.exit(f"Unknown mode: {args.mode}")
