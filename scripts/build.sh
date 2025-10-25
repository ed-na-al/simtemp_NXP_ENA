#!/usr/bin/env bash
# scripts/build.sh
# Build kernel module and user CLI. Exit non-zero on failure.

set -euo pipefail

TOPDIR="$(cd "$(dirname "$0")/.." && pwd)"
KDIR="${KDIR:-/lib/modules/$(uname -r)/build}"
KER_DIR="$TOPDIR/kernel"
USER_CLI_DIR="$TOPDIR/user/cli"

echo "Top dir: $TOPDIR"
echo "Kernel headers: $KDIR"

# Verify kernel headers
if [ ! -d "$KDIR" ]; then
    echo "ERROR: Kernel build dir not found: $KDIR"
    echo "Try installing kernel headers or set KDIR environment variable."
    exit 2
fi

# Verify kernel sources/Makefile presence
if [ ! -d "$KER_DIR" ]; then
    echo "ERROR: kernel directory not found: $KER_DIR"
    exit 3
fi

# Build kernel module (out-of-tree)
echo "Building kernel module..."
make -C "$KDIR" M="$KER_DIR" modules

# Ensure .ko exists (try to find any .ko produced in kernel dir)
KO_FILE="$(find "$KER_DIR" -maxdepth 1 -type f -name '*.ko' -print -quit || true)"
if [ -z "$KO_FILE" ]; then
    echo "ERROR: Module build did not produce .ko in $KER_DIR"
    exit 4
fi
echo "Built module: $KO_FILE"

# Prepare user CLI: if Python, run syntax check / byte-compile
if command -v python3 >/dev/null 2>&1; then
    echo "Checking Python CLI(s)..."
    if [ -d "$USER_CLI_DIR" ]; then
        PY_FILES=$(find "$USER_CLI_DIR" -name '*.py')
    else
        # fallback: try top-level cli*.py
        PY_FILES=$(find "$TOPDIR" -maxdepth 2 -name 'cli*.py' -o -name 'main.py' 2>/dev/null || true)
    fi

    if [ -n "$PY_FILES" ]; then
        for f in $PY_FILES; do
            echo "Compiling $f ..."
            python3 -m py_compile "$f"
        done
    else
        echo "Warning: No Python CLI files found to check. (expected under user/cli/ or top-level cli*.py)"
    fi
else
    echo "Warning: python3 not found; skipping CLI checks."
fi

echo "Build completed successfully."
exit 0

