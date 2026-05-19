#!/bin/bash
set -e
BINARY="$(dirname "$0")/build/src/a3-motion-ui/a3-motion-ui_artefacts/Debug/Standalone/a3-motion-ui"
exec "$BINARY" "$@"
