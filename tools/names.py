"""Readable labels for game units. Symbols themselves come from the compiler."""

from __future__ import annotations

from pathlib import Path


def load_names(path: Path) -> dict[str, str]:
    labels: dict[str, str] = {}
    if not path.is_file():
        return labels
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.split("#", 1)[0].strip()
        if not text:
            continue
        ident, label = text.split(None, 1)
        labels[ident] = label.strip()
    return labels
