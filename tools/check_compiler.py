#!/usr/bin/env python3
"""Verify the MSVC 7.0 toolchain is the XP SP1 matching compiler (13.00.9178)."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WRAPPER = ROOT / "tools" / "wine_msvc.sh"

# XP DDK 2600.1106: cl/c1 banner is 13.00.9176; c2.dll (codegen) is 13.00.9178
# which is what spider.exe's Rich header records.
REQUIRED_CL = "13.00.9178"
ACCEPTED_CL = ("13.00.9178", "13.00.9176")
REQUIRED_LINK = "7.00.9210"
REJECT_CL = (
    "13.10.",  # VC7.1 (SP3 / Tablet)
    "13.00.9466",  # retail VS .NET 2002 unless proven
    "14.",
    "12.",
)


def run_tool(name: str) -> str:
    env = os.environ.copy()
    env.setdefault("WINEDEBUG", "-all")
    proc = subprocess.run(
        [str(WRAPPER), name],
        cwd=ROOT,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    return proc.stdout


def parse_cl_version(output: str) -> str | None:
    m = re.search(r"Version\s+(\d+\.\d+\.\d+)", output)
    return m.group(1) if m else None


def parse_link_version(output: str) -> str | None:
    m = re.search(r"Version\s+(\d+\.\d+(?:\.\d+)?)", output)
    return m.group(1) if m else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--allow-unproven",
        action="store_true",
        help="warn instead of failing on 13.00.9466",
    )
    args = parser.parse_args()

    if not (ROOT / "orig" / "toolchain").exists():
        print("error: orig/toolchain/ is missing", file=sys.stderr)
        return 1

    cl_out = run_tool("cl")
    cl_ver = parse_cl_version(cl_out)
    if cl_ver is None:
        print("error: could not run cl.exe / parse its version banner:", file=sys.stderr)
        print(cl_out or "(no output — is Wine installed and orig/toolchain populated?)", file=sys.stderr)
        return 1

    print(f"cl.exe {cl_ver}")
    if cl_ver.startswith(REJECT_CL) or any(p in cl_ver for p in ("13.10.", "14.", "12.")):
        print(
            f"error: {cl_ver} cannot match English XP SP1 spider.exe "
            f"(need {REQUIRED_CL}, not VC7.1 4035 or VC6)",
            file=sys.stderr,
        )
        return 1
    if cl_ver == "13.00.9466" and not args.allow_unproven:
        print(
            "error: retail VS .NET 2002 13.00.9466 is not assumed equal to "
            f"build-lab {REQUIRED_CL}. Re-run with --allow-unproven to try it.",
            file=sys.stderr,
        )
        return 1
    if cl_ver not in ACCEPTED_CL:
        print(
            f"warning: wanted cl {REQUIRED_CL} (or DDK front-end 13.00.9176), "
            f"got {cl_ver}. Proceed only if a canary object matches.",
            file=sys.stderr,
        )
    elif cl_ver == "13.00.9176":
        print("cl.exe 13.00.9176 is XP DDK 2600.1106 (c2.dll is 13.00.9178)")

    link_out = run_tool("link")
    link_ver = parse_link_version(link_out)
    if link_ver:
        print(f"link.exe {link_ver}")
        if not link_ver.startswith("7.00"):
            print(
                f"error: linker {link_ver} is not 7.00 (XP SP1 used link {REQUIRED_LINK})",
                file=sys.stderr,
            )
            return 1
    else:
        print("warning: could not parse link.exe version banner", file=sys.stderr)

    print("toolchain ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
