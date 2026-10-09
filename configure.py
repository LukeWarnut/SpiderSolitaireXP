#!/usr/bin/env python3
"""Generate build.ninja and objdiff.json for XP SP1 spider.exe and cards.dll."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List

from tools.extract_assets import extract_assets, resource_script, write_if_changed
from tools.modules import Module, all_modules, file_sha1
from tools.pe_rsrc import ResourceSection
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
ALLOW_FLAG = "--allow-nonmatching"

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
parser.add_argument(
    "--orig",
    type=Path,
    default=Path("orig"),
    help="directory holding spider.exe / cards.dll to take resources from (default orig). "
    "A binary missing there is taken from orig/.",
)
parser.add_argument(
    ALLOW_FLAG,
    "-y",
    dest="allow_nonmatching",
    action="store_true",
    help="build from a binary that is not gold without asking",
)
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


def select_sources(modules: List[Module], orig_dir: Path) -> List[Module]:
    """Point each module at its binary; return the modules whose binary is not gold."""
    nonmatching = []
    for module in modules:
        candidate = orig_dir / module.orig.name
        source = candidate if candidate.is_file() else module.orig
        module.source = source
        if not source.is_file():
            continue
        module.matching = file_sha1(source) == module.gold_sha1
        if not module.matching:
            nonmatching.append(module)
    return nonmatching


def describe(path: Path) -> str:
    rs = ResourceSection(path)
    langs = ", ".join(f"{lang:#06x}" for lang in sorted(rs.languages()))
    return f"version {rs.file_version() or 'unknown'}, language {langs}"


def confirm_nonmatching(modules: List[Module]) -> bool:
    print("warning: these binaries are not the builds this decompilation matches:", file=sys.stderr)
    for module in modules:
        source = module.resource_binary
        print(f"  {module.name}: {source.as_posix()} ({describe(source)})", file=sys.stderr)
        print(f"      sha1 {file_sha1(source)}, gold {module.gold_sha1}", file=sys.stderr)
    names = ", ".join(m.name for m in modules)
    print(
        "The decompiled code will be built with these binaries' resources and assets in\n"
        "place of gold's. The output will not match gold, and splitting, objdiff,\n"
        f"report_<module>, and check_<module> are disabled for: {names}.",
        file=sys.stderr,
    )
    if args.allow_nonmatching:
        return True
    try:
        answer = input("Continue? [y/N] ")
    except EOFError:
        print(f"\nNo answer on stdin. Re-run with {ALLOW_FLAG} to continue.", file=sys.stderr)
        return False
    return answer.strip().lower() in ("y", "yes")


def write_split_yml(module: Module) -> None:
    """Copy config.yml with object_base/object pointing at gold outside orig/."""
    if module.split_yml == module.config_yml:
        return
    source = module.resource_binary
    text = module.config_yml.read_text(encoding="utf-8")
    text = re.sub(r"(?m)^object_base:.*$", f"object_base: {source.parent.as_posix()}", text)
    text = re.sub(r"(?m)^object:.*$", f"object: {source.name}", text)
    module.build_dir.mkdir(parents=True, exist_ok=True)
    write_if_changed(module.split_yml, text.encode("utf-8"))


def prune_assets(assets_dir: Path, keep: List[Path]) -> None:
    """Drop media left by a previously selected build."""
    keep_set = {p.resolve() for p in keep}
    if not assets_dir.is_dir():
        return
    for path in assets_dir.rglob("*"):
        if path.is_file() and path.resolve() not in keep_set:
            path.unlink()


if args.mode == "configure":
    nonmatching = select_sources(config.modules, args.orig)
    configure_args = sys.argv[1:]
    if nonmatching:
        if not confirm_nonmatching(nonmatching):
            sys.exit("Stopped. Nothing was written.")
        if not args.allow_nonmatching:
            # ninja re-runs configure.py without a terminal; carry the answer over.
            configure_args = configure_args + [ALLOW_FLAG]
    config.configure_args = configure_args

    for module in config.modules:
        updated = sync_symbols(module)
        if updated:
            print(f"Synced {updated} COFF symbol(s) into {module.symbols}")
        source = module.resource_binary
        if not source.is_file():
            print(f"warning: {module.orig} missing; {module.name} resources will not build")
            continue
        files = extract_assets(source, module.assets_dir)
        prune_assets(module.assets_dir, files)
        config.module_assets[module.name] = files
        print(f"Extracted {len(files)} asset file(s) from {source.as_posix()} into {module.assets_dir.as_posix()}")
        if module.matching:
            write_split_yml(module)
        else:
            script = resource_script(ResourceSection(source), orig=source, assets=module.assets_dir)
            write_if_changed(module.resource_script, script.encode("ascii"))
            print(f"Wrote {module.resource_script.as_posix()} from {source.as_posix()} (not gold)")
    generate_build(config)
elif args.mode == "progress":
    calculate_progress(config)
else:
    sys.exit(f"Unknown mode: {args.mode}")
