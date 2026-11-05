#!/bin/sh
# Package the local native macOS build as a Finder-launchable Mercenaries.app.
# Run after make mercs (or use make app). Based on the mature MW2 bundle layout
# with a native SDL3 library, Mercenaries identity and no inherited release tag.
set -eu

mercs_root=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
mercs_app="$mercs_root/Mercenaries.app"
mercs_exe="$mercs_root/mercs"
mercs_stage=

mercs_fail() { printf '%s\n' "Mercenaries app: $*" >&2; exit 1; }
mercs_cleanup() { [ -z "$mercs_stage" ] || rm -rf -- "$mercs_stage"; }
trap mercs_cleanup EXIT
trap 'exit 1' HUP INT TERM

[ "$(uname -s)" = Darwin ] || mercs_fail "run make app on macOS after building mercs there."
[ -x "$mercs_exe" ] || mercs_fail "the local mercs executable is missing; run make mercs first."
for mercs_tool in otool install_name_tool codesign lipo awk; do
    command -v "$mercs_tool" >/dev/null 2>&1 || mercs_fail "required macOS tool not found: $mercs_tool"
done
otool -h "$mercs_exe" >/dev/null 2>&1 || mercs_fail "mercs must be a native macOS Mach-O executable."
# The application must use SDL_GPU/Metal, with no direct legacy GL imports.
if otool -L "$mercs_exe" | awk '/OpenGL\.framework|libGL[^/]*\.dylib|libEGL/ {found=1} END {exit !found}'; then
    mercs_fail "mercs still links OpenGL; run make clean and rebuild this source."
fi
mercs_archs=$(lipo -archs "$mercs_exe")
for mercs_font in LiberationSans-Bold.ttf README.md; do
    [ -f "$mercs_root/assets/fonts/$mercs_font" ] || mercs_fail "missing assets/fonts/$mercs_font"
done

mkdir -p "$mercs_root/build"
mercs_stage=$(mktemp -d "$mercs_root/build/mercs-app.XXXXXX")
mercs_bundle="$mercs_stage/Mercenaries.app"
mkdir -p "$mercs_bundle/Contents/MacOS" "$mercs_bundle/Contents/Resources/assets/fonts" "$mercs_bundle/Contents/Frameworks"
cp "$mercs_exe" "$mercs_bundle/Contents/MacOS/mercs"
chmod u+w "$mercs_bundle/Contents/MacOS/mercs"
cp "$mercs_root/assets/fonts/LiberationSans-Bold.ttf" "$mercs_root/assets/fonts/README.md" "$mercs_bundle/Contents/Resources/assets/fonts/"

