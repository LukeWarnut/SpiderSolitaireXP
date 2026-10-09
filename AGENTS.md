# MSVC 7.0 decomp iteration

English XP SP1 `spider.exe` and `cards.dll` are rebuilt with VC7.0 `cl` 13.00.9178 under Wine (`tools/wine_msvc.sh`). Each binary is a module under a version-first layout: `config/XPSP1/<module>/`, `src/<module>/`, `include/<module>/`, `build/XPSP1/<module>/`, gold at `orig/XPSP1/<binary>`. Instruction bytes are compared with `tools/cmp_fn.py` / `tools/cmp_reloc.py` (`--module spider|cards`, or inferred from the path); COFF symbols are compared by objdiff. A unit is not done until both are clean. Do not mark `complete` in that module's `units.json` before that.

Spider flags (from `tools/modules.py`): `/O1 /ML /GR- /DUNICODE /D_UNICODE /DNDEBUG`. No RTTI, no exceptions. Link is `/FIXED /BASE:0x01000000` plus the single-threaded `libc.lib` (`/ML` versus `/MT` changes no game code). `ninja spider` / `ninja cards`.

## Loop

Compare one unit. Wine `cl` is slow.

```sh
python3 tools/cmp_fn.py src/spider/game/GameWin/deal.cpp /tmp/out.obj 0x0100XXXX 0xSIZE
python3 tools/cmp_reloc.py build/XPSP1/cards/src/cards.obj 0x6FC11086 0xf _DllMain@12
python3 configure.py && ninja spider
```

Game sources are named for what they define: `src/spider/game/<Class>/<method>.cpp` (`ctor.cpp` / `dtor.cpp` for constructors and destructors), with free functions in `app/`, `dialogs/`, and `vec/`. Each file is still one dtk unit and one translation unit; `config/XPSP1/spider/units.json` maps the unit name (`fn_<address>.c`, which also names the object `build/XPSP1/spider/src/fn_<address>.obj`) to its `source`. Do not merge spider units into one TU: callee bodies become visible and `/O1` inlines them or keeps registers live across calls, which changed 20 of 141 functions when tried. `cards.dll` is the opposite: gold is one C translation unit, so `src/cards/cards.c` stays one unit.

`cmp_fn` masks `call`/`jmp` rel32, `push imm32`, and some absolute addresses, then prints the first masked mismatch. It does not mask `jcc` rel8/rel32. An extra three bytes in an early-out (`mov ecx, [esi+8]` before a second thiscall on `col`) shifts every later displacement, so the first hit is often `jge`/`je` and the masked percent looks like a failed body. Disassemble from a later unique instruction (`push ebx`, `push 0x28` / `pop edi`) before rewriting the main path.

`cmp_fn` uses the first COFF `.text` section. An `inline` helper in the same TU (the `any_empty` COMDAT) is that first section; the function under test is a later `.text`. Compare the largest section, or keep the helper out of line.

A masked match can still be wrong on the relocation name; objdiff checks that. After a declaration change, rebuild and run `python3 configure.py` so `tools/sync_symbols.py` copies the COFF symbol into `config/XPSP1/<module>/symbols.txt`. `names.txt` is only the objdiff label.

Ghidra (`orig/XPSP1/spider.exe_ghidra.c`) is a control-flow guide. Register choice, SIB base, and “globals” that are really fields of the object at `0x01010FC8` come from the bytes (capstone / `cmp_fn`), not from Ghidra’s names.

## cards.dll

English XP RTM `5.1.2600.0`, base `0x6FC10000`, one C file, `/O1 /TC /Zl`, no `/DUNICODE` (`LoadBitmapA`). Link `/DLL /BASE:0x6FC10000 /NODEFAULTLIB /ENTRY:DllMain@12 /DEF:src/cards/cards.def` plus `user32.lib gdi32.lib`. Internal helpers that gold calls with a full stdcall stack must not be `static` (that lets `/O1` pass args in registers and breaks `save_corners` / `load_back`). `load_face` is static cdecl with the card in `eax`. `cdtTerm` is cdecl (`ret`, not `ret 0`).

