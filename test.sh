#!/bin/bash
# A3 Motion UI Test Script
#
# Usage: ./test.sh [OPTIONS] [-- CTEST ARGS]
#
# Options:
#   -d, --debug     Test the Debug build
#   -r, --release   Test the Release build [default]
#   -h, --help      Show this help
#
# Anything after -- is handed to ctest unchanged.
#
# Examples:
#   ./test.sh                       # build the tests, then run them all
#   ./test.sh -- -R ConeThinning    # ... only that suite
#   ./test.sh -d                    # the Debug build
#
# Why this exists: ctest RUNS a binary, it does not BUILD one. So
# `./build.sh && ctest` runs whatever test binary was built last -- on
# 2026-09-13 that was the evening before, and two runs were reported as green
# that had tested nothing of that morning's work. A red result announces
# itself; a green one that tested the wrong thing never does. This script does
# the two steps in the order that makes the answer true, and prints WHEN the
# binary it ran was built, so the claim can be checked rather than believed.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"
BUILD_FLAG=""
CTEST_ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            BUILD_FLAG="-d"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            BUILD_FLAG="-r"
            shift
            ;;
        -h|--help)
            sed -n '2,${/^#/!q; s/^# \?//p}' "$0"
            exit 0
            ;;
        --)
            shift
            CTEST_ARGS=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1  (ctest arguments go after --)"
            exit 1
            ;;
    esac
done

# Build the app as well as the tests, deliberately. The tests link the engine
# and the UI's own units; building only the test target would still leave the
# question "does the thing I am about to run on the device agree with this?"
# open, and that question is the whole reason this script exists.
"$SCRIPT_DIR/build.sh" -t $BUILD_FLAG

TEST_BINARY="$BUILD_DIR/src/a3-motion-tests/a3-motion-tests_artefacts/$BUILD_TYPE/a3-motion-tests"

if [ ! -x "$TEST_BINARY" ]; then
    echo ""
    echo "=== No test runner at $TEST_BINARY ==="
    echo "TESTS_ENABLED is on by default; if this is missing, the configure step is wrong."
    exit 1
fi

BUILT_AT="$(date -r "$TEST_BINARY" '+%Y-%m-%d %H:%M:%S')"

# The shipped material as committed (see src/a3-motion-tests/CMakeLists.txt):
# what ACTION and the skin panel write into the working copy on the rig is a
# set being played, not the library the tests hold to its promises.
COMMITTED_DIR="$BUILD_DIR/committed"
rm -rf "$COMMITTED_DIR"
mkdir -p "$COMMITTED_DIR"
git -C "$SCRIPT_DIR" archive HEAD config pattern | tar -x -C "$COMMITTED_DIR"
COMMITTED_AT="$(git -C "$SCRIPT_DIR" log -1 --format='%h')"

# The one truth for OSC is a3-core's file, not this repository's. Installed it
# is /usr/share/a3/a3-osc.json; on a machine without the package, the a3-core
# checkout beside this one or above it (#63). OscTruthContract fails without
# either -- and the line below says which one the run held Motion against.
#
# Without an A3_OSC_TRUTH of the caller's, the run holds Motion against an
# offline copy of that file: the same addresses and ports, but every named
# host except "local" and "any" moved to 192.0.2.x, the documentation range
# that routes nowhere. On the rig the installed truth names the live Core, and
# the suite used to send positions and 3D/FREQ/Q there (#67). The test runner
# refuses every OSC sender on its own; this is the second wall, not the first.
installed_truth() {
    if [ -f /usr/share/a3/a3-osc.json ]; then
        echo /usr/share/a3/a3-osc.json
        return
    fi
    # Beside this checkout, or -- inside the a3-system umbrella, where this is
    # a3-motion's ui -- two levels up.
    for CORE in "$SCRIPT_DIR/../a3-core" "$SCRIPT_DIR/../../a3-core"; do
        BESIDE="$CORE/platform-config/debian-x86_64/a3-core/usr/share/a3/a3-osc.json"
        if [ -f "$BESIDE" ]; then
            realpath "$BESIDE"
            return
        fi
    done
}

offline_copy() {
    python3 -I "$SCRIPT_DIR/tools/a3-offline-truth" "$1" "$2"
}

if [ -n "$A3_OSC_TRUTH" ]; then
    OSC_TRUTH_USED="$A3_OSC_TRUTH (given)"
else
    INSTALLED_TRUTH="$(installed_truth)"
    if [ -n "$INSTALLED_TRUTH" ]; then
        export A3_OSC_TRUTH="$BUILD_DIR/a3-osc-offline.json"
        offline_copy "$INSTALLED_TRUTH" "$A3_OSC_TRUTH"
        OSC_TRUTH_USED="$A3_OSC_TRUTH (offline copy of $INSTALLED_TRUTH)"
    else
        OSC_TRUTH_USED="none found -- OscTruthContract will say so"
    fi
fi

echo ""
echo "=== Running tests ($BUILD_TYPE, runner built $BUILT_AT, library as committed in $COMMITTED_AT) ==="
echo "=== OSC truth: $OSC_TRUTH_USED ==="

set +e
ctest --test-dir "$BUILD_DIR" --output-on-failure "${CTEST_ARGS[@]}"
CTEST_STATUS=$?
set -e

echo ""
if [ "$CTEST_STATUS" -eq 0 ]; then
    echo "=== Tests passed — runner built $BUILT_AT ==="
else
    echo "=== Tests FAILED (exit $CTEST_STATUS) — runner built $BUILT_AT ==="
fi

# The timestamp is not decoration. Quote it whenever reporting a result: it is
# the difference between "the tests are green" and "the tests are green for
# the code that is actually here".
exit "$CTEST_STATUS"
