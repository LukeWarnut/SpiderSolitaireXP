# Spider for the web (WASM)

A playable port of Windows XP Spider Solitaire that runs in a browser. The rules, scoring, undo, and save format come from the XP SP1 `spider.exe` decompilation in this repository — the same game core the macOS port uses (`src/mac/game/`). The C++ core is compiled to WebAssembly with Emscripten; the window, menus, dialogs, and rendering are HTML and a Canvas2D canvas. It does not try to match the original instruction bytes.

Saved games go to the browser's IndexedDB (an IDBFS mount of `spider.sav`), in the same integer layout as XP. Settings and statistics go to `localStorage` (`AnimDeal`, `SaveOnExit`, per-difficulty high scores, and the rest of the XP registry keys).

## Build

Install CMake and [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (Chocolatey's `emscripten` package is enough). On Windows, `.\build.ps1 spider_wasm` looks for `emcmake` on `PATH`, then `%LOCALAPPDATA%\emsdk`, and for `cmake` on `PATH` then `C:\Program Files\CMake`. If a new PowerShell window still cannot see `emcmake`, run `%LOCALAPPDATA%\emsdk\emsdk_env.ps1` once in that window (or open a fresh window after `emsdk activate --permanent`). Then:

```sh
# A spider.exe to take media from. Not committed.
cp /path/to/spider.exe orig/spider.exe

./build.sh spider_wasm          # .\build.ps1 spider_wasm on Windows
```

That produces `build/wasm/` with `index.html`, `spider.js`, `spider.wasm`, and the extracted `assets/`. Serve the directory over HTTP (browsers will not run a wasm module from `file://`):

```sh
python3 -m http.server -d build/wasm        # python on Windows
# then open http://localhost:8000
```

The build extracts bitmaps and sounds from `orig/spider.exe` at build time; those files are copyrighted, so they are not stored in git. The port does not need the exact XP SP1 build — point `--orig` at another directory as with the macOS port. `./build.sh clean spider_wasm` deletes `build/wasm`.

## Controls

| Action | Key |
|---|---|
| New game | F2 or Ctrl-N |
| Undo | Ctrl-Z |
| Deal a row | D, or click the stock at the lower right |
| Hint | M, or click the score box |
| Restart this deal | Game menu |
| Difficulty, statistics, options | F3, F4, F5 |
| Save / open | Game menu, or Ctrl-S / Ctrl-O |
| Help / about | F1 / Help menu |

## Layout

| Path | What it is |
|---|---|
| `src/wasm/host/main.cpp` | Emscripten entry point: bitmap fetch/decode, IDBFS mount, embind bindings |
| `src/wasm/host/wasm_host.cpp` | `Host` over the browser: clock, Web Audio, HTML dialogs, localStorage, IDBFS save |
| `src/wasm/web/` | The page: `index.html`, `main.js` (boot, input, rAF loop), `render.js` (Canvas2D), `ui.js` (menus/dialogs), `audio.js`, `style.css` |
| `src/wasm/CMakeLists.txt` | The Emscripten build |
| `src/mac/game/`, `src/mac/render/bmp.cpp` | The shared game core and BMP decoder |
| `src/spider/`, `src/cards/` | The XP matching decompilation. The WASM target does not compile it |

The modal dialogs (confirm, difficulty, statistics, options) are `Promise`s that the C++ `Host` awaits through `EM_ASYNC_JS`, so the module is linked with `-sASYNCIFY`. The win prompt, help, and about are non-blocking HTML overlays.
