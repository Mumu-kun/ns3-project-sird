#!/bin/bash
# Sync and build ns-3

PROJECT_DIR="${PROJECT_DIR:-/workspaces/ns3-project}"
NS3_DIR="${NS3_DIR:-/ns-3}"

# Sync first (default: project → ns-3)
bash "$PROJECT_DIR/scripts/sync.sh" --to-ns3

# Build
cd "$NS3_DIR"
./ns3 build -j$(nproc)

echo "Build complete"
