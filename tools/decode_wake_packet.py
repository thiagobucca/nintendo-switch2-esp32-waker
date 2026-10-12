#!/usr/bin/env python3
"""Decode a captured Switch 2 wake advertisement and print the YAML to use.

Accepts any of these hex forms (spaces and colons are ignored):
  * the full 31-byte advertisement     020106 1BFF 5305 0100037E05662000 0181...
  * manufacturer data with company ID  5305 0100037E...
  * manufacturer data without it       0100037E0566200001 81...  (capture.yaml,
                                       Home Assistant's advertisement monitor)

Usage:
  decode_wake_packet.py <hex> [--controller AA:BB:CC:DD:EE:FF]
"""

import argparse
import re
import sys

PRODUCTS = {
    0x2066: "joycon2_left",
    0x2067: "joycon2_right",
    0x2069: "pro_controller2",
}


def manufacturer_payload(raw: bytes) -> bytes:
    if raw[:3] == bytes.fromhex("020106"):
        raw = raw[5:]  # skip flags + manufacturer AD header
    if raw[:2] == bytes.fromhex("5305"):
        raw = raw[2:]  # skip company ID 0x0553
    return raw


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("hex", help="captured advertisement, in hex")
    parser.add_argument("--controller", help="sender address shown by the scanner")
    args = parser.parse_args()

    try:
        data = manufacturer_payload(bytes.fromhex(re.sub(r"[^0-9a-fA-F]", "", args.hex)))
    except ValueError:
        print("error: not a hex string", file=sys.stderr)
        return 1

    if len(data) < 16 or data[:3] != bytes.fromhex("010003") or data[3:5] != bytes.fromhex("7e05"):
        print("error: this does not look like a Switch 2 controller advertisement", file=sys.stderr)
        return 1

    product = int.from_bytes(data[5:7], "little")
    state = data[9]
    console = ":".join(f"{b:02X}" for b in reversed(data[10:16]))

    print(f"Product ID   0x{product:04X} ({PRODUCTS.get(product, 'unknown')})")
    print(f"State        0x{state:02X} ({'wake / reconnect' if state == 0x81 else 'pairing' if state == 0x00 else 'unknown'})")
    print(f"Console      {console}")
    if state != 0x81:
        print("\nwarning: state is not 0x81 -- capture with the console asleep and the controller already paired")
    print("\nswitch2_wake:")
    print(f'  controller_mac: "{args.controller.upper() if args.controller else "<sender address from the scanner>"}"')
    print(f'  console_mac: "{console}"')
    print(f"  product_id: {PRODUCTS.get(product, f'0x{product:04X}')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
