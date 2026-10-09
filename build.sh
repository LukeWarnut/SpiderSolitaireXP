#!/usr/bin/env bash
# Build Spider on macOS (or Linux). See "Building" in README.md.
#
#   ./build.sh                       spider and cards (the matching decompilation)
#   ./build.sh cards                 one target; any ninja target works
#   ./build.sh spider_mac            the macOS app, build/mac/Spider.app
#   ./build.sh clean [target...]     delete build output, keeping downloaded tools
#   ./build.sh --orig orig/XPSP3 -y  options before or after targets go to configure.py
#
# macOS ships bash 3.2: no associative arrays, and empty arrays are expanded
# with ${a[@]+"${a[@]}"} so `set -u` is not needed.
set -eo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

die() { echo "error: $*" >&2; exit 1; }

usage() {
  sed -n '4,8p' "$0" | sed 's/^# \{0,1\}//'
  cat <<'EOF'

Targets:
  spider, cards      link build/XPSP1/<module>/<binary> (default: both)
  spider_mac         macOS app (CMake + SDL3)
  spider_mac_test    macOS headless rule tests
  check, report      byte-compare with gold / objdiff report (also *_spider, *_cards)
  progress           objdiff report, then the progress summary
  configure          re-run configure.py (with the options last used)
  clean              delete build output, keeping build/tools; with targets, only theirs
EOF
}

BUILD_DIR="build"
ORIG_DIR=""
CLEAN=0
RECONFIGURE=0
CONF_OPTS=()
NINJA_TARGETS=()
MAC_TARGETS=()
PROGRESS=0

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    clean) CLEAN=1 ;;
    configure) RECONFIGURE=1 ;;
    progress) PROGRESS=1 ;;
    spider_mac|spider_mac_test) MAC_TARGETS+=("$1") ;;
    --orig|--build-dir|--dtk|--objdiff|--ninja|--wrapper|-v|--version)
      [ $# -ge 2 ] || die "$1 needs a value"
      CONF_OPTS+=("$1" "$2")
      [ "$1" = --orig ] && ORIG_DIR="$2"
      [ "$1" = --build-dir ] && BUILD_DIR="$2"
      shift ;;
    --orig=*) CONF_OPTS+=("$1"); ORIG_DIR="${1#--orig=}" ;;
    --build-dir=*) CONF_OPTS+=("$1"); BUILD_DIR="${1#--build-dir=}" ;;
    -*) CONF_OPTS+=("$1") ;;
    *) NINJA_TARGETS+=("$1") ;;
  esac
  shift
done

MODULE_DIR="$BUILD_DIR/XPSP1"
MAC_DIR="$BUILD_DIR/mac"

# ---- clean ------------------------------------------------------------------

