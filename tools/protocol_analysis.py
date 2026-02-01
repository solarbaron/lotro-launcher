#!/usr/bin/env python3
"""
LOTRO Patch Protocol Analyzer
Analyzes captured pcap to understand the encryption format
"""

import struct

# Example request payload (from pcap)
# m....3des........[encrypted]
# First bytes: 6d 01 00 80 04 33 64 65 73 80 00 00 00

REQUEST_HEADER = bytes([
    0x6d,  # 'm' - message type
    0x01, 0x00, 0x80, 0x04,  # flags/version?
    0x33, 0x64, 0x65, 0x73,  # "3des" 
    0x80, 0x00, 0x00, 0x00   # padding/nonce?
])

# Alternative header from second request:
# d5 00 00 80 04 33 64 65 73 80 00 00 00
REQUEST_HEADER_2 = bytes([
    0xd5,  # different message type
    0x00, 0x00, 0x80, 0x04,
    0x33, 0x64, 0x65, 0x73,  # "3des"
    0x80, 0x00, 0x00, 0x00
])

def analyze_header(data: bytes):
    """Analyze the message header format"""
    if len(data) < 13:
        print("Header too short")
        return
    
    msg_type = data[0]
    flags = struct.unpack("<I", data[1:5])[0]
    cipher = data[5:9].decode('ascii', errors='ignore')
    padding = struct.unpack("<I", data[9:13])[0]
    
    print(f"Message Type: 0x{msg_type:02x} ({chr(msg_type) if 32 <= msg_type < 127 else '?'})")
    print(f"Flags: 0x{flags:08x}")
    print(f"Cipher: {cipher}")
    print(f"Padding: 0x{padding:08x}")
    
    return {
        'type': msg_type,
        'flags': flags,
        'cipher': cipher,
        'padding': padding
    }

def main():
    print("=== LOTRO Patch Protocol Analysis ===\n")
    
    print("First request header:")
    analyze_header(REQUEST_HEADER)
    
    print("\nSecond request header:")
    analyze_header(REQUEST_HEADER_2)
    
    print("\n=== Protocol Format ===")
    print("""
Request:
  POST /stateless2 HTTP/1.1
  Host: patch.lotro.com:6015
  Content-Type: application/x-www-form-urlencoded
  
  [1 byte type][4 bytes flags][4 bytes "3des"][4 bytes padding][encrypted payload]

Response:
  HTTP/1.1 200
  content-type: binary
  
  [encrypted binary response]
    
Message Types:
  0x6d ('m') - Main request
  0xd5       - Status check?
""")

if __name__ == "__main__":
    main()
