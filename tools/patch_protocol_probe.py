#!/usr/bin/env python3
"""
LOTRO Patch Server Protocol - XML-RPC Client Prototype

Based on reverse engineering of PatchClient.dll:
- Protocol: XML-RPC with encryption envelope
- Port: 6015
- Encryption: 3DES with RSA key exchange

This is a prototype to test connectivity and understand the protocol.
"""

import socket
import ssl
import struct
from typing import Optional

PATCH_SERVER = "patch.lotro.com"
PATCH_PORT = 6015

# From DLL analysis - these appear to be crypto parameters
# Could be elliptic curve parameters (432 bit ~ 54 bytes)
CRYPTO_PARAMS = {
    'param1': bytes.fromhex("0340340340340340340340340340340340340340340340340340340323C313FAB50589703B5EC68D3587FEC60D161CC149C1AD4A91"),
    'param2': bytes.fromhex("20D0AF8903A96F8D5FA2C255745D3C451B302C9346D9B7E485E7BCE41F6B591F3E8F6ADDCBB0BC4C2F947A7DE1A89B625D6A598B3760"),
}

def create_xmlrpc_request(method: str, params: list) -> bytes:
    """Create an XML-RPC request payload"""
    xml = f'''<?xml version="1.0"?>
<methodCall>
<methodName>{method}</methodName>
<params>
'''
    for param in params:
        if isinstance(param, str):
            xml += f'<param><value><string>{param}</string></value></param>\n'
        elif isinstance(param, int):
            xml += f'<param><value><i4>{param}</i4></value></param>\n'
        elif isinstance(param, bool):
            xml += f'<param><value><boolean>{"1" if param else "0"}</boolean></value></param>\n'
    
    xml += '''</params>
</methodCall>'''
    
    return xml.encode('utf-8')

def create_envelope(payload: bytes, encrypted: bool = False) -> bytes:
    """
    Create the message envelope
    
    Based on DLL strings, the envelope likely has:
    - Length prefix
    - Encryption flag
    - Encrypted/plain payload
    """
    # Simple envelope format (speculation - needs verification)
    # [4 bytes: length] [1 byte: flags] [payload]
    flags = 0x01 if encrypted else 0x00
    envelope = struct.pack('<I', len(payload) + 1) + bytes([flags]) + payload
    return envelope

def try_connection():
    """Attempt to connect and probe the patch server"""
    print(f"Connecting to {PATCH_SERVER}:{PATCH_PORT}...")
    
    # Try plain TCP first
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((PATCH_SERVER, PATCH_PORT))
        print("[OK] TCP connection established")
        
        # Try sending a simple probe
        # First, try just connecting and see if server sends anything
        print("Waiting for server greeting...")
        sock.settimeout(5)
        try:
            data = sock.recv(1024)
            print(f"[<--] Received {len(data)} bytes: {data[:64].hex()}")
        except socket.timeout:
            print("[--] No greeting received (expected)")
        
        # Try sending a minimal HTTP-like request
        print("\n[-->] Sending HTTP probe...")
        http_request = b"GET / HTTP/1.1\r\nHost: patch.lotro.com\r\n\r\n"
        sock.send(http_request)
        
        sock.settimeout(5)
        try:
            response = sock.recv(4096)
            print(f"[<--] Received {len(response)} bytes")
            print(f"      Hex: {response[:64].hex()}")
            print(f"      Text: {response[:200]}")
        except socket.timeout:
            print("[--] No response to HTTP probe")
        
        sock.close()
        
        # Try sending an XML-RPC request
        print("\n" + "="*60)
        print("Trying XML-RPC format...")
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((PATCH_SERVER, PATCH_PORT))
        
        # Create an XML-RPC request
        xmlrpc = create_xmlrpc_request("system.listMethods", [])
        
        # Try with HTTP headers
        http_xmlrpc = (
            f"POST / HTTP/1.1\r\n"
            f"Host: {PATCH_SERVER}\r\n"
            f"Content-Type: text/xml\r\n"
            f"Content-Length: {len(xmlrpc)}\r\n"
            f"\r\n"
        ).encode() + xmlrpc
        
        print(f"[-->] Sending XML-RPC request ({len(http_xmlrpc)} bytes)...")
        sock.send(http_xmlrpc)
        
        sock.settimeout(5)
        try:
            response = sock.recv(4096)
            print(f"[<--] Received {len(response)} bytes")
            print(f"      Hex: {response[:128].hex()}")
            try:
                print(f"      Text: {response.decode('utf-8', errors='replace')[:500]}")
            except:
                pass
        except socket.timeout:
            print("[--] No response to XML-RPC")
        
        sock.close()
        
    except Exception as e:
        print(f"[ERROR] {e}")
        
    # Try with SSL
    print("\n" + "="*60)
    print("Trying SSL/TLS connection...")
    try:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        
        ssl_sock = context.wrap_socket(sock, server_hostname=PATCH_SERVER)
        ssl_sock.connect((PATCH_SERVER, PATCH_PORT))
        print("[OK] SSL connection established")
        print(f"     Cipher: {ssl_sock.cipher()}")
        
        # Try XML-RPC over SSL
        xmlrpc = create_xmlrpc_request("GetVersion", [])
        ssl_sock.send(xmlrpc)
        
        response = ssl_sock.recv(4096)
        print(f"[<--] Received {len(response)} bytes")
        print(f"      Hex: {response[:64].hex()}")
        
        ssl_sock.close()
        
    except ssl.SSLError as e:
        print(f"[SSL ERROR] {e}")
        print("(Server likely uses custom encryption, not TLS)")
    except Exception as e:
        print(f"[ERROR] {e}")

if __name__ == "__main__":
    try_connection()
