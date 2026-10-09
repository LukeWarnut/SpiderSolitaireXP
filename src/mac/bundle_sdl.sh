#!/bin/sh
# Copy SDL3 into the app and rewrite the load command so the bundle does not
# refer to the machine it was built on. Usage: bundle_sdl.sh <libSDL3.dylib> <Spider.app>
set -eu

src=$(python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' "$1")
app=$2
name=$(basename "$src")
fw="$app/Contents/Frameworks"
exe="$app/Contents/MacOS/Spider"
new_id="@executable_path/../Frameworks/$name"

mkdir -p "$fw"
cp -f "$src" "$fw/$name"
codesign --remove-signature "$fw/$name" 2>/dev/null || true
codesign --remove-signature "$exe" 2>/dev/null || true

old_id=$(otool -D "$fw/$name" | awk 'NR==2 { print; exit }')
install_name_tool -id "$new_id" "$fw/$name"

# The executable records the dylib's install name, which may differ from the
# path cmake passed to the linker.
otool -L "$exe" | awk 'NR>1 { print $1 }' | while IFS= read -r dep; do
    case "$dep" in
        "$new_id") ;;
        *"$name") install_name_tool -change "$dep" "$new_id" "$exe" ;;
    esac
done
if [ "$old_id" != "$new_id" ]; then
    install_name_tool -change "$old_id" "$new_id" "$exe" 2>/dev/null || true
fi

otool -l "$exe" | awk '/cmd LC_RPATH/ { getline; getline; print $2 }' | while IFS= read -r rp; do
    case "$rp" in
        @*) ;;
        *) install_name_tool -delete_rpath "$rp" "$exe" ;;
    esac
done

codesign --force --sign - "$fw/$name"
codesign --force --sign - "$app"

bad=$(otool -L "$exe" | awk 'NR>1 { print $1 }' | grep -Ev '^(/System/|/usr/lib/|@)' || true)
if [ -n "$bad" ]; then
    echo "bundle_sdl: Spider still links a library outside the bundle:" >&2
    echo "$bad" >&2
    exit 1
fi
