# Spider for macOS

A playable port of Windows XP Spider Solitaire. The rules, scoring, undo, and save format come from the XP SP1 `spider.exe` decompilation in this repository. The Mac app is a normal 64-bit program: an SDL3 window, a Metal renderer, and AppKit menus and dialogs. It does not try to match the original instruction bytes.

Saved games go to `~/Library/Application Support/Spider/spider.sav`, in the same integer layout as XP. Settings and statistics go to the app's preferences (`AnimDeal`, `SaveOnExit`, window rect, per-difficulty high scores, and the rest of the XP registry keys).

## Build

Install the tools, then point CMake at `src/mac`:

```sh
brew install cmake sdl3 ffmpeg
# Xcode supplies clang and iconutil.

# English XP SP1 spider.exe (5.1.2600.1106). Not committed.
cp /path/to/spider.exe orig/XPSP1/spider.exe

cmake -S src/mac -B build/mac
cmake --build build/mac
open build/mac/Spider.app
```

The build extracts bitmaps, sounds, and icon 103 from `orig/XPSP1/spider.exe` into the app bundle. Those files are copyrighted, so they are not stored in git. Override the source exe with `-DSPIDER_EXE=/path/to/spider.exe` if it does not live at `orig/XPSP1/spider.exe`.

`ffmpeg` and `iconutil` are only used to turn icon 103 into `Spider.icns`. Without them the app still builds, with the default icon.

SDL3 is copied into `Spider.app/Contents/Frameworks` and the executable is rewritten to load that copy. The finished `.app` does not need Homebrew on the machine that runs it. The bundled SDL3 only runs on the macOS version it was built for (a current Homebrew SDL3 targets the OS it was installed on).

Headless rule tests, with no window and no assets:

```sh
cmake --build build/mac --target spider_test
ctest --test-dir build/mac
```

## Controls

| Action | Key |
|---|---|
| New game | F2 or Command-N |
| Undo | Command-Z |
| Deal a row | D, or click the stock at the lower right |
| Hint | M, or click the score box |
| Restart this deal | Game menu |
| Difficulty, statistics, options | F3, F4, F5 |
| Save / open | Command-S / Command-O |
| Help / about | F1 / Spider menu |
| Quit | Command-Q |
| Minimize | Escape |


## Layout

| Path | What it is |
|---|---|
| `src/mac/game/` | Rules, undo, deal, save, and the win-firework simulation |
| `src/mac/host/` | SDL3 event loop, audio, AppKit menu and dialogs, preferences |
| `src/mac/render/` | BMP loader and the Metal renderer |
| `src/mac/CMakeLists.txt` | The Mac build |
| `src/spider/`, `src/cards/` | The XP matching decompilation. The Mac target does not compile it |

`Spider --snapshot out.png {board,drag,hint,win}` writes one frame without opening a dialog. It is for checking the renderer, not for playing.
