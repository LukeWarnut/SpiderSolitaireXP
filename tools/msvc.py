#!/usr/bin/env python3
"""Run a VC7.0 tool (cl, link, lib, rc, cvtres, ml) from orig/toolchain.

Windows executes the toolchain binary directly. INCLUDE and LIB are set from
that tree, ignoring a host Visual Studio prompt. macOS and Linux hand off to
tools/wine_msvc.sh, which runs the same tools under Wine.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_TOOL_LAYOUTS = (
    "bin/{name}",
    "BIN/{name}",
    "{name}",
    "VC7/bin/{name}",
    "VC7/BIN/{name}",
    "bin/x86/{name}",
)


def toolchain_root() -> Path:
    override = os.environ.get("MSVC_TOOLCHAIN")
    if override:
        return Path(override)
    return ROOT / "orig" / "toolchain"


def tool_filename(name: str) -> str:
    if name.lower().endswith(".exe"):
        return name
    return name + ".exe"


def find_tool(name: str, toolchain: Path | None = None) -> Path | None:
    root = toolchain if toolchain is not None else toolchain_root()
    filename = tool_filename(name)
    for layout in _TOOL_LAYOUTS:
        path = root.joinpath(*layout.format(name=filename).split("/"))
        if path.is_file():
            return path
    return None


def _existing(dirs: list[Path]) -> str:
    found: list[str] = []
    seen: set[str] = set()
    for path in dirs:
        if not path.is_dir():
            continue
        # Windows treats include and INCLUDE as the same directory.
        key = os.path.normcase(str(path.resolve()))
        if key in seen:
            continue
        seen.add(key)
        found.append(str(path))
    return ";".join(found)


def include_dirs(toolchain: Path | None = None) -> str:
    root = toolchain if toolchain is not None else toolchain_root()
    return _existing(
        [
            ROOT / "include",
            root / "include",
            root / "INCLUDE",
            root / "inc",
            root / "inc" / "wxp",
            root / "inc" / "crt",
            root / "inc" / "crt" / "sys",
        ]
    )


def lib_dirs(toolchain: Path | None = None) -> str:
    root = toolchain if toolchain is not None else toolchain_root()
    return _existing(
        [
            root / "lib",
            root / "LIB",
            root / "lib" / "wnet" / "i386",
            root / "lib" / "wxp" / "i386",
        ]
    )


def tool_environ(exe: Path, toolchain: Path | None = None) -> dict[str, str]:
    env = os.environ.copy()
    # Always replace host INCLUDE/LIB. A VS developer prompt would otherwise
    # point cl and link at a compiler that cannot match XP SP1.
    env["INCLUDE"] = include_dirs(toolchain)
    env["LIB"] = lib_dirs(toolchain)
    bindir = str(exe.parent)
    env["PATH"] = bindir + os.pathsep + env.get("PATH", "")
    return env


def _handoff_wine() -> None:
    wine = ROOT / "tools" / "wine_msvc.sh"
    if not wine.is_file():
        print(f"error: Wine wrapper missing: {wine}", file=sys.stderr)
        raise SystemExit(1)
    argv = [str(wine), *sys.argv[1:]]
    try:
        os.execv(wine, argv)
    except OSError:
        os.execvp("bash", ["bash", *argv])


def run_native(argv: list[str]) -> int:
    if not argv:
        print(
            "usage: msvc.py <cl|link|lib|rc|cvtres|ml> [args...]",
            file=sys.stderr,
        )
        return 2
    tool = argv[0]
    root = toolchain_root()
    exe = find_tool(tool, root)
    if exe is None:
        print(f"error: {tool_filename(tool)} not found under {root}", file=sys.stderr)
        print("See orig/README.md for the expected VC7.0 13.00.9178 layout.", file=sys.stderr)
        return 1
    completed = subprocess.run(
        [str(exe), *argv[1:]],
        cwd=ROOT,
        env=tool_environ(exe, root),
    )
    return int(completed.returncode)


def main() -> int:
    if os.name != "nt":
        _handoff_wine()
    return run_native(sys.argv[1:])


if __name__ == "__main__":
    sys.exit(main())
