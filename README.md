# Spider Solitaire (Windows XP SP1) decompilation

Attempted byte-matching decompilation of English **Windows XP SP1** `spider.exe` (`5.1.2600.1106`, `xpsp1.020828-1920`) and English XP RTM `cards.dll` (`5.1.2600.0`, `xpclient.010817-1148`). Each binary is a module: `config/XPSP1/{spider,cards}/`, `src/{spider,cards}/`, `build/XPSP1/{spider,cards}/`.

`spider.exe` does not call `cards.dll`. It draws from bitmaps in its own resources. The DLL is a second matching target: the shared card library used by Solitaire and FreeCell.

## macOS port

[`src/mac`](src/mac/README.md) is a playable 64-bit port. It keeps this decompilation's rules, scoring, undo, and save format, and replaces the Win32/GDI shell with an SDL3 window, a Metal renderer, and AppKit menus and dialogs. It does not compile the matching sources and does not try to match the original instruction bytes.

Build instructions are in [src/mac/README.md](src/mac/README.md).

## Status

Regenerate the numbers with `ninja report && python3 configure.py progress`. "Exact" means objdiff reports 100% for the function and `cmp_reloc` finds no difference outside relocations. Bytes are counted only for exactly matching functions.

| | `spider.exe` | `cards.dll` |
|---|---|---|
| Functions exact | 533 / 543 (98.2%) | 12 / 12 in `cards.c` |
| Code bytes in exact functions | 55,407 / 60,151 (92.1%) | 1,975 / 1,975 in `cards.c` (100%) |
| Units complete | 283 / 293 | 1 / 1 (`cards.c` is the only source unit) |
| `.rsrc` (`cmp_rsrc`) | match (90 resources) | match (75 resources) |
| Whole image (`ninja check_<module>`) | fails: translation-unit structure | **`IMAGE MATCH`** |

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
| `AnimState::run_fx` | 98.9% | Same size, frame, and stack slots; the `mov esi, [fx]` reload sits two `fst`s early |
| `GameBoard::slide_drag` | 96.1% | `(a+b) - b + c` vs `c - b + (a+b)`; 2 bytes longer |

The remaining CRT unit, `crt_free.c` (`0x0100988D`, 144 bytes), is two functions in one range: `_free` (56 bytes) and `_forcdecpt` (88 bytes). Each matches `libc.lib` exactly once relocations are masked. The unit needs splitting in two, not decompiling.

### cards.dll

Every function is exact (objdiff 100% and `cmp_reloc` clean), including `cdtDrawExt` (800 bytes, jump table), `load_face` (the face-bitmap cache), and both corner save/restore helpers.

The `.bss` (0x100 bytes of globals) also matches, so `cards.c` is complete. The globals are file-`static` (an extern width global changes the load order in `cdtInit`), and MSVC lays statics out by a hash of their names, not in declaration order. The names in `cards.c` were chosen so that the hash order reproduces gold's layout; see `AGENTS.md` before renaming any of them.

### Whole-image check

`ninja check_cards` prints `IMAGE MATCH (0x57e00 bytes)`. The two files are the same size, with the same section table, and `.data`, `.rsrc`, and `.reloc` are identical on disk. The check is not a raw file compare. `cmp_image.py` ignores these link-time and `bind.exe` differences, which are still present:

- The PE timestamp, the optional-header checksum, and the export-directory timestamp.
- The debug-directory timestamp, and the signature and age inside the `NB10` CodeView record. Both records name `cards.pdb`.
- A bound-import directory in the original only (48 bytes in the header slack, naming `USER32.dll` and `GDI32.dll`). Each of its import descriptors has `TimeDateStamp` and `ForwarderChain` set to `0xFFFFFFFF`; both are zero in the rebuild.
- The import address table at the start of `.text` (89 bytes). The original holds the absolute addresses `bind.exe` wrote; the rebuild still has the linker's relative virtual addresses into the import name table.

That took linker settings:

- `/MERGE:.rdata=.text /FILEALIGN:0x200 /STACK:0x40000 /SECTION:.rsrc,R /OPT:REF`;
- `/DEBUG /DEBUGTYPE:VC6 /PDBALTPATH:cards.pdb`: gold's debug directory holds an `NB10` (PDB 2.0) CodeView record naming just `cards.pdb`. This linker writes `RSDS` unless given the undocumented `/DEBUGTYPE:VC6`, and `/PDBALTPATH` replaces the full path;
- the exports come from a `cards.exp` built by `lib /DEF` from the objects, linked ahead of the resource object and then the code object. That input order is recorded in the Rich header, which `cmp_image.py` compares.

Spider links with the same settings plus `/TSAWARE`, the single-threaded `libc.lib` (`/ML`; changes no game code), gold's import-library order (`advapi32`, `kernel32`, `gdi32`, `user32`, ...), and gold's `WinMain` as `_WinMain@16`. The image has gold's file size, section table, imported functions and DLL order, debug record, and an identical `.rsrc`. It still fails, because gold was linked from eight C++ objects and ours from 127 (one per function unit). The object structure is visible in three ways:

