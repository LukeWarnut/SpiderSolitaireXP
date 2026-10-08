#!/usr/bin/env bash
# Run a 32-bit MSVC 7.0 tool (cl, link, rc, cvtres, ml) under Wine.
# Rewrites Unix absolute paths to z: paths so cl.exe does not treat /Users/... as a switch.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WINEPREFIX="${WINEPREFIX:-$ROOT/tools/wineprefix}"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"

TOOLCHAIN="${MSVC_TOOLCHAIN:-$ROOT/orig/toolchain}"

find_exe() {
  local name="$1"
  local candidates=(
    "$TOOLCHAIN/bin/$name"
    "$TOOLCHAIN/BIN/$name"
    "$TOOLCHAIN/$name"
    "$TOOLCHAIN/VC7/bin/$name"
    "$TOOLCHAIN/VC7/BIN/$name"
    "$TOOLCHAIN/bin/x86/$name"
  )
  local p
  for p in "${candidates[@]}"; do
    if [[ -f "$p" ]]; then
      printf '%s\n' "$p"
      return 0
    fi
  done
  return 1
}

to_wine_path() {
  local p="$1"
  if [[ "$p" == /* ]]; then
    printf 'z:%s' "$p"
  else
    printf '%s' "$p"
  fi
}

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <cl|link|rc|cvtres|ml|...> [args...]" >&2
  exit 2
fi

TOOL_NAME="$1"
shift

if ! TOOL_EXE="$(find_exe "$TOOL_NAME.exe" || find_exe "$TOOL_NAME")"; then
  echo "error: $TOOL_NAME.exe not found under $TOOLCHAIN" >&2
  echo "See orig/README.md for the expected VC7.0 13.00.9178 layout." >&2
  exit 1
fi

INCLUDE_DIR=""
for d in "$ROOT/include" "$TOOLCHAIN/include" "$TOOLCHAIN/INCLUDE" \
         "$TOOLCHAIN/inc" "$TOOLCHAIN/inc/wxp" "$TOOLCHAIN/inc/crt" \
         "$TOOLCHAIN/inc/crt/sys"; do
  if [[ -d "$d" ]]; then
    if [[ -z "$INCLUDE_DIR" ]]; then
      INCLUDE_DIR="$(to_wine_path "$d")"
    else
      INCLUDE_DIR="$INCLUDE_DIR;$(to_wine_path "$d")"
    fi
  fi
done

LIB_DIR=""
for d in "$TOOLCHAIN/lib" "$TOOLCHAIN/LIB" "$TOOLCHAIN/lib/wnet/i386" "$TOOLCHAIN/lib/wxp/i386"; do
  if [[ -d "$d" ]]; then
    if [[ -z "$LIB_DIR" ]]; then
      LIB_DIR="$(to_wine_path "$d")"
    else
      LIB_DIR="$LIB_DIR;$(to_wine_path "$d")"
    fi
  fi
done

BIN_DIR="$(dirname "$TOOL_EXE")"
export INCLUDE="${INCLUDE:-$INCLUDE_DIR}"
export LIB="${LIB:-$LIB_DIR}"
# cl.exe loads c2.dll / c1.dll from PATH (same directory as cl).
export PATH="$BIN_DIR:$PATH"

WINE_BIN="${WINE:-wine}"

# /nologo is not a path. dirname /nologo is /, which exists, so existence
# checks alone would rewrite every MSVC /flag into z:/flag.
rewrite_unix_abs() {
  local p="$1"
  if [[ "$p" == /*/* ]]; then
    to_wine_path "$p"
    return 0
  fi
  return 1
}

rewrite_arg() {
  local a="$1"

  if [[ "$a" == @/* ]]; then
    printf '@%s' "$(to_wine_path "${a:1}")"
    return
  fi

  # /I/abs/path  /Fo/abs/path  /Fe /Fd /Fa /Fl /FR /FI /Fp
  if [[ "$a" =~ ^([/-][Ii])(/.+)$ ]]; then
    local rest="${BASH_REMATCH[2]}"
    if rewritten="$(rewrite_unix_abs "$rest")"; then
      printf '%s%s' "${BASH_REMATCH[1]}" "$rewritten"
      return
    fi
  fi
  if [[ "$a" =~ ^([/-][Ff][oOeEdDaAlLrRiI])(/.+)$ ]]; then
    local rest="${BASH_REMATCH[2]}"
    if rewritten="$(rewrite_unix_abs "$rest")"; then
      printf '%s%s' "${BASH_REMATCH[1]}" "$rewritten"
      return
    fi
  fi
  # /OUT:/abs  /PDB:  /MAP:  /DEF:  /IMPLIB:  /LIBPATH:
  if [[ "$a" =~ ^([/-][A-Za-z][A-Za-z0-9]*:)(/.+)$ ]]; then
    local rest="${BASH_REMATCH[2]}"
    if rewritten="$(rewrite_unix_abs "$rest")"; then
      printf '%s%s' "${BASH_REMATCH[1]}" "$rewritten"
      return
    fi
  fi

  # Bare Unix absolute path (/Users/..., /tmp/..., not /nologo /W3 /c)
  if rewritten="$(rewrite_unix_abs "$a")"; then
    if [[ -e "$a" || -e "$(dirname "$a")" ]]; then
      printf '%s' "$rewritten"
      return
    fi
  fi

  printf '%s' "$a"
}

ARGS=()
for a in "$@"; do
  ARGS+=("$(rewrite_arg "$a")")
done

mkdir -p "$WINEPREFIX"
cd "$ROOT"
if ((${#ARGS[@]})); then
  exec "$WINE_BIN" "$(to_wine_path "$TOOL_EXE")" "${ARGS[@]}"
else
  exec "$WINE_BIN" "$(to_wine_path "$TOOL_EXE")"
fi
