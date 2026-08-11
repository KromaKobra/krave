#!/usr/bin/env bash
#
# Configure and build on Linux. The counterpart to scripts/build.ps1.
#
# If cmake is not on PATH and this is a Nix machine, the script re-execs itself
# inside `nix develop`, so it works from a bare terminal as well as from inside
# the dev shell.
#
#   ./scripts/build.sh                  # debug build
#   ./scripts/build.sh --config release
#   ./scripts/build.sh --test           # build then run ctest
#   ./scripts/build.sh --run            # build then launch the app
#   ./scripts/build.sh --clean          # wipe the build dir first
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

config=debug
clean=0
test=0
run=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config) config=${2,,}; shift 2 ;;
        --clean)  clean=1; shift ;;
        --test)   test=1; shift ;;
        --run)    run=1; shift ;;
        -h|--help) sed -n '2,14p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

case "$config" in
    debug)             preset=linux-debug ;;
    release|relwithdebinfo) preset=linux-release ;;
    *) echo "Unknown config: $config (expected debug or release)" >&2; exit 2 ;;
esac

# Toolchain comes from the flake. Re-enter it rather than asking the user to.
if ! command -v cmake >/dev/null 2>&1 && command -v nix >/dev/null 2>&1; then
    echo "cmake not on PATH - re-running inside 'nix develop'"
    exec nix develop "$root" --command "${BASH_SOURCE[0]}" \
        --config "$config" \
        $([[ $clean == 1 ]] && echo --clean) \
        $([[ $test  == 1 ]] && echo --test) \
        $([[ $run   == 1 ]] && echo --run)
fi

build_dir="$root/build/$preset"

if [[ $clean == 1 && -d $build_dir ]]; then
    echo "Removing $build_dir"
    rm -rf "$build_dir"
fi

# Presets resolve paths relative to the source dir, so drive them from there.
cd "$root"

[[ -f "$build_dir/CMakeCache.txt" ]] || cmake --preset "$preset"
cmake --build --preset "$preset"

# The in-tree binary is not wrapped, so it needs the Qt plugin and QML import
# paths the dev shell exports. Dump them where launch.json can pick them up as
# an envFile -- VS Code's debug session does not inherit the shell otherwise.
{
    for var in PATH LD_LIBRARY_PATH QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH XDG_DATA_DIRS; do
        [[ -n ${!var:-} ]] && echo "$var=${!var}"
    done
} > "$build_dir/dev.env"

echo "Build succeeded: $build_dir"

if [[ $test == 1 ]]; then
    ctest --output-on-failure --test-dir "$build_dir"
    echo "Tests passed"
fi

if [[ $run == 1 ]]; then
    exec "$build_dir/bin/krave"
fi