`load_face` matched only after the hand-tuned locals came out. Gold's eviction search (`mov eax, [g_lru_pos]` / loop / store `g_lru_pos` only if the loop ran) is `while (!g_face_bmps[g_lru_pos]) g_lru_pos = ...;` on the global, which `/O1` promotes to a register. A local `n` copied from `g_lru_pos` gives the same loop but moves the card from `esi` to `ebx` and recolors the whole function. Likewise `id = a + b` with `MAKEINTRESOURCEA(id + 1)` gives gold's `add edx, esi` / `inc edx`; folding the `+ 1` into `id` gives `lea`.

The cards globals are file-`static`. With an extern `int g_face_dx`, `cdtInit`'s early-out loads the global before `pdx`; static (address never taken, so `/O1` knows no pointer aliases it) gives gold's `pdx`-first order and changes nothing else.

The cards global names are load-bearing; do not rename them. MSVC places statics in `.bss` (and the linker places commons) in the order of a hash of the name, not declaration order; names in the same bucket fall back to declaration order. Each name's position is independent of the other names, so one compile of a file with hundreds of `static int` candidates (all referenced from one function) ranks them all. Pick one name per variable whose ranks increase in gold's address order and confirm in the real file. That produced `g_face_dy`, `g_face_dx`, `g_draw_bmp`, `g_n_loaded`, `g_face_bmps`, `g_hbmH`, `g_hbmCover`, `g_hbmX`, `g_hbmNaught`, `g_anim_id`, `g_init_cnt`, `g_lru_pos`, `g_hinst_dll` at `0x6FC12000`–`0x6FC12100`, with no padding before the 8-aligned array. A rename has to change `symbols.txt` too.

`cmp_rsrc orig/XPSP1/cards.dll build/XPSP1/cards/cards.dll` prints `RSRC MATCH`.

## Per-module report and image check

Each module has its own objdiff project (`build/XPSP1/<module>/objdiff.json`, paths relative to that directory), report (`ninja report_<module>` → `build/XPSP1/<module>/report.json`), and image check (`ninja check_<module>`). `harvest_literals.py --module <m>` reads that module's report. Both gold images are bound by `bind.exe`, so there is no SHA-1 target; `tools/cmp_image.py <gold> <ours>` (or `--module`) normalizes bind data, checksum, and link/debug timestamps on both sides and requires every other byte to match. `check_cards` passes. `cmp_fn` takes the largest `.text` COMDAT; for a per-function section use `cmp_reloc` with the symbol.

## Image-level link settings

`check_cards` is an `IMAGE MATCH`. What it took, all in `tools/modules.py` / `tools/project.py`:

