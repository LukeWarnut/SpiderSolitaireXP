# Original binaries and toolchain

This directory is gitignored except for this file and `toolchain/.gitkeep`.

## Binaries

Place (or keep) the dumps here:

| Path | Role |
|------|------|
| `XPSP1/spider.exe` | **Matching target** — English XP SP1 (`5.1.2600.1106`) |
| `cards.dll` | Unmodified English `cards.dll` (copied next to the rebuilt exe) |
| `XPSP3/spider.exe` | Reference only (VC7.1 /GS build) |
| `TABLET/spider.exe` | Reference only (SP2-era, same family as SP3) |
| `TR_RTM/spider.exe` | Reference only (Turkish RTM, VC7.0) |

`configure.py` expects `orig/XPSP1/spider.exe` and `orig/cards.dll`.

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