# Return load-command paths without splitting paths containing spaces.
mercs_dependencies() {
    otool -L "$1" | awk '/^[ \t]/ {sub(/^[ \t]+/, ""); sub(/ \(compatibility version.*$/, ""); print}'
}
mercs_rpaths() {
    otool -l "$1" | awk '$1 == "cmd" {r = ($2 == "LC_RPATH")} r && $1 == "path" {sub(/^[ \t]*path /, ""); sub(/ \(offset.*$/, ""); print; r = 0}'
}
mercs_expand_path() {
    case "$1" in
        @loader_path) dirname "$2" ;;
        @executable_path) printf '%s\n' "$mercs_root" ;;
        @loader_path/*) printf '%s/%s\n' "$(dirname "$2")" "${1#@loader_path/}" ;;
        @executable_path/*) printf '%s/%s\n' "$mercs_root" "${1#@executable_path/}" ;;
        /*) printf '%s\n' "$1" ;;
        *) return 1 ;;
    esac
}
mercs_check_arch() {
    for mercs_arch in $mercs_archs; do
        lipo -verify_arch "$mercs_arch" "$1" >/dev/null 2>&1 || mercs_fail "$1 lacks the executable's $mercs_arch architecture."
    done
}
# Resolve @rpath against the built executable's actual load commands first.
mercs_resolve_library() {
    mercs_link=$1
    mercs_owner=$2
    case "$mercs_link" in
        @rpath/*)
            mercs_rpaths "$mercs_owner" > "$mercs_stage/rpaths.txt"
            while IFS= read -r mercs_rpath; do
                mercs_dir=$(mercs_expand_path "$mercs_rpath" "$mercs_owner") || continue
                mercs_candidate="$mercs_dir/${mercs_link#@rpath/}"
                if [ -f "$mercs_candidate" ]; then printf '%s\n' "$mercs_candidate"; return 0; fi
            done < "$mercs_stage/rpaths.txt"
            ;;
        *)
            mercs_candidate=$(mercs_expand_path "$mercs_link" "$mercs_owner") || return 1
            if [ -f "$mercs_candidate" ]; then printf '%s\n' "$mercs_candidate"; return 0; fi
            ;;
    esac
    return 1
}

# Bundle the SDL3 library the executable actually links. A Homebrew dylib or
# an official SDL3.framework is supported; no SDL2 compatibility shim is used.
mercs_sdl_link=$(mercs_dependencies "$mercs_exe" | awk '/(^|\/)libSDL3[^/]*\.dylib$/ || /(^|\/)SDL3\.framework\/(Versions\/[^/]+\/)?SDL3$/ {print; exit}')
[ -n "$mercs_sdl_link" ] || mercs_fail "no native SDL3 library was found in mercs; rebuild with pkg-config sdl3."
mercs_sdl_source=${MERCS_SDL3_LIBRARY:-${MERCS_SDL3_DYLIB:-}}
if [ -z "$mercs_sdl_source" ]; then
    mercs_sdl_source=$(mercs_resolve_library "$mercs_sdl_link" "$mercs_exe") || mercs_sdl_source=
fi
if [ -z "$mercs_sdl_source" ]; then
    mercs_pkg_config=${PKG_CONFIG:-pkg-config}
    mercs_sdl_libdir=$("$mercs_pkg_config" --variable=libdir sdl3 2>/dev/null) || mercs_sdl_libdir=
    case "$mercs_sdl_link" in
        */SDL3.framework/*) mercs_sdl_relative="SDL3.framework/${mercs_sdl_link#*SDL3.framework/}" ;;
        *) mercs_sdl_relative=$(basename "$mercs_sdl_link") ;;
    esac
    mercs_candidate="$mercs_sdl_libdir/$mercs_sdl_relative"
    [ -z "$mercs_sdl_libdir" ] || [ ! -f "$mercs_candidate" ] || mercs_sdl_source=$mercs_candidate
fi
[ -n "$mercs_sdl_source" ] && [ -f "$mercs_sdl_source" ] || mercs_fail "cannot locate $mercs_sdl_link; set MERCS_SDL3_LIBRARY to the matching local SDL3 library binary."
mercs_check_arch "$mercs_sdl_source"
case "$mercs_sdl_link" in
    */SDL3.framework/*)
        case "$mercs_sdl_source" in
            */SDL3.framework/*) mercs_framework_source="${mercs_sdl_source%%/SDL3.framework/*}/SDL3.framework" ;;
            *) mercs_fail "the SDL3.framework load command requires a binary inside its matching SDL3.framework." ;;
        esac
        mercs_sdl_relative="SDL3.framework/${mercs_sdl_link#*SDL3.framework/}"
        # Preserve version and Resources symlinks in the complete framework.
        cp -R -P "$mercs_framework_source" "$mercs_bundle/Contents/Frameworks/SDL3.framework"
        mercs_sdl_sign_target="$mercs_bundle/Contents/Frameworks/SDL3.framework"
        ;;
    *)
        mercs_sdl_relative=$(basename "$mercs_sdl_link")
        cp -L "$mercs_sdl_source" "$mercs_bundle/Contents/Frameworks/$mercs_sdl_relative"
        mercs_sdl_sign_target="$mercs_bundle/Contents/Frameworks/$mercs_sdl_relative"
        ;;
esac
mercs_sdl_binary="$mercs_bundle/Contents/Frameworks/$mercs_sdl_relative"
[ -f "$mercs_sdl_binary" ] || mercs_fail "the copied SDL3 library does not match $mercs_sdl_link."
chmod u+w "$mercs_sdl_binary"
install_name_tool -id "@executable_path/../Frameworks/$mercs_sdl_relative" "$mercs_sdl_binary"
install_name_tool -change "$mercs_sdl_link" "@executable_path/../Frameworks/$mercs_sdl_relative" "$mercs_bundle/Contents/MacOS/mercs"

cat > "$mercs_bundle/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Mercenaries</string>
  <key>CFBundleDisplayName</key><string>MechWarrior 2: Mercenaries</string>
  <key>CFBundleExecutable</key><string>mercs</string>
  <key>CFBundleIdentifier</key><string>org.mercsport.mercenaries</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSMinimumSystemVersion</key><string>10.14</string>
PLIST
if [ -f "$mercs_root/packaging/macos/mercsport.icns" ]; then
    cp "$mercs_root/packaging/macos/mercsport.icns" "$mercs_bundle/Contents/Resources/"
    printf '%s\n' '  <key>CFBundleIconFile</key><string>mercsport</string>' >> "$mercs_bundle/Contents/Info.plist"
fi
# An explicit Mercs version is optional. Never reuse MW2's CI tag or 1.0.0.
if [ -n "${MERCS_APP_VERSION:-}" ]; then
    printf '%s\n' "$MERCS_APP_VERSION" | awk '/^[0-9]+(\.[0-9]+)?(\.[0-9]+)?$/ {ok=1} END {exit !ok}' || mercs_fail "MERCS_APP_VERSION must contain one to three numeric components."
    printf '  <key>CFBundleShortVersionString</key><string>%s</string>\n  <key>CFBundleVersion</key><string>%s</string>\n' "$MERCS_APP_VERSION" "$MERCS_APP_VERSION" >> "$mercs_bundle/Contents/Info.plist"
fi
printf '%s\n' '</dict></plist>' >> "$mercs_bundle/Contents/Info.plist"

# Do not silently leave a Homebrew-only load command in an otherwise complete
# Finder app. Unusual extra native dependencies need explicit bundling support.
for mercs_binary in "$mercs_bundle/Contents/MacOS/mercs" "$mercs_sdl_binary"; do
    mercs_dependencies "$mercs_binary" > "$mercs_stage/dependencies.txt"
    while IFS= read -r mercs_dependency; do
        case "$mercs_dependency" in
            /System/Library/*|/usr/lib/*) continue ;;
            @executable_path/../Frameworks/*) mercs_target="$mercs_bundle/Contents/Frameworks/${mercs_dependency#@executable_path/../Frameworks/}" ;;
            @loader_path/*) mercs_target="$(dirname "$mercs_binary")/${mercs_dependency#@loader_path/}" ;;
            *) mercs_fail "unbundled dependency in $mercs_binary: $mercs_dependency" ;;
        esac
        [ -f "$mercs_target" ] || mercs_fail "missing bundled dependency: $mercs_dependency"
    done < "$mercs_stage/dependencies.txt"
done

# install_name_tool changes invalidate signatures; sign the nested libraries
# before the app. This is a local ad-hoc signature, not notarization.
codesign --force -s - "$mercs_sdl_sign_target"
codesign --force -s - "$mercs_bundle"
codesign --verify --deep --strict "$mercs_bundle"

# Replace the previous generated app only after the new bundle is complete.
if [ -e "$mercs_app" ] || [ -L "$mercs_app" ]; then
    [ ! -L "$mercs_app" ] || mercs_fail "$mercs_app is a symlink; move it before building the app."
    mv "$mercs_app" "$mercs_stage/previous.app"
fi
if ! mv "$mercs_bundle" "$mercs_app"; then
    if [ -e "$mercs_stage/previous.app" ] && ! mv "$mercs_stage/previous.app" "$mercs_app"; then
        printf '%s\n' "Previous app preserved at $mercs_stage/previous.app" >&2
        mercs_stage=   # keep the backup if restoring it also failed
    fi
    mercs_fail "could not install the completed app."
fi
printf '%s\n' "Built $mercs_app" "Open it in Finder. It finds Mercs-game beside the app or in your home folder, or asks you to choose it."
