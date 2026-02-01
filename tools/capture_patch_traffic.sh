#!/bin/bash
#
# Capture PatchClient.dll traffic
#
# This script:
# 1. Starts the proxy
# 2. Modifies DNS resolution for the game
# 3. Runs PatchClient.dll
# 4. Collects the traffic

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="/home/solarbaron/nvme/SteamLibrary/steamapps/common/Lord of the Rings Online"

echo "================================================"
echo "LOTRO Patch Traffic Capture"
echo "================================================"
echo ""

# Check if running as root (needed for hosts modification)
if [[ $EUID -ne 0 ]]; then
   echo "This script needs sudo to modify /etc/hosts"
   echo ""
   echo "Option 1: Run with sudo"
   echo "  sudo $0"
   echo ""
   echo "Option 2: Manual setup"
   echo "  1. Add to /etc/hosts:  127.0.0.1 patch.lotro.com"
   echo "  2. Run: python3 $SCRIPT_DIR/patch_proxy.py"
   echo "  3. In another terminal, run PatchClient:"
   echo "     cd '$GAME_DIR'"
   echo "     wine rundll32.exe PatchClient.dll,Patch"
   echo "  4. Remove the hosts entry when done"
   exit 1
fi

# Backup hosts file
cp /etc/hosts /etc/hosts.backup

# Add our redirect
echo "127.0.0.1 patch.lotro.com" >> /etc/hosts
echo "[*] Added patch.lotro.com redirect to /etc/hosts"

# Start proxy in background
cd "$SCRIPT_DIR"
python3 patch_proxy.py &
PROXY_PID=$!
echo "[*] Started proxy (PID: $PROXY_PID)"

sleep 2

# Try to trigger a patch check
echo "[*] Running PatchClient.dll..."
cd "$GAME_DIR"
WINEPREFIX="$HOME/.wine" wine rundll32.exe PatchClient.dll,Patch --help 2>&1 || true

# Wait a bit for connections
sleep 5

# Cleanup
echo ""
echo "[*] Stopping proxy..."
kill $PROXY_PID 2>/dev/null || true

# Restore hosts file
cp /etc/hosts.backup /etc/hosts
echo "[*] Restored /etc/hosts"

echo ""
echo "[*] Traffic captured! Check:"
echo "    $SCRIPT_DIR/patch_traffic.log"
echo "    $SCRIPT_DIR/patch_traffic.bin"
