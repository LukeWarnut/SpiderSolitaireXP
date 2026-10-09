# Original binaries and toolchain

This directory is gitignored except for this file and `toolchain/.gitkeep`.

## Binaries

The original binaries are required to compile this project to aquire the necessary assets. Place them here:

| Path | Role |
|------|------|
| `XPSP1/spider.exe` | **Matching target** — English XP SP1 (`5.1.2600.1106`) |
| `XPSP1/cards.dll` | **Matching target** — English XP RTM cards library (`5.1.2600.0`) |

## Toolchain (not committed)

Drop a **Visual C++ 13.00.9178** / **link 7.00.9210** tree under `orig/toolchain/`. That is the XP SP1 build-lab / XP DDK compiler (VC7.0). Retail VS .NET 2002 `13.00.9466` and VC7.1 `13.10.4035` will be rejected until proven to match.

Accepted layouts:

```
orig/toolchain/bin/cl.exe
orig/toolchain/bin/link.exe
orig/toolchain/include/   (or inc/)
orig/toolchain/lib/
```

or `cl.exe` / `link.exe` directly in `orig/toolchain/`.

Then:

```
python3 tools/check_compiler.py
```

On Windows, use `python` instead of `python3`. The checker runs `cl.exe` and `link.exe` directly there. On macOS and Linux it runs them under Wine.
