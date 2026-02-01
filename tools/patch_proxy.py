#!/usr/bin/env python3
"""
LOTRO Patch Server Traffic Proxy

This script acts as a proxy between the game and patch.lotro.com:6015
to capture and analyze the patch server protocol.

Usage:
1. Run this script: python3 patch_proxy.py
2. Modify /etc/hosts or use iptables to redirect patch.lotro.com to 127.0.0.1
3. Run PatchClient.dll (via Wine or the launcher)
4. Analyze the captured traffic

The captured data will be saved to patch_traffic.bin
"""

import socket
import threading
import sys
import os
from datetime import datetime

LISTEN_HOST = '0.0.0.0'
LISTEN_PORT = 6015
TARGET_HOST = '64.37.188.7'  # patch.lotro.com IP
TARGET_PORT = 6015

BUFFER_SIZE = 8192
LOG_FILE = 'patch_traffic.log'
BINARY_FILE = 'patch_traffic.bin'

class TrafficLogger:
    def __init__(self):
        self.log = open(LOG_FILE, 'w')
        self.binary = open(BINARY_FILE, 'wb')
        
    def log_packet(self, direction: str, data: bytes):
        timestamp = datetime.now().isoformat()
        
        # Text log
        self.log.write(f"\n{'='*60}\n")
        self.log.write(f"[{timestamp}] {direction} ({len(data)} bytes)\n")
        self.log.write(f"{'='*60}\n")
        self.log.write(f"Hex: {data[:256].hex()}\n")
        
        # Try to decode as text
        try:
            text = data[:256].decode('utf-8', errors='replace')
            self.log.write(f"Text: {text}\n")
        except:
            pass
            
        self.log.flush()
        
        # Binary log with marker
        marker = b'\x00\x00\x00\x00' + direction.encode() + b'\x00\x00\x00\x00'
        length = len(data).to_bytes(4, 'little')
        self.binary.write(marker + length + data)
        self.binary.flush()
        
        print(f"[{timestamp}] {direction}: {len(data)} bytes")
        print(f"  First 32 bytes: {data[:32].hex()}")
        
    def close(self):
        self.log.close()
        self.binary.close()

def forward_data(src: socket.socket, dst: socket.socket, direction: str, logger: TrafficLogger):
    """Forward data from src to dst and log it"""
    try:
        while True:
            data = src.recv(BUFFER_SIZE)
            if not data:
                break
            logger.log_packet(direction, data)
            dst.send(data)
    except Exception as e:
        print(f"Forward error ({direction}): {e}")
    finally:
        try:
            src.close()
        except:
            pass
        try:
            dst.close()
        except:
            pass

def handle_client(client_sock: socket.socket, client_addr, logger: TrafficLogger):
    """Handle a connection from the game client"""
    print(f"\n[*] New connection from {client_addr}")
    
    # Connect to the real patch server
    try:
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.connect((TARGET_HOST, TARGET_PORT))
        print(f"[*] Connected to {TARGET_HOST}:{TARGET_PORT}")
    except Exception as e:
        print(f"[!] Failed to connect to target: {e}")
        client_sock.close()
        return
        
    # Start forwarding threads
    client_to_server = threading.Thread(
        target=forward_data,
        args=(client_sock, server_sock, "CLIENT->SERVER", logger)
    )
    server_to_client = threading.Thread(
        target=forward_data,
        args=(server_sock, client_sock, "SERVER->CLIENT", logger)
    )
    
    client_to_server.start()
    server_to_client.start()
    
    client_to_server.join()
    server_to_client.join()
    
    print(f"[*] Connection from {client_addr} closed")

def main():
    print(f"""
LOTRO Patch Server Traffic Proxy
================================
Listening on: {LISTEN_HOST}:{LISTEN_PORT}
Forwarding to: {TARGET_HOST}:{TARGET_PORT}
Log file: {LOG_FILE}
Binary dump: {BINARY_FILE}

To use:
1. Add to /etc/hosts: 127.0.0.1 patch.lotro.com
2. Run PatchClient.dll or the launcher
3. Press Ctrl+C when done

""")
    
    logger = TrafficLogger()
    
    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((LISTEN_HOST, LISTEN_PORT))
        server.listen(5)
        print(f"[*] Listening for connections...")
        
        while True:
            client_sock, client_addr = server.accept()
            handler = threading.Thread(
                target=handle_client,
                args=(client_sock, client_addr, logger)
            )
            handler.start()
            
    except KeyboardInterrupt:
        print("\n[*] Shutting down...")
    except Exception as e:
        print(f"[!] Error: {e}")
    finally:
        logger.close()
        print(f"[*] Traffic saved to {LOG_FILE} and {BINARY_FILE}")

if __name__ == '__main__':
    main()
