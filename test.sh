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

echo ""
echo "=== Running tests ($BUILD_TYPE, runner built $BUILT_AT) ==="

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