- `/MERGE:.rdata=.text /FILEALIGN:0x200 /STACK:0x40000 /SECTION:.rsrc,R` (cvtres marks `.rsrc` writable; gold's is read-only).
- `/DEBUG` turns `/OPT:REF` off, so pass `/OPT:REF` explicitly or unreferenced code comes back.
- Gold's CodeView record is `NB10` naming only `<module>.pdb`. This link 7.00 writes `RSDS` with the full path unless given the undocumented `/DEBUGTYPE:VC6` (loads `mspdb60.dll`) and `/PDBALTPATH:<module>.pdb`. A stale PDB in the other format is `LNK1207`; delete it. Keep `/PDB:` inside `build/`.
- The Rich header (compared, not normalized) lists tool entries in link-input order. Gold's order is the export record, cvtres, then C: build `cards.exp` with `lib /DEF` from the objects and link `cards.exp cards_res.obj cards.obj` (`res_first`).
- Import descriptors follow import-library order on the command line (spider: `advapi32 kernel32 gdi32 user32 shell32 winmm comctl32`).

Spider with the same settings, `/TSAWARE`, and `libc.lib` gets gold's file size, section table, import set and DLL order, debug record, and `.rsrc`. What remains is translation-unit structure: the Rich header counts 8 C++ objects in gold versus our 127, and object order decides code placement (inline COMDATs, literals), the CRT pull order from `libc.lib`, and the thunk order inside each DLL's imports. No link switch reproduces those.

## One declaration, one mangled name

Declare each spider function once in `include/spider/game_api.h` and call that declaration everywhere. The call relocation must name the callee’s symbol. Cards declarations live in `include/cards/cards.h`.

| Gold | Declaration | COFF |
| --- | --- | --- |
| thiscall member | `void Class::fn(...)` | `?fn@Class@@QAE...` |
| stdcall | `extern "C" void __stdcall fn(...)` | `_fn@N` |
| fastcall | `extern "C" void __fastcall fn(...)` | `@fn@N` |
| cdecl | `extern "C" void __cdecl fn(...)` | `_fn` |

A member and an `extern "C"` function with the same source name are different symbols. `draw_card` is thiscall `GameBoard::draw_card` even though it never reads `this`: every gold caller sets `ecx` before the call, and a stdcall declaration drops that `mov ecx, esi`. `HtmlHelpW` (formerly `notify_cmd`) is `_HtmlHelpW@16` from `htmlhelp.lib`. The reverse also holds: if no gold caller sets `ecx` and the callee never reads it, declare it stdcall (`vec_normalize`, `vec_length_sq`). A member call on a dummy or uninitialized pointer still emits a `mov ecx`.

Two non-inline definitions of the same member are LNK2005, even when the bodies are identical. `/O1` does not emit them as COMDATs.

## `/O1` and `this` in `ecx`

A thiscall receives `this` in `ecx`. `/O1` keeps `ecx` live across the call only when the callee body is visible in that translation unit. Pattern that both matches and links:

```cpp
inline int LayoutBox::card_x(int i) { /* body */ }

#pragma auto_inline(off)
void LayoutBox::fill_rect(RECT *out)
{
    out->left = card_x(n);   /* gold CALL, ecx still this */
}
#pragma auto_inline(on)
```

`inline` makes the out-of-line copy a COMDAT, so the linker merges the copy in the callee’s unit with the copy in the caller’s unit. `auto_inline(off)` stops `/O1` from expanding the call. VC7 has no reliable `__forceinline` for this; do not depend on it.

An unreferenced `inline` function is dropped, and the unit’s `.text` becomes empty. A non-const global of external linkage that takes the address forces the copy out:

```cpp
int (LayoutBox::*k_card_x)(int) = &LayoutBox::card_x;
```

A `const` member-pointer is internal and `/O1` deletes it, along with the function.

That pattern keeps `this` live so the next thiscall can be `mov ecx, esi`. It does not make a CALL on a different object leave `ecx` as that object. Gold `any_empty` only uses `eax`/`edx`, so `alert` can run with leftover `ecx = col`. `inline` plus `#pragma auto_inline(off)` still treats the CALL as clobbering `ecx`, and CSE of `col` into `edi` steals the loop IV. A non-inline definition of the callee in the same TU (before or after the caller) does preserve `ecx`. That second strong symbol is LNK2005 if the callee's unit is also linked. Set `"link": false` on the callee unit so objdiff still builds it and the caller's object is the copy in the image.

## Register choice

`/O1` picks registers from assignment order and from how many live temps exist. Named C identifiers (`int ebx`, `int ebp`) do not bind. These are the levers that actually move a diff:

- An extra named temp steals `esi` / `edi`. `this` often wants to stay in `edi` (or `ebx`). Delete temps before adding casts.
- Frameless thiscall with three extra callee-saved values is usually `ebx` = zeroed loop counter, `ebp` = spilled snapshot reused as the working temp, `edi` = `push imm8` / `pop` stride. Pin the counter to `ebx` by using it in the first `if` (`if (use_fx == pile)` with `pile = 0`, `InvalidateRect(..., pile)`, `GetModuleHandleW(0)` while `ebx` is 0). If the snapshot load is delayed until after that `if`, the stride takes `ebp` and the snapshot takes `edi`.
- Gold reuses one register per path. One C local for `ebp` (saved `level`, then `idx`, then `last` on the anim path) plus a branch-local `last` that is allowed to overwrite the `edi` stride (restore from a stack spill) matches that. Separate `idx`/`last`/`saved` temps recolor the whole function.
- `lea eax, [ecx+disp]` is “address of a field”. `add ecx, imm` is “this plus a constant”. Writing `eax = &suit_src` versus `eax = (int *)((char *)this + 0xF58)` changes which one you get.
- Argument order is the source order, but the register for each argument is not. Gold `mov edx, [esp+8]` versus our `mov eax, [esp+8]` is the same load into a different register. Reordering the loads and naming the gold register (`int edx = n`) sometimes flips it; a pointer typed as the field’s struct (`ColumnInner *eax = inner`) sometimes does too.
- Eval order of a comparison is not source order when both sides are calls. Gold often evaluates the last-assigned operand first. Matching that can move `this` into another register, which is worse than the original diff. Stop when the next tweak trades one mismatch for a larger one. Swapping the operands of `!=` usually does not change the call order at all. What does: assign both calls to locals in gold's order, reusing locals the function already has. `collect_moves` matched with `fa = rank_of(src, last); fb = rank_of(src, prev); if (fa != fb)`, reusing the `fa`/`fb` of the next check. A new `ra` temp moved `this` from `edi` to `esi`. The written order of `fa != fb` versus `fb != fa` then picks `cmp esi, eax` versus `cmp eax, esi`.
- Before chasing a register permutation with temps and casts, rewrite the function in its plainest form (globals used directly, one local per value gold actually keeps, natural `while`/`if` shapes). Hand-split steps (`id = x >> 2; id %= 13; ...`) and pointer locals that mirror gold's registers usually pin the wrong coloring.
- Permuting independent statements is cheap to brute-force (generate every order and compile them in parallel). It fixed `on_left_down`. It does not move a constant-zero register (`xor edi, edi` in `save_game`, `xor eax, eax` in `move_run`); no reorder, `buf = 0` (a dead store, removed), or condition shape tried so far reproduces those.
- A float field that is assigned and then read back can need the field as the source of truth. `run_fx` keeps two stack copies of `life` because the source is `it->life = age / it->pad3c; life = it->life;` with the final test on `it->life`. Assigning a local first swaps two slots at the top of the function.
- `lea edi, [ebx+0xc]` before `mov esi, eax` in a `Vec3` copy from a call result comes out right when nothing sits between the call and the copy. When an FP load is scheduled in between (`fld 0.1` in `burst_fx`), ours can emit `mov esi` first. In `run_fx` that order was a side effect of `volatile` float homes elsewhere in the block; with them gone the `fdiv` case matches.
- An x87 value that gold stores to a lone field of an otherwise unused stack `Vec3` is a component of a real `Vec3` temp, not a spill. `run_fx` writes `k.z` to `[ebp+0xc]` and `k.x` to `[ebp+0x1c]`; it matched (frame `0xb8`, every slot) with `vset(&k, acc * 1.8)`, `vset(&gr, 0, 6.8, 0)`, `vset(&k2, k.x - gr.x, k.y - gr.y, k.z - gr.z)`. The `- 0.0f` subtracts fold away. Writing `k.y - 6.8f` directly, value-returning inline helpers, or `volatile` homes all give the wrong frame.
- `run_fx`'s last diff: gold reloads `fx` (`mov esi, [ebp+0x7c]`) after the `it->life` stores, ours right after the `movsd` copy. Nothing changed it: the shape of the `life` / `exp` statements, `exp` versus the float overload, an inline `fade`, alias-weakening pointers, taking `&fx`, a local copy of `fx`, loop shape, or declaration order.
- An invariant `face + 1` in a loop comparison becomes `lea edi, [eax+1]`. Writing `face = face_of(...) + 1` instead gives `mov edi, eax` / `inc edi` (`full_suit`).
- SIB base is the pointer, not the integer offset. `*(int *)(src_off + (char *)p)` compiles as `[reg_p+reg_off]`. Gold `mov ebx, [ecx+eax]` wants the offset as the base (`modrm` 01). Swapping the addends in C does not flip it; MSVC still bases the address on the pointer. Exception: when that offset is already the `edi` loop IV, `*(int *)(off + (char *)p)` can emit `[edi+reg_p]` (offset as base). Get `edi` right first; do not add a `coli` temp to chase the SIB.
- Nested `EnableMenuItem(GetSubMenu(menu, 0), id, 1)` is how gold leaves `push 1` / `push id` under `GetSubMenu`’s args. Separate locals for the submenu miss that interleave.
- A loop-invariant stack value such as `delta = -8 - this` plus a walking pointer is `/O1` strength reduction, not source. `fit_piles` matches with plain `extras[pile]` and `((PileTable *)inner)->counts[pile]` in a `for` loop; the hand-written pointer walk put `this` in `esi`.
- An import held in a register (`mov edi, [__imp__GetMessageW]` / `call edi`) for a call that appears once means the source called it twice and the compiler merged the tails. `WinMain`’s message pump is written out in both the no-animation branch and the `PeekMessageW` branch; one shared pump gives a different loop layout and different stack slots.

## Constants and intrinsics

- Signed `n % 2` is `cdq` / `idiv`, not `and`.
- Unsigned `DWORD` to `float` is `fild` / `jns` / add 2^32. Cast through `DWORD` (the `timeGetTime` stamp).
- `#pragma intrinsic(exp)` emits `__CIexp`. A plain `exp` / `expf` call is `_exp`.
- A float passed to a stdcall is `push ecx` / `fstp dword ptr [esp]`.
- A `Vec3` copy is three `movsd`s when the local is a real `Vec3`, not three scalar stores.
- `#pragma intrinsic(memset)` and `memcpy` become `rep stosd` / `rep movsd`. Gold’s `pop_row` (`lea ecx, [edx+edx*4]; shl ecx, 2; shr ecx, 2; rep movsd`) is a loop of 20-byte struct assignments `rows[i] = rows[i + 1]`, not `memcpy`.
- `wchar_t buf[0x14]` is `sub esp, 0x2C` once alignment is counted. `[0x16]` became `sub esp, 0x30`. Match the gold `sub esp` immediate, not the C array bound you first guessed.
- Small immediates are `push imm8` / `pop reg` (`off = 0x28` → `6A 28 5F`). An independent `saved += 10` can sit between the `push` and the `pop`. A store of `off` to a live stack spill after the add (`spill_off = off`) keeps the pair adjacent: `pop edi` / `add ebp, 0xa` / `mov [esp+0x14], edi` / `mov [esp+0x20], ebp`.

## Frames

Gold sometimes uses a biased frame: `lea ebp, [esp-0x74]` / `sub esp, 0xB8`, and `add ebp, 0x78` / `leave` on the way out (`run_fx`, `paint_hdc`, `load_settings`). This compiler emits it on its own once the real body and its locals are written out; the stubs simply had too few. It also appears where gold has none (a block-scoped `INITCOMMONCONTROLSEX` plus `memset` in `WinMain`), so check `55 8B EC` versus `8D 6C 24` first when every `[ebp+disp]` is off by a constant.

Frameless `sub esp, N` is the sum of a `RECT` plus spill homes that are actually read later. Unused locals are deleted. Deal’s gold `0x24` is one `RECT` (16) plus off, saved, y, `b`, and `hdc` (20). A 4-byte miss on `sub esp` then shifts every `[esp+disp]` and looks like a large mismatch.

Stack slots for address-taken locals do not follow declaration order or names. Locals declared inside two disjoint branches can share a slot (`on_lbutton_up`: the snap-back `RECT` and the redraw branch's intersect destination are both `[ebp-0x38]`), so a function-scope `RECT` used in both branches permutes the whole frame.

`cmp_fn` treats opcode `0x68` as `push imm32` and masks the next four bytes. `sub esp, 0x68` is therefore a false first-diff. It does the same with `0xE8` inside `lea eax, [ebp-0x18]` (`8D 45 E8`), so a same-size body with `[ebp-0x18]` locals can report a few masked mismatches that are not real. `python3 tools/cmp_reloc.py /tmp/out.obj 0x0100XXXX SIZE [symbol]` masks only at the object's own relocation offsets and picks the section by symbol; `RELOC MATCH` there plus 100% in objdiff is done (`on_right_up`, `on_command` were both false `cmp_fn` diffs).

## False splits

A unit of 3–10 bytes that is only `push ebp` / `mov ebp, esp` / `sub esp, N` is the prologue of the next unit. Gold falls through. Two C functions will not. Merge the ranges in `config/XPSP1/spider/splits.txt` and `units.json`, then re-run `python3 configure.py`. A 3-byte unit that is already a complete instruction (`0x01006611` is `sub esp, 0x24`) is still that prologue; merge it with the rest of the body (`0x01006611`–`0x01006821` is `GameWin::deal`).

`0x01006821` (10 bytes) plus `0x0100682B` is one `GameWin` command handler. `0x010057F8` (3 bytes, `push ecx; push ebx; push ebp`) is the start of new-game, not `Rng::set_seed`. The real seeder is `fn_01008A8A` (`srand`).

The window procedure’s registered entry is the 10 bytes at `0x01006CE3` (`mov ecx, 0x01010FC8` / `jmp wnd_proc`). It is its own unit, `wnd_proc_thunk` (stdcall, `app/wnd_proc_thunk.cpp`); `/O1` compiles the one-line forwarding wrapper to exactly those bytes. A gold unit that ends in a tail-call thunk like this is a false merge: split the thunk off.

## Link errors that look like match work

- LNK2005: the same member is defined in two TUs without `inline`.
- LNK2019 `_heap_free`: gold’s scalar deleting destructor calls cdecl `_free`. Map the call to `free`.
- LNK2019 `_fn_01008A94` / `_fn_01008A8A`: gold’s CRT `rand` / `srand`. Resolve them with `extern "C"` wrappers; do not give them a C++ mangling.
- LNK2019 on a member you defined as `extern "C"` (or the reverse): the caller was compiled against the other declaration.

`/OPT:REF` drops import DLLs that stub `WinMain` never touches. That is a warning, not a failed match.

## Library code

`python3 tools/crt_ident.py [lib] [unit-regex]` compares `crt_fn_*` units with every code symbol in a static library (default `libc.lib`), masking only the member's relocations; `EXACT` means the whole unit up to `int3` padding. Name the gold symbol after the library symbol, rename the unit `crt_<name>.c`, and mark it complete. Labels inside assembler objects (`start` in `exp_pentium4.obj`, `cintrinexit` in `87cdisp.obj`) begin mid-section; slide over the section to find them.

Gold's CRT is single-threaded `libc.lib`: `srand`/`rand` keep state in a plain global (10-byte `srand`); `libcmt.lib` has the 13-byte `_getptd` version. `_free` at `0x0100988D` is a small-block-heap build that no library under `orig/toolchain` contains; it is named but not complete.

`0x0100ECEA` (`GetRegisteredLocation`) and `0x0100ED49` (`HtmlHelpW`) are `htmlhelp.lib`. `0x0100EDE2` (`$E1`/`$E2`) is the compiler's dynamic initializer for `GameBoard g_board;` in `src/spider/app/g_board.cpp`: `GameBoard::GameBoard` (`fn_010053E8`), then `atexit` of a thunk that calls `~GameBoard` (`fn_01004732`). `$E1`/`$E2` are static and live in `.text$yc`/`.text$yd`; `cmp_reloc` takes `'_$E1'` as the symbol. The two `int3` after `$E2` belong to `import_data.c` (`0x0100EE02`–`0x0100FAF8`), which is import data, not code.

## Absolute addresses versus real globals

`*(T *)0x0101xxxx` is not codegen-equivalent to a global symbol. `/O1` picks different registers for it (`dlg_options` reloaded `G` into `eax`, 5 bytes short, until `G` became `extern "C" GameWin *g_opts`), and it does not CSE repeated loads (`WinMain`'s `wrect.right == CW_USEDEFAULT ? ... : ...` duplicates the compare unless `wrect` is a real symbol). When a register choice on a global load will not move, try a real symbol first.

## Game data

Game `.data` and `.bss` are defined in the unit that owns them, and that unit's split in `splits.txt` carries the range, so objdiff compares the data against our object:

| Range | Object | Unit |
| --- | --- | --- |
| `0x01010000`–`0x01010038` | `g_help_ids` (WinHelp context IDs) | `dlg_options` |
| `0x010107C0`–`0x01010FC0` | `g_strbuf` | `load_string` |
| `0x01010FC0`–`0x01010FC4` | `g_hinst` | `WinMain` |
| `0x01010FC8`–`0x01012000` | `g_board` (`GameBoard`, `align:8`) | `fn_0100EDE2` |
| `0x01012000`–`0x0101200C` | `g_opts`, `g_level_game`, `g_stats_game` | the three dialog procedures |

`0x01010038`–`0x010107C0` and `0x0101200C`–`0x010125B4` are `libc.lib` data (auto units, target only). Uninitialized ranges use `rename:.bss`; a unit can have one `.data` and one `.bss` range. Gold `.data` has raw size 0x800 only because of file alignment, so dtk truncates a `.bss` split that starts before `0x01010800` (`g_strbuf`). `tools/fix_bss.py` runs after the split and turns those all-zero pieces back into uninitialized sections of the full size. The 4 bytes at `0x01010FC4` are alignment padding before the 8-aligned board, not a variable.

The board is one object with two views: `extern "C" GameBoard g_board` is the constructed type, and `g_game` (`*(GameWin *)&g_board`) is the window-side view. A new `symbols.txt` name in gold `.data` adds relocations to every gold unit that touches it, so convert every source use at once and diff the report before and after.

## Dialog procedures and shared returns

MSVC keeps the last copy of a duplicated `return 0` tail. To keep the shared block at the first site, write `if (...) { return 0; } else { ... }`. Gold dialog procedures use an inner `switch (LOWORD(wp))` with `break` per button, `default: return FALSE;`, then `return TRUE;` after the inner switch and one `return FALSE;` after the outer switch.

## Resources

`configure.py` extracts each module's media into `build/XPSP1/<module>/assets/` (`tools/extract_assets.py`); the build runs `rc` on that module's `.rc` with `/i` that assets dir, then `cvtres`. `python3 tools/cmp_rsrc.py orig/XPSP1/spider.exe build/XPSP1/spider/spider.exe` prints `RSRC MATCH` when the section is byte-identical apart from its RVA (same for `cards.dll`). Statement order in the `.rc` is the data order in `.rsrc`; `rc` always emits `STRINGTABLE` last when the original has strings, and icon images are numbered in `ICON` statement order.

## Literals in gold rdata

dtk splits gold rdata as one `text_rdata` blob, so a gold `push text_rdata+0x378` cannot pair with our `push ??_C@_15...@` (`L"%d"`). Write the real literal (`L"%d"`, `0.01f`), not the absolute address, then run `python3 tools/harvest_literals.py --write` and rebuild. It pairs objdiff instructions and adds `symbols.txt` entries naming each gold address after the MSVC literal symbol, with a size (dtk drops the relocation for a sizeless symbol). Once one literal inside `text_rdata` is named, dtk emits later references in that range as bare immediates; the harvester pairs those too, so re-run until it adds nothing. Two gold addresses that want the same literal name are reported as conflicts and skipped.
