#!/usr/bin/env bash
# scripts/run_demo.sh
# Demo: insmod -> configure sysfs -> run CLI test -> rmmod
# Returns 0 on success (test passed), non-zero otherwise.

set -euo pipefail

TOPDIR="$(cd "$(dirname "$0")/.." && pwd)"
KER_DIR="$TOPDIR/kernel"
KO_FILE="$(find "$KER_DIR" -maxdepth 1 -type f -name '*.ko' -print -quit || true)"
SIMTMP_DEVNAME="simtemp0"

# Prefer user/cli, fallback to cli*.py at top-level
USER_CLI="$TOPDIR/user/cli/main.py"
if [ ! -f "$USER_CLI" ]; then
    USER_CLI="$(find "$TOPDIR" -maxdepth 2 -type f -name 'cli*.py' -print -quit || true)"
fi

# Sudo if not root
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

if [ -z "$KO_FILE" ]; then
    echo "ERROR: Module .ko not found in $KER_DIR. Run scripts/build.sh first."
    exit 2
fi

echo "Using module: $KO_FILE"
echo "Using CLI: $USER_CLI"

cleanup() {
    echo "Cleaning up..."
    if lsmod | grep -q "$(basename "$KO_FILE" .ko)"; then
        $SUDO rmmod "$(basename "$KO_FILE" .ko)" || true
    fi
}
trap cleanup EXIT

# Insert module
echo "Inserting module..."
$SUDO insmod "$KO_FILE" || {
    echo "Module already inserted or error occurred."
}

# Wait for device and sysfs
echo "Waiting for /sys/class/simtemp/* and /dev/$SIMTMP_DEVNAME..."
timeout=10
i=0
while [ $i -lt $timeout ]; do
    simsys="$(ls -d /sys/class/simtemp/* 2>/dev/null | head -n1 || true)"
    devnode="/dev/$SIMTMP_DEVNAME"
    if [ -n "$simsys" ] && [ -e "$devnode" ]; then
        echo "Device available: $devnode  sysfs: $simsys"
        break
    fi
    sleep 1
    i=$((i+1))
done
if [ $i -ge $timeout ]; then
    echo "ERROR: Device or sysfs did not appear in $timeout seconds."
    exit 3
fi

# Quick test configuration
SAMPLING_MS=2000
THRESHOLD_MC=28000   # default temp ~25000 => triggers alert
MODE="normal"

echo "Configuring device: sampling_ms=$SAMPLING_MS threshold_mc=$THRESHOLD_MC mode=$MODE"
echo "$SAMPLING_MS" | $SUDO tee "$simsys/sampling_ms" >/dev/null
echo "$THRESHOLD_MC" | $SUDO tee "$simsys/threshold_mc" >/dev/null
echo -n "$MODE" | $SUDO tee "$simsys/mode" >/dev/null

# Run CLI test
if [ -n "$USER_CLI" ] && [ -f "$USER_CLI" ]; then
    echo "Running CLI test..."
    python3 "$USER_CLI" --sampling "$SAMPLING_MS" --threshold "$THRESHOLD_MC" --test
    rc=$?
    if [ $rc -eq 0 ]; then
        echo "CLI test exited with code 0 -> PASS"
        exit 0
    else
        echo "CLI test failed (exit code $rc)"
        exit $rc
    fi
else
    echo "CLI not found. Performing fallback read test."
    devnode="/dev/$SIMTMP_DEVNAME"
    if [ -e "$devnode" ]; then
        echo "Attempting quick read (timeout 3s)..."
        dd if="$devnode" bs=1 count=1 status=none || true
    fi
    echo "Demo completed (no CLI run)."
    exit 0
fi