- the Rich header counts C++ objects (8 versus 127), so it differs even with every function exact;
- the linker places code, inline COMDATs, and literals in object order, so game functions sit at different addresses, and the CRT objects pulled from `libc.lib` come in a different order;
- the import thunks within each DLL are in first-reference order, which differs too.

On top of that, the nine near-miss functions still change some function sizes (net −7 bytes). A spider image match therefore needs the game rebuilt as gold's eight translation units, which `AGENTS.md` notes changed codegen in 20 of 141 functions when tried.

## Why SP1

Four dumps were compared. English SP1 is the easiest matching target:

| Build | Compiler | `.text` | `/GS` + hotpatch |
|-------|----------|---------|------------------|
| **XP SP1 (this project)** | VC7.0 13.00.9178 / link 7.00 | 60152 | no |
| XP RTM (Turkish) | same VC7.0 | 60136 | no |
| Tablet 2005 / XP SP3 | VC7.1 13.10.4035 | 64586 | yes |

SP3 is a different compile. Matching SP1 will not produce an SP3-identical exe.

## Dependencies

Python 3, [ninja](https://ninja-build.org/), a **VC7.0 `cl.exe` 13.00.9178** tree (XP DDK / XP build-lab compiler, not committed; see [orig/README.md](orig/README.md)), and the original `orig/spider.exe` and `orig/cards.dll`.

`configure.py` downloads a host build of:

- `dtk` v0.0.29 from [openblack/decomp-toolkit](https://github.com/openblack/decomp-toolkit) (`coff_prototype`, PE split)
- `objdiff-cli` v3.8.2 from [encounter/objdiff](https://github.com/encounter/objdiff)

### macOS

- Ninja: `brew install ninja`
- [Wine](https://wiki.winehq.org/MacOS) 9+ — only Wine 11.18 tested. `tools/wine_msvc.sh` runs `cl.exe`, `link.exe`, `lib.exe`, `rc.exe`, and `cvtres.exe`.

### Windows

- Ninja on `PATH` (`winget install Ninja-build.Ninja`, or `ninja-win.zip` from the [Ninja releases](https://github.com/ninja-build/ninja/releases)).
- `tools/msvc.py` runs `cl.exe`, `link.exe`, `lib.exe`, `rc.exe`, and `cvtres.exe` from `orig/toolchain`. It sets `INCLUDE`, `LIB`, and `PATH` from that tree, including when a Visual Studio developer prompt has already set them.
- Spider links one object per function. The link step writes that list to a response file so it fits on the `cmd.exe` command line (8191 characters). Object order is unchanged.
- Run the commands below with `python` instead of `python3`.

## Setup

1. Copy the SP1 exe and English `cards.dll` into `orig/` if they are not already there.
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
ninja all_source          # compile every module's sources with cl.exe
ninja spider              # link build/XPSP1/spider/spider.exe
ninja cards               # link build/XPSP1/cards/cards.dll
ninja report              # build/XPSP1/{spider,cards}/report.json
python3 configure.py progress
```

Each module is measured and checked on its own:

| Target | What it does |
|--------|--------------|
| `ninja report_spider` / `ninja report_cards` | objdiff report for that module only, from `build/XPSP1/<module>/objdiff.json` |
| `ninja check_spider` / `ninja check_cards` | Byte-compares the rebuilt image with `orig/<binary>` (`tools/cmp_image.py`) |
| `ninja report` / `ninja check` | Both modules |

Both gold images were processed by `bind.exe`, so a whole-file SHA-1 can never match. `cmp_image.py` normalizes both sides first (IAT restored from the import name table, bind timestamps and the bound-import directory zeroed, checksum and link/debug timestamps zeroed) and then requires every other byte to match. On failure it names the differing header fields and the first differing addresses per section. The root `objdiff.json` still lists every module for the GUI.

## objdiff

Download the GUI from [objdiff releases](https://github.com/encounter/objdiff/releases): `objdiff-macos-arm64` on macOS, or `objdiff-windows-x86_64.exe` on Windows.

Open the app, set **Project directory** to this repository root. `objdiff.json` is generated by `configure.py` (`custom_make: ninja`, `build_base` / `build_target` on). The sidebar lists split units; renamed functions show the label from `names.txt`. Saving a `.c` / `.cpp` / `.h` rebuilds automatically.

## Readable names (`names.txt`)

Instruction bytes do not contain C++ names. objdiff still pairs a function only when the COFF symbol on the rebuilt object is the same string as on the split object, including every call reloc.

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
| `orig/spider.exe` | Matching target |
| `orig/cards.dll` | Matching target (English XP RTM cards library) |
| `orig/toolchain/` | User-supplied `cl.exe` / `link.exe` / headers / `libcmt.lib` |
| `orig/XPSP3`, `TABLET`, `TR_RTM` | Reference binaries only |
| `config/XPSP1/spider/`, `config/XPSP1/cards/` | Per-module `config.yml`, `splits.txt`, `symbols.txt`, `names.txt`, `units.json` |
| `src/mac/` | macOS port (SDL3, Metal, AppKit). See [src/mac/README.md](src/mac/README.md) |
| `src/spider/`, `include/spider/` | Spider decompiled C / C++ (`game_api.h` holds shared declarations) |
| `src/cards/`, `include/cards/` | cards.dll C, `.def`, and `cards.rc` |
| `src/spider/spider.rc` | Resource script: menu, dialogs, strings, accelerators, version info |
| `build/XPSP1/spider/assets/`, `build/XPSP1/cards/assets/` | Media extracted from each original by `configure.py` |
| `tools/msvc.py` | Runs `cl` / `link` / `lib` / `rc` / `cvtres`. Native on Windows; Wine via `wine_msvc.sh` on macOS and Linux |
| `tools/wine_msvc.sh` | Wine wrapper (project `WINEPREFIX`, `z:` path rewrite) |
| `configure.py` | Writes `build.ninja` + `objdiff.json` |

## Resources

`python3 configure.py` writes every media resource of each original into `build/XPSP1/<module>/assets/` as a normal file (`bitmaps/*.bmp`, `icons/*.ico`, `sounds/*.wav`, `manifest/1.manifest`). They are copyrighted, so they stay out of the repository. `ninja` compiles that module's `.rc` with the toolchain's `rc.exe` (`/i` the assets dir), converts it with `cvtres`, and links it. The rebuilt `.rsrc` section is byte-identical to the original's once data addresses are taken relative to the section:

```sh
python3 tools/cmp_rsrc.py orig/spider.exe build/XPSP1/spider/spider.exe   # RSRC MATCH
python3 tools/cmp_rsrc.py orig/cards.dll build/XPSP1/cards/cards.dll
```

`rc` writes resource data in statement order (string tables always last), so the order of statements in the `.rc` is part of the match. Each script sets `#pragma code_page(1252)` so `\251` and `\256` widen to the copyright and registered-trademark characters even when the host ANSI code page is UTF-8. `python3 tools/extract_assets.py orig/cards.dll build/XPSP1/cards/assets --rc src/cards/cards.rc` regenerates the script from the original.

## cards.dll

Gold is one C translation unit (`src/cards/cards.c`), compiled `/O1 /TC /Zl` with no `/DUNICODE`, linked `/NODEFAULTLIB /ENTRY:DllMain@12` against a `cards.exp` that `lib /DEF:src/cards/cards.def` builds from the objects. Exports: `WEP`, `cdtAnimate`, `cdtDraw`, `cdtDrawExt`, `cdtInit`, `cdtTerm`. Like spider, its image check is `ninja check_cards`.

CRT is not decompiled: spider links gold's single-threaded `libc.lib` from the toolchain, and `tools/crt_ident.py` names each `crt_*` unit after the library symbol it matches.

## Compiler flags

Spider:

```
cl  /W3 /wd4234 /ML /GR- /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS /DNDEBUG /O1
link /MACHINE:I386 /SUBSYSTEM:WINDOWS,4.0 /OSVERSION:5.1 /VERSION:5.1
     /BASE:0x01000000 /FIXED /RELEASE /INCREMENTAL:NO /OPT:REF /OPT:ICF
     /MERGE:.rdata=.text /FILEALIGN:0x200 /STACK:0x40000 /SECTION:.rsrc,R /TSAWARE
     /DEBUG /DEBUGTYPE:VC6 /PDB:build/XPSP1/spider/spider.pdb /PDBALTPATH:spider.pdb
     advapi32.lib kernel32.lib gdi32.lib user32.lib shell32.lib winmm.lib
     comctl32.lib htmlhelp.lib libc.lib
```

cards.dll:

```
cl  /W3 /wd4234 /TC /Zl /GR- /DWIN32 /D_WINDOWS /DNDEBUG /O1
lib /MACHINE:I386 /DEF:src/cards/cards.def /OUT:cards.lib cards_res.obj cards.obj
link /DLL /MACHINE:I386 /SUBSYSTEM:WINDOWS,4.0 /OSVERSION:5.1 /VERSION:5.1
     /BASE:0x6FC10000 /NODEFAULTLIB /ENTRY:DllMain@12 /RELEASE /INCREMENTAL:NO
     /MERGE:.rdata=.text /FILEALIGN:0x200 /STACK:0x40000 /SECTION:.rsrc,R /OPT:REF
     /DEBUG /DEBUGTYPE:VC6 /PDB:build/XPSP1/cards/cards.pdb /PDBALTPATH:cards.pdb
     user32.lib gdi32.lib cards.exp cards_res.obj cards.obj
```

`/O1` is locked by a 100% objdiff match on spider `fn_01007836` and by reloc-matched cards helpers (`DllMain`, `WEP`, `delete_if`, `cdtDraw`). No `/GS`, `/hotpatch`, or `/SAFESEH` (those are the SP2/SP3 / VC7.1 additions).
