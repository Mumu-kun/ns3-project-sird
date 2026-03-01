#!/bin/bash
# Simple bidirectional sync between /ns-3 and project
# Edit the SYNC_PATHS list below to specify what to sync

set -e

PROJECT_DIR="${PROJECT_DIR:-/workspaces/ns3-project}"
NS3_DIR="${NS3_DIR:-/ns-3}"

# List of directories (relative to ns-3 root) to sync
# Add your custom module directories here, e.g., "src/my-module/"
SYNC_PATHS=(
    ".vscode/"
    "scratch/my-simulations/"
    # "src/my-module/"
)

DIRECTION="from-ns3"

if [ $# -gt 0 ]; then
    case "$1" in
        --to-ns3) DIRECTION="to-ns3" ;;
        --from-ns3) DIRECTION="from-ns3" ;;
        *) echo "Usage: $0 [--to-ns3|--from-ns3]"; exit 1 ;;
    esac
fi

if [ "$DIRECTION" = "to-ns3" ]; then
    echo "Syncing from project to /ns-3..."
    for path in "${SYNC_PATHS[@]}"; do
        src="$PROJECT_DIR/$path"
        dst="$NS3_DIR/$path"
        if [ -e "$src" ]; then
            rsync -av "$src" "$dst"
            echo "  ✓ $path"
        else
            echo "  ⚠ $path not found in project, skipping"
        fi
    done
    # Apply patches
    if [ -d "$PROJECT_DIR/patches" ]; then
        for p in "$PROJECT_DIR/patches"/*.patch; do
            [ -f "$p" ] && { echo "  Applying patch: $(basename "$p")"; patch -d "$NS3_DIR" -p1 < "$p"; }
        done
    fi
else
    echo "Syncing from /ns-3 to project..."
    for path in "${SYNC_PATHS[@]}"; do
        src="$NS3_DIR/$path"
        dst="$PROJECT_DIR/$path"
        if [ -e "$src" ]; then
            mkdir -p "$dst"
            rsync -av --delete "$src" "$dst"
            echo "  ✓ $path"
        else
            echo "  ⚠ $path not found in /ns-3, skipping"
        fi
    done
fi

echo "Sync complete."