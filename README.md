# Spider Solitaire (Windows XP SP1) matching decompilation

Attempted byte-matching decompilation of English **Windows XP SP1** `spider.exe` (`5.1.2600.1106`, `xpsp1.020828-1920`) and English XP RTM `cards.dll` (`5.1.2600.0`, `xpclient.010817-1148`). Each binary is a module: `config/XPSP1/{spider,cards}/`, `src/{spider,cards}/`, `build/XPSP1/{spider,cards}/`.

This is a scaffold: split the original PE, compile C++ with the original MSVC 7.0 toolchain under Wine, and diff COFF objects in [objdiff](https://github.com/encounter/objdiff).

## Status

Regenerate the numbers with `ninja report && python3 configure.py progress`. "Exact" means objdiff reports 100% for the function and `cmp_reloc` finds no difference outside relocations. Bytes are counted only for exactly matching functions.

| | `spider.exe` | `cards.dll` |
|---|---|---|
| Functions exact | 533 / 543 (98.2%) | 11 / 12 in `cards.c` |
| Code bytes in exact functions | 55,407 / 60,151 (92.1%) | 1,776 / 1,975 in `cards.c` (89.9%) |
| Units complete | 283 / 293 | 0 / 1 (`cards.c` is the only source unit) |
| `.rsrc` (`cmp_rsrc`) | match (90 resources) | match (75 resources) |
| Whole image (`ninja check_<module>`) | fails: linker layout | fails: linker layout |

### spider.exe

Game code is 122 of 131 functions exact (81.4% of 24,782 bytes). The C runtime is 178 of 179 (99.5%). The nine game functions left are all between 96% and 99.9%. Each was checked by hand and computes the same values, makes the same calls, and takes the same branches as gold. What's left is register choice, stack-slot choice, or instruction order:

| Function | Match | What still differs |
|---|---|---|
| `GameBoard::draw_felt` | 99.7% | Six stack slots renamed consistently; `eax`/`edx` swapped in one block |
| `WinMain` | 99.9% | Window x/y loads use swapped registers |
| `AnimState::burst_fx` | 98.9% | `mov esi, eax` and `lea edi, [ebx+0xc]` swapped around an x87 load |
| `GameWin::paint_hdc` | 98.6% | `ExcludeClipRect` rectangle built by a shorter sequence (11 bytes); gold has one dead stack store |
| `GameWin::save_game` | 98.2% | Gold keeps 0 in `edi` for compares, ours uses immediates; one address has base and index swapped |
| `GameWin::move_run` | 97.1% | The constant zero lives in `edx` instead of `eax` |
| `DealView::full_suit` | 96.7% | `add esi, -2` and `lea edi, [eax+1]` in the opposite order |
| `AnimState::run_fx` | 96.1% | Frame is 20 bytes smaller, and the x87 block is scheduled differently (same rounding) |
| `GameBoard::slide_drag` | 96.1% | `(a+b) - b + c` vs `c - b + (a+b)`; 2 bytes longer |

The remaining CRT unit, `crt_free.c` (`0x0100988D`, 144 bytes), is two functions in one range: `_free` (56 bytes) and `_forcdecpt` (88 bytes). Each matches `libc.lib` exactly once relocations are masked. The unit needs splitting in two, not decompiling.

### cards.dll

| Function | Match | What still differs |
|---|---|---|
| `cdtInit` | 98.0% | In the early-out, gold loads `pdx` before `g_width`; ours loads the global first (four bytes) |

All other functions are exact, including `cdtDrawExt` (800 bytes, jump table), `load_face` (the face-bitmap cache), and both corner save/restore helpers.

### Whole-image check

`ninja check_spider` and `ninja check_cards` fail before they reach the code differences above, because the rebuilt images are laid out differently from gold. In both binaries:

- gold folds `.rdata` into `.text`, and we emit a separate `.rdata`;
- gold's `FileAlignment` is 0x200, and ours is 0x1000;
- gold has a debug directory (CodeView record), and ours has none;
- `AddressOfEntryPoint` and `SizeOfStackReserve` differ.

Every address after the first layout difference shifts, so the per-section byte counts from `cmp_image.py` aren't meaningful yet. The next step is linker options (`/MERGE:.rdata=.text`, 0x200 alignment, a debug record), not source changes. Spider has two more image-level gaps:

- Gold links the single-threaded `libc.lib`, and we link `libcmt.lib`. Our `free`, `srand` and `rand` are therefore the multithreaded versions, with a heap lock and per-thread state.
- The nine near-miss functions change some function sizes (net −7 bytes), so later code addresses move even once the layout matches.

## Why SP1

Four dumps were compared. English SP1 is the easiest matching target:

| Build | Compiler | `.text` | `/GS` + hotpatch |
|-------|----------|---------|------------------|
| **XP SP1 (this project)** | VC7.0 13.00.9178 / link 7.00 | 60152 | no |
| XP RTM (Turkish) | same VC7.0 | 60136 | no |
| Tablet 2005 / XP SP3 | VC7.1 13.10.4035 | 64586 | yes |

SP3 is a different compile. Matching SP1 will not produce an SP3-identical exe.

## Dependencies (macOS)

- Python 3
- [ninja](https://ninja-build.org/) (`brew install ninja`)
- [Wine](https://wiki.winehq.org/MacOS) 9+ — already used here as Wine 11.x
- A **VC7.0 `cl.exe` 13.00.9178** tree (XP DDK / XP build-lab compiler). Not committed. See [orig/README.md](orig/README.md).
- Original `orig/XPSP1/spider.exe` and `orig/XPSP1/cards.dll`

`configure.py` downloads:

- `dtk` v0.0.29 from [openblack/decomp-toolkit](https://github.com/openblack/decomp-toolkit) (`coff_prototype`, PE split)
- `objdiff-cli` v3.8.2 from [encounter/objdiff](https://github.com/encounter/objdiff)

## Setup

1. Copy the SP1 exe and English `cards.dll` into `orig/XPSP1/` if they are not already there.
2. Install the 13.00.9178 toolchain under `orig/toolchain/` (see `orig/README.md`).
3. Check the compiler:

```sh
python3 tools/check_compiler.py
```

Retail VS .NET 2002 `13.00.9466` is rejected until a canary object matches (`--allow-unproven`). VC7.1 `13.10.4035` (SP3) is always rejected.

4. Configure and split:

```sh
python3 configure.py
ninja
```

The first `ninja` downloads dtk, splits `spider.exe` into expected COFF objects, then re-runs `configure.py`. After the toolchain is in place:

```sh
ninja all_source          # compile every module's sources with Wine cl
ninja spider              # link build/XPSP1/spider/spider.exe
ninja cards               # link build/XPSP1/cards/cards.dll
ninja report              # build/XPSP1/{spider,cards}/report.json
python3 configure.py progress
```

Each module is measured and checked on its own:

| Target | What it does |
|--------|--------------|
| `ninja report_spider` / `ninja report_cards` | objdiff report for that module only, from `build/XPSP1/<module>/objdiff.json` |
| `ninja check_spider` / `ninja check_cards` | Byte-compares the rebuilt image with `orig/XPSP1/<binary>` (`tools/cmp_image.py`) |
| `ninja report` / `ninja check` | Both modules |

Both gold images were processed by `bind.exe`, so a whole-file SHA-1 can never match. `cmp_image.py` normalizes both sides first (IAT restored from the import name table, bind timestamps and the bound-import directory zeroed, checksum and link/debug timestamps zeroed) and then requires every other byte to match. On failure it names the differing header fields and the first differing addresses per section. The root `objdiff.json` still lists every module for the GUI.

## objdiff

Download the macOS arm64 GUI from [objdiff releases](https://github.com/encounter/objdiff/releases) (`objdiff-macos-arm64`).

Open the app, set **Project directory** to this repository root. `objdiff.json` is generated by `configure.py` (`custom_make: ninja`, `build_base` / `build_target` on). The sidebar lists split units; renamed functions show the label from `names.txt`. Saving a `.c` / `.cpp` / `.h` rebuilds automatically.

## Readable names (`names.txt`)

Instruction bytes do not contain C++ names. objdiff still pairs a function only when the COFF symbol on the Wine-built object is the same string as on the split object, including every call reloc.

MSVC encodes that string from the C++ declaration, so each renamed spider function is declared once in `include/spider/game_api.h` and defined in its unit's source file (`src/spider/game/<Class>/<method>.cpp`; `config/XPSP1/spider/units.json` maps each `fn_<address>` unit to its `source`). Callers must use that declaration; a local copy on another class mangles to a different symbol.

`config/XPSP1/spider/names.txt` is the objdiff sidebar label only (`fn_01007836` → `CardColumn::slot_empty`). It is not the linker symbol. After changing a declaration or this file:

```sh
ninja all_source          # compile so the .obj has the new mangled name
python3 configure.py      # copies that COFF symbol into symbols.txt
ninja                     # re-split so the expected object uses the same name
```

`python3 configure.py` runs `tools/sync_symbols.py` for every `fn_<address>.obj` listed in `names.txt`.

## Layout

| Path | Purpose |
|------|---------|
| `orig/XPSP1/spider.exe` | Matching target |
| `orig/XPSP1/cards.dll` | Matching target (English XP RTM cards library) |
| `orig/toolchain/` | User-supplied `cl.exe` / `link.exe` / headers / `libcmt.lib` |
| `orig/XPSP3`, `TABLET`, `TR_RTM` | Reference binaries only |
| `config/XPSP1/spider/`, `config/XPSP1/cards/` | Per-module `config.yml`, `splits.txt`, `symbols.txt`, `names.txt`, `units.json` |
| `src/spider/`, `include/spider/` | Spider decompiled C / C++ (`game_api.h` holds shared declarations) |
| `src/cards/`, `include/cards/` | cards.dll C, `.def`, and `cards.rc` |
| `src/spider/spider.rc` | Resource script: menu, dialogs, strings, accelerators, version info |
| `build/XPSP1/spider/assets/`, `build/XPSP1/cards/assets/` | Media extracted from each original by `configure.py` |
| `tools/wine_msvc.sh` | Wine wrapper (project `WINEPREFIX`, `z:` path rewrite) |
| `configure.py` | Writes `build.ninja` + `objdiff.json` |

## Resources

`python3 configure.py` writes every media resource of each original into `build/XPSP1/<module>/assets/` as a normal file (`bitmaps/*.bmp`, `icons/*.ico`, `sounds/*.wav`, `manifest/1.manifest`). They are copyrighted, so they stay out of the repository. `ninja` compiles that module's `.rc` with the toolchain's `rc.exe` (`/i` the assets dir), converts it with `cvtres`, and links it. The rebuilt `.rsrc` section is byte-identical to the original's once data addresses are taken relative to the section:

```sh
python3 tools/cmp_rsrc.py orig/XPSP1/spider.exe build/XPSP1/spider/spider.exe   # RSRC MATCH
python3 tools/cmp_rsrc.py orig/XPSP1/cards.dll build/XPSP1/cards/cards.dll
```

`rc` writes resource data in statement order (string tables always last), so the order of statements in the `.rc` is part of the match. `python3 tools/extract_assets.py orig/XPSP1/cards.dll build/XPSP1/cards/assets --rc src/cards/cards.rc` regenerates the script from the original.

## cards.dll

Gold is one C translation unit (`src/cards/cards.c`), compiled `/O1 /TC /Zl` with no `/DUNICODE`, linked `/NODEFAULTLIB /ENTRY:DllMain@12 /DEF:src/cards/cards.def`. Exports: `WEP`, `cdtAnimate`, `cdtDraw`, `cdtDrawExt`, `cdtInit`, `cdtTerm`. Like spider, its image check is `ninja check_cards`.

CRT is not decompiled: once the toolchain is installed, pull matching objects from that `libcmt.lib` and list them in `configure.py` / `splits.txt`.

## Compiler flags (starting point)

Spider:

```
cl  /W3 /wd4234 /MT /GR- /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS /DNDEBUG /O1
link /MACHINE:I386 /SUBSYSTEM:WINDOWS,4.0 /OSVERSION:5.1 /VERSION:5.1
     /BASE:0x01000000 /FIXED /RELEASE /INCREMENTAL:NO /OPT:REF /OPT:ICF
```

cards.dll:

```
cl  /W3 /wd4234 /TC /Zl /GR- /DWIN32 /D_WINDOWS /DNDEBUG /O1
link /DLL /MACHINE:I386 /SUBSYSTEM:WINDOWS,4.0 /OSVERSION:5.1 /VERSION:5.1
     /BASE:0x6FC10000 /NODEFAULTLIB /ENTRY:DllMain@12 /DEF:src/cards/cards.def
```

`/O1` is locked by a 100% objdiff match on spider `fn_01007836` and by reloc-matched cards helpers (`DllMain`, `WEP`, `delete_if`, `cdtDraw`). No `/GS`, `/hotpatch`, or `/SAFESEH` (those are the SP2/SP3 / VC7.1 additions).
