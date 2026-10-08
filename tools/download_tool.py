#!/usr/bin/env python3
"""Download pinned dtk (openblack) and objdiff-cli (encounter) binaries."""

from __future__ import annotations

import argparse
import platform
import stat
import sys
import urllib.request
from pathlib import Path

DTK_REPO = "openblack/decomp-toolkit"
OBJDIFF_REPO = "encounter/objdiff"


def host() -> tuple[str, str]:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "darwin":
        osname = "macos"
        arch = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    elif system == "linux":
        osname = "linux"
        arch = "aarch64" if machine in ("arm64", "aarch64") else "x86_64"
        if machine in ("i386", "i686"):
            arch = "i686"
    elif system == "windows":
        osname = "windows"
        arch = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    else:
        sys.exit(f"unsupported host {system}/{machine}")
    return osname, arch


def asset_name(tool: str, osname: str, arch: str) -> str:
    suffix = ".exe" if osname == "windows" else ""
    if tool == "dtk":
        return f"dtk-{osname}-{arch}{suffix}"
    if tool == "objdiff-cli":
        return f"objdiff-cli-{osname}-{arch}{suffix}"
    sys.exit(f"unknown tool {tool}")


def repo_for(tool: str) -> str:
    if tool == "dtk":
        return DTK_REPO
    if tool == "objdiff-cli":
        return OBJDIFF_REPO
    sys.exit(f"unknown tool {tool}")


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".tmp")
    print(f"Downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "spiderxp-decomp"})
    with urllib.request.urlopen(req) as resp, open(tmp, "wb") as out:
        out.write(resp.read())
    tmp.replace(dest)
    dest.chmod(dest.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", required=True, choices=("dtk", "objdiff-cli"))
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if args.output.is_file() and args.output.stat().st_size > 0 and not args.force:
        print(f"Already have {args.output}")
        return 0

    osname, arch = host()
    name = asset_name(args.tool, osname, arch)
    url = f"https://github.com/{repo_for(args.tool)}/releases/download/{args.tag}/{name}"
    try:
        download(url, args.output)
    except Exception as exc:  # noqa: BLE001
        print(f"error: failed to download {url}: {exc}", file=sys.stderr)
        return 1
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
