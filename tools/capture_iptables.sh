#!/bin/bash
#
# Alternative traffic capture using socat and iptables
#
# This uses iptables REDIRECT to force traffic to our proxy
#

PATCH_IP="64.37.188.7"
PATCH_PORT=6015
LOCAL_PORT=16015  # Our proxy port
LOG_DIR="/home/solarbaron/git/lotro-launcher/tools"

echo "================================================"
echo "LOTRO Patch Traffic Capture (iptables method)"
echo "================================================"

if [[ $EUID -ne 0 ]]; then
   echo "This script needs sudo for iptables"
   echo "Usage: sudo $0"
   exit 1
fi

# Create log files
TRAFFIC_LOG="$LOG_DIR/patch_capture_$(date +%Y%m%d_%H%M%S).txt"
echo "Logging to: $TRAFFIC_LOG"

# Start socat proxy with logging
echo "[*] Starting socat proxy..."
socat -v TCP-LISTEN:$LOCAL_PORT,fork,reuseaddr TCP:$PATCH_IP:$PATCH_PORT 2>&1 | tee "$TRAFFIC_LOG" &
SOCAT_PID=$!

sleep 1

# Add iptables redirect rule
echo "[*] Adding iptables redirect rule..."
iptables -t nat -A OUTPUT -p tcp -d $PATCH_IP --dport $PATCH_PORT -j REDIRECT --to-port $LOCAL_PORT

echo ""
echo "[*] Capture running! Now run PatchClient in another terminal:"
echo "    cd '/home/solarbaron/nvme/SteamLibrary/steamapps/common/Lord of the Rings Online'"
echo "    wine rundll32.exe PatchClient.dll,Patch"
echo ""
echo "Press Ctrl+C when done..."

# Wait for interrupt
trap cleanup INT
cleanup() {
    echo ""
    echo "[*] Cleaning up..."
    iptables -t nat -D OUTPUT -p tcp -d $PATCH_IP --dport $PATCH_PORT -j REDIRECT --to-port $LOCAL_PORT 2>/dev/null
    kill $SOCAT_PID 2>/dev/null
    echo "[*] Traffic saved to: $TRAFFIC_LOG"
    exit 0
}

wait $SOCAT_PID
