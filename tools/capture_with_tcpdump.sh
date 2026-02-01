#!/bin/bash
#
# Capture patch server traffic with tcpdump + run PatchClient
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="/home/solarbaron/nvme/SteamLibrary/steamapps/common/Lord of the Rings Online"
WRAPPER="$SCRIPT_DIR/run_patch_client.exe"
PCAP_FILE="/tmp/patch_$(date +%s).pcap"

echo "================================================"
echo "LOTRO Patch Traffic Capture (tcpdump)"
echo "================================================"
echo ""
echo "[*] Capture file: $PCAP_FILE"
echo "[*] Press Ctrl+C when patching completes"
echo ""

# Start tcpdump in background
echo "[*] Starting tcpdump (requires sudo)..."
sudo tcpdump -i any host 64.37.188.7 -w "$PCAP_FILE" &
TCPDUMP_PID=$!
sleep 1

cd "$GAME_DIR"

# Run patchclient with dataonly to check iterations
echo "[*] Running patchclient..."
wine "$WRAPPER" PatchClient.dll "patch.lotro.com:6015 --language English --filesonly" 2>&1

echo ""
echo "[*] Running dataonly phase..."
wine "$WRAPPER" PatchClient.dll "patch.lotro.com:6015 --language English --dataonly" 2>&1

# Stop tcpdump
echo ""
echo "[*] Stopping tcpdump..."
sudo kill $TCPDUMP_PID 2>/dev/null
sleep 1

echo ""
echo "[*] Capture complete!"
echo "[*] PCAP file: $PCAP_FILE"
echo ""
echo "[*] View with: wireshark $PCAP_FILE"
echo "[*] Or: tcpdump -r $PCAP_FILE -X | head -200"
