#!/bin/bash
#
# Run PatchClient.dll with traffic capture using strace
# Uses our custom run_patch_client.exe wrapper
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="/home/solarbaron/nvme/SteamLibrary/steamapps/common/Lord of the Rings Online"
WRAPPER="$SCRIPT_DIR/run_patch_client.exe"
TRACE_FILE="/tmp/patch_traffic_$(date +%s).log"

echo "================================================"
echo "LOTRO Patch Traffic Capture"
echo "================================================"
echo ""
echo "[*] Wrapper: $WRAPPER"
echo "[*] Game directory: $GAME_DIR"
echo "[*] Trace output: $TRACE_FILE"
echo ""

cd "$GAME_DIR"

# Patching arguments (address:port format, no http:// prefix)
PATCH_ARGS="patch.lotro.com:6015 --language English --filesonly"

echo "[*] Running with args: $PATCH_ARGS"
echo ""

# Run with strace
strace -f -e trace=network,read,write -s 4096 -x -o "$TRACE_FILE" \
    wine "$WRAPPER" PatchClient.dll "$PATCH_ARGS" 2>&1 | tee /tmp/patch_output.log

echo ""
echo "[*] Capture complete!"
echo "[*] Trace file: $TRACE_FILE"
echo "[*] Output: /tmp/patch_output.log"
echo ""

# Look for connections to the patch server
if [ -f "$TRACE_FILE" ]; then
    echo "[*] Searching for patch server connections..."
    grep -E "connect.*6015|64\.37\.188\." "$TRACE_FILE" | head -10
fi
