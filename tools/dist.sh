#!/usr/bin/env bash
# Builds an optimized app (with debug symbols, so crash backtraces stay readable) and puts it in
# dist/ for the user to run on their machine. Meant for the sandbox, whose Arch toolchain and
# libraries match the user's desktop:
#
#   tools/sandbox.sh run tools/dist.sh
#   ./dist/juicy-loops-studio        # on the host
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-dist"
cd "$ROOT"

cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DJLS_BUILD_TESTS=OFF \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >/dev/null
cmake --build "$BUILD"

mkdir -p "$ROOT/dist"
# Copy next to the target, then rename over it: the binary may be running on the host ("text
# file busy" for a direct copy); the running app keeps the old file, the next start gets the new.
cp "$BUILD/src/app/JuicyLoopsStudio_artefacts/RelWithDebInfo/Juicy Loops Studio" \
    "$ROOT/dist/.juicy-loops-studio.new"
mv -f "$ROOT/dist/.juicy-loops-studio.new" "$ROOT/dist/juicy-loops-studio"
{
    echo "Juicy Loops Studio $(sed -n 's/^project(JuicyLoopsStudio VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)"
    echo "commit:   $(git rev-parse --short HEAD)$(git diff --quiet HEAD -- src || echo ' + local changes')"
    echo "built:    $(date '+%Y-%m-%d %H:%M')"
    echo "compiler: $(c++ --version | head -1)"
} > "$ROOT/dist/BUILD-INFO.txt"
cat "$ROOT/dist/BUILD-INFO.txt"
