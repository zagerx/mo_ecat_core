#!/usr/bin/env python3
"""Parse SII EEPROM backup to show STRINGS and GENERAL categories."""

import struct
import sys

EEPROM_FILE = "docs/backup_eeprom_20260617_170023.bin"
ECT_SII_STRING = 0x0A
ECT_SII_GENERAL = 0x1E
ECT_SII_FMMU = 0x28
ECT_SII_SYNC_M = 0x29
ECT_SII_TXPDO = 0x32
ECT_SII_RXPDO = 0x33
ECT_SII_DC = 0x3C


def read_u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def parse_strings(data, start):
    """Parse STRINGS category starting at byte offset `start`.

    Category header:
      word0: category type
      word1: following category word size
    Data starts at start + 4.
    """
    cat_type = read_u16(data, start)
    cat_size = read_u16(data, start + 2)
    print(f"STRINGS category at 0x{start:04x}: type=0x{cat_type:04x}, size={cat_size} words ({cat_size * 2} bytes)")

    ba = start + 4
    n_strings = data[ba]
    ba += 1
    print(f"  Number of strings: {n_strings}")

    strings = [""]  # index 0 is empty
    for i in range(1, n_strings + 1):
        length = data[ba]
        ba += 1
        s = data[ba:ba + length].decode("latin-1")
        ba += length
        strings.append(s)
        print(f"  string[{i}] (len={length}): \"{s}\"")

    return strings


def parse_general(data, start, strings):
    cat_type = read_u16(data, start)
    cat_size = read_u16(data, start + 2)
    print(f"\nGENERAL category at 0x{start:04x}: type=0x{cat_type:04x}, size={cat_size} words ({cat_size * 2} bytes)")

    ba = start + 4
    group_idx = data[ba + 0]
    img_idx = data[ba + 1]
    order_idx = data[ba + 2]
    name_idx = data[ba + 3]
    print(f"  GroupIdx  = {group_idx} -> \"{strings[group_idx] if group_idx < len(strings) else 'N/A'}\"")
    print(f"  ImgIdx    = {img_idx} -> \"{strings[img_idx] if img_idx < len(strings) else 'N/A'}\"")
    print(f"  OrderIdx  = {order_idx} -> \"{strings[order_idx] if order_idx < len(strings) else 'N/A'}\"")
    print(f"  NameIdx   = {name_idx} -> \"{strings[name_idx] if name_idx < len(strings) else 'N/A'}\"")


def find_category(data, cat_type, start=0x80):
    """Traverse category sections looking for cat_type."""
    a = start
    while a + 4 <= len(data):
        p = read_u16(data, a)
        if p == 0xFFFF:
            return None
        length = read_u16(data, a + 2)
        if p == cat_type:
            return a
        a += 4 + length * 2
    return None


def main():
    with open(EEPROM_FILE, "rb") as f:
        data = f.read()

    print(f"EEPROM size: {len(data)} bytes\n")

    # Header
    print("=== Fixed Header ===")
    print(f"  PDI Control       : 0x{read_u16(data, 0x00):04x}")
    print(f"  PDI Configuration : 0x{read_u16(data, 0x02):04x}")
    print(f"  Vendor ID         : 0x{read_u32(data, 0x10):08x}")
    print(f"  Product Code      : 0x{read_u32(data, 0x14):08x}")
    print(f"  Revision Number   : 0x{read_u32(data, 0x18):08x}")
    print(f"  Serial Number     : 0x{read_u32(data, 0x1c):08x}")
    print(f"  EEPROM Size word  : 0x{read_u16(data, 0x7c):04x}")

    print("\n=== Category Sections ===")
    strings_offset = find_category(data, ECT_SII_STRING)
    if strings_offset is None:
        print("STRINGS category not found")
        return
    strings = parse_strings(data, strings_offset)

    general_offset = find_category(data, ECT_SII_GENERAL)
    if general_offset is None:
        print("GENERAL category not found")
        return
    parse_general(data, general_offset, strings)


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


if __name__ == "__main__":
    main()