if [ "$CLEAN" = 1 ]; then
  if [ ${#NINJA_TARGETS[@]} -eq 0 ] && [ ${#MAC_TARGETS[@]} -eq 0 ]; then
    echo "Removing $BUILD_DIR/ (keeping $BUILD_DIR/tools) and the generated ninja files"
    if [ -d "$BUILD_DIR" ]; then
      for entry in "$BUILD_DIR"/* "$BUILD_DIR"/.[!.]*; do
        [ -e "$entry" ] || continue
        [ "$(basename "$entry")" = tools ] && continue
        rm -rf "$entry"
      done
    fi
    rm -rf build.ninja .ninja_deps .ninja_log objdiff.json
  else
    for t in ${NINJA_TARGETS[@]+"${NINJA_TARGETS[@]}"} ${MAC_TARGETS[@]+"${MAC_TARGETS[@]}"}; do
      case "$t" in
        spider|cards) echo "Removing $MODULE_DIR/$t/"; rm -rf "${MODULE_DIR:?}/$t" ;;
        spider_mac|spider_mac_test) echo "Removing $MAC_DIR/"; rm -rf "$MAC_DIR" ;;
        *) die "clean takes spider, cards, or spider_mac (got $t)" ;;
      esac
    done
  fi
  exit 0
fi

# ---- tools ------------------------------------------------------------------

PYTHON="${PYTHON:-}"
if [ -z "$PYTHON" ]; then
  if command -v python3 >/dev/null 2>&1; then PYTHON=python3
  elif command -v python >/dev/null 2>&1; then PYTHON=python
  else die "Python 3 not found (brew install python)"; fi
fi

if [ ${#NINJA_TARGETS[@]} -eq 0 ] && [ ${#MAC_TARGETS[@]} -eq 0 ] \
   && [ "$PROGRESS" = 0 ] && [ "$RECONFIGURE" = 0 ]; then
  NINJA_TARGETS=(spider cards)
fi
[ "$PROGRESS" = 1 ] && NINJA_TARGETS+=(report)

# ---- matching decompilation (configure.py + ninja) -------------------------

saved_configure_args() {
  [ -f build.ninja ] || return 0
  sed -n 's/^configure_args = //p' build.ninja | head -n 1
}

if [ ${#NINJA_TARGETS[@]} -gt 0 ] || [ "$RECONFIGURE" = 1 ]; then
  command -v ninja >/dev/null 2>&1 || die "ninja not found (brew install ninja)"

  # configure.py extracts assets and writes build.ninja. Run it when that has
  # not happened yet, when options were given, or after a per-module clean.
  need_configure="$RECONFIGURE"
  [ -f build.ninja ] || need_configure=1
  [ ${#CONF_OPTS[@]} -gt 0 ] && need_configure=1
  for m in spider cards; do
    [ -d "$MODULE_DIR/$m/assets" ] || need_configure=1
  done

  if [ "$need_configure" = 1 ]; then
    if [ ${#CONF_OPTS[@]} -eq 0 ]; then
      # Keep the options of the last configure (--orig, --allow-nonmatching).
      read -r -a CONF_OPTS <<< "$(saved_configure_args)" || true
    fi
    if [ ${#NINJA_TARGETS[@]} -gt 0 ]; then
      "$PYTHON" tools/check_compiler.py
    fi
    "$PYTHON" configure.py ${CONF_OPTS[@]+"${CONF_OPTS[@]}"}
  fi

  if [ ${#NINJA_TARGETS[@]} -gt 0 ]; then
    ninja "${NINJA_TARGETS[@]}"
  fi
  if [ "$PROGRESS" = 1 ]; then
    "$PYTHON" configure.py progress
  fi
fi

# ---- macOS port (CMake) -----------------------------------------------------

if [ ${#MAC_TARGETS[@]} -gt 0 ]; then
  [ "$(uname -s)" = Darwin ] || die "spider_mac builds only on macOS"
  command -v cmake >/dev/null 2>&1 || die "cmake not found (brew install cmake sdl3 ffmpeg)"
  # Same lookup as configure.py: --orig first, then orig/. Passed every time so
  # CMake's cache does not keep an exe from an earlier --orig.
  spider_exe="$ROOT/orig/spider.exe"
  if [ -n "$ORIG_DIR" ]; then
    case "$ORIG_DIR" in /*) candidate="$ORIG_DIR/spider.exe" ;; *) candidate="$ROOT/$ORIG_DIR/spider.exe" ;; esac
    [ -f "$candidate" ] && spider_exe="$candidate"
  fi
  [ -f "$spider_exe" ] || die "no spider.exe at $spider_exe (see orig/README.md)"
  cmake -S src/mac -B "$MAC_DIR" "-DSPIDER_EXE=$spider_exe"
  for t in "${MAC_TARGETS[@]}"; do
    case "$t" in
      spider_mac)
        cmake --build "$MAC_DIR" --target spider
        echo "Built $MAC_DIR/Spider.app" ;;
      spider_mac_test)
        cmake --build "$MAC_DIR" --target spider_test
        ctest --test-dir "$MAC_DIR" --output-on-failure ;;
    esac
  done
fi
