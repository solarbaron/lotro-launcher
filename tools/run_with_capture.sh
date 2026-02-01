#!/bin/bash
#
# Run PatchClient.dll with socket traffic capture
#
# This uses LD_PRELOAD to intercept socket calls
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="/home/solarbaron/nvme/SteamLibrary/steamapps/common/Lord of the Rings Online"
HOOK_LIB="$SCRIPT_DIR/socket_hook32.so"

echo "================================================"
echo "LOTRO Patch Traffic Capture (LD_PRELOAD method)"
echo "================================================"
echo ""

# Check if hook library exists
if [ ! -f "$HOOK_LIB" ]; then
    echo "Building socket hook..."
    gcc -shared -fPIC -o "$HOOK_LIB" "$SCRIPT_DIR/socket_hook.c" -ldl
    if [ $? -ne 0 ]; then
        echo "Failed to build socket hook"
        exit 1
    fi
fi

echo "[*] Hook library: $HOOK_LIB"
echo "[*] Game directory: $GAME_DIR"
echo ""

cd "$GAME_DIR"

echo "[*] Running PatchClient.dll with traffic capture..."
echo "[*] Traffic will be logged to /tmp/patch_traffic_*.log"
echo ""

# Run with LD_PRELOAD
LD_PRELOAD="$HOOK_LIB" wine rundll32.exe PatchClient.dll,Patch \
    --patchserver=patch.lotro.com:6015 \
    --gamepath="$GAME_DIR" \
    --filesonly=true

echo ""
echo "[*] Capture complete!"
echo "[*] Check logs:"
ls -la /tmp/patch_traffic_*.log 2>/dev/null || echo "    No traffic captured"
