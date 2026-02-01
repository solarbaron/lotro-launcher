#!/usr/bin/env python3
"""
Analyze crypto parameters extracted from PatchClient.dll

These hex strings were found in the DLL and appear to be
cryptographic parameters (possibly elliptic curve or RSA).
"""

# Extracted hex strings from PatchClient.dll
PARAMS = [
    "0340340340340340340340340340340340340340340340340340340323C313FAB50589703B5EC68D3587FEC60D161CC149C1AD4A91",
    "20D0AF8903A96F8D5FA2C255745D3C451B302C9346D9B7E485E7BCE41F6B591F3E8F6ADDCBB0BC4C2F947A7DE1A89B625D6A598B3760",
    "120FC05D3C67A99DE161D2F4092622FECA701BE4F50F4758714E8A87BBF2A658EF8C21E7C5EFE965361F6C2999C0C247B0DBD70CE6B7",
    "10D9B4A3D9047D8B154359ABFB1B7F5485B04CEB868237DDC9DEDA982A679A5A919B626D4E50A8DD731B107A9962381FB5D807BF2618",
    "1A827EF00DD6FC0E234CAF046C6A5D8A85395B236CC4AD2CF32A0CADBDC9DDF620B0EB9906D0957F6C6FEACD615468DF104DE296CD8F",
    "800000000000000000000000000000000000000000000000000000000000000000000000000001000000000000000000000000000001",
    "00010090512DA9AF72B08349D98A5DD4C7B0532ECA51CE03E2D10F3B7AC579BD87E909AE40A6F131E9CFCE5BD967",
    "7B3EB1BDDCBA62D5D8B2059B525797FC73822C59059C623A45FF3843CEE8F87CD1855ADAA81E2A0750B80FDA2310",
    "1085E2755381DCCCE3C1557AFA10C2F0C0C2825646C5B34A394CBCFA8BC16B22E7E789E927BE216F02E1FB136A5F",
    "FC1217D4320A90452C760A58EDCD30C8DD069B3C34453837A34ED50CB54917E1C2112D84D164F444F8F74786046A",
    "E0D2EE25095206F5E2A4F9ED229F1F256E79A0E2B455970D8D0D865BD94778C576D62F0AB7519CCD2A1A906AE30D",
]

def analyze_params():
    print("Analyzing crypto parameters from PatchClient.dll")
    print("=" * 60)
    
    for i, param in enumerate(PARAMS):
        try:
            # Convert to bytes
            data = bytes.fromhex(param)
            bit_length = len(data) * 8
            
            print(f"\nParameter {i+1}:")
            print(f"  Hex length: {len(param)} chars")
            print(f"  Byte length: {len(data)} bytes")
            print(f"  Bit length: {bit_length} bits")
            print(f"  First 16 bytes: {data[:16].hex()}")
            
            # Check if it could be an RSA modulus (common sizes)
            if bit_length in [512, 1024, 2048, 4096]:
                print(f"  Could be: RSA-{bit_length} modulus")
            elif bit_length in [256, 384, 521]:
                print(f"  Could be: EC P-{bit_length} parameter")
            elif bit_length == 192:
                print(f"  Could be: 3DES key (24 bytes)")
            elif bit_length == 128:
                print(f"  Could be: AES-128 key")
                
            # Check for patterns
            int_val = int(param, 16)
            if int_val.bit_length() < bit_length - 8:
                print(f"  Note: Leading zeros present")
                
        except Exception as e:
            print(f"  Error: {e}")

    # Specific analysis
    print("\n" + "=" * 60)
    print("Specific Analysis:")
    
    # The 6th parameter looks like an order/modulus for EC
    p6 = PARAMS[5]
    print(f"\nParameter 6 (potential curve order):")
    print(f"  Value: 2^384 + 2^64 + 1")
    print(f"  This looks like a Koblitz curve parameter")
    
    # Check well-known curves
    print("\nWell-known curve parameters for reference:")
    print("  secp384r1 p: FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFF")
    print("  Our param 6:", p6)

if __name__ == "__main__":
    analyze_params()
