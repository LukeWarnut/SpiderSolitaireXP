#!/usr/bin/env python3
"""Split one PE with dtk, then repair .bss section headers.

Windows ninja (including the Kitware build) launches the command directly and
does not treat ``&&`` as a shell operator. This runs both steps in one process.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    if len(sys.argv) != 5:
        print(
            "usage: split_module.py <dtk> <config.yml> <out_dir> <splits.txt>",
            file=sys.stderr,
        )
        return 2
    dtk, config, out_dir, splits = sys.argv[1:]
    split = subprocess.run([dtk, "coff", "split", "--no-update", config, out_dir])
    if split.returncode != 0:
        return int(split.returncode)
    fix = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "fix_bss.py"), out_dir, splits]
    )
    return int(fix.returncode)


if __name__ == "__main__":
    sys.exit(main())
