#!/usr/bin/env python3
"""Generate a basic EtherCAT SII EEPROM binary from an ESI device entry.

This generator focuses on the SII fields used by SOEM during scan/config:
configuration header, strings, general, FMMU, SyncManager, and PDO categories.
It is intended as a reproducible reference image when the ESI XML is the source
of truth.
"""

import argparse
import struct
import xml.etree.ElementTree as ET


CAT_STRINGS = 0x000A
CAT_GENERAL = 0x001E
CAT_FMMU = 0x0028
CAT_SYNCM = 0x0029
CAT_TXPDO = 0x0032
CAT_RXPDO = 0x0033
CAT_END = 0xFFFF


def parse_int(value, default=0):
    if value is None:
        return default
    value = value.strip()
    if value.startswith("#x"):
        return int(value[2:], 16)
    return int(value, 0)


def put_u16(buf, offset, value):
    struct.pack_into("<H", buf, offset, value & 0xFFFF)


def put_u32(buf, offset, value):
    struct.pack_into("<I", buf, offset, value & 0xFFFFFFFF)


def calc_crc_byte(crc, byte):
    crc ^= byte
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x07) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def sii_crc(buf):
    crc = 0xFF
    for byte in buf[:14]:
        crc = calc_crc_byte(crc, byte)
    return crc


def category(cat_type, payload):
    if len(payload) % 2:
        payload += b"\x00"
    return struct.pack("<HH", cat_type, len(payload) // 2) + payload


class StringTable:
    def __init__(self):
        self.items = []
        self.index = {"": 0}

    def add(self, value):
        value = (value or "").strip()
        if not value:
            return 0
        if value in self.index:
            return self.index[value]
        if len(value.encode("latin-1", errors="replace")) > 255:
            raise ValueError(f"string too long for SII string table: {value!r}")
        self.items.append(value)
        self.index[value] = len(self.items)
        return len(self.items)

    def payload(self):
        data = bytearray([len(self.items)])
        for item in self.items:
            raw = item.encode("latin-1", errors="replace")
            data.append(len(raw))
            data.extend(raw)
        return bytes(data)


def find_device(root, product_code, revision_no):
    for dev in root.findall("./Descriptions/Devices/Device"):
        typ = dev.find("Type")
        if typ is None:
            continue
        if parse_int(typ.attrib.get("ProductCode")) == product_code and parse_int(typ.attrib.get("RevisionNo")) == revision_no:
            return dev
    raise SystemExit(f"device ProductCode=0x{product_code:08x} RevisionNo=0x{revision_no:08x} not found")


def coe_details(coe):
    if coe is None:
        return 0
    value = 0x01
    if coe.attrib.get("SdoInfo") == "1":
        value |= 0x02
    if coe.attrib.get("PdoAssign") == "1":
        value |= 0x04
    if coe.attrib.get("PdoConfig") == "1":
        value |= 0x08
    if coe.attrib.get("CompleteAccess") == "1":
        value |= 0x20
    return value


def mailbox_protocol(mailbox):
    if mailbox is None:
        return 0
    proto = 0
    if mailbox.find("CoE") is not None:
        proto |= 0x0004
    if mailbox.find("FoE") is not None:
        proto |= 0x0008
    if mailbox.find("EoE") is not None:
        proto |= 0x0002
    if mailbox.find("SoE") is not None:
        proto |= 0x0010
    if mailbox.find("VoE") is not None:
        proto |= 0x0020
    return proto


def sm_type(name):
    return {
        "MBoxOut": 1,
        "MBoxIn": 2,
        "Outputs": 3,
        "Inputs": 4,
    }.get(name or "", 0)


def fmmu_value(name):
    return {
        "Outputs": 1,
        "Inputs": 2,
        "MBoxState": 3,
    }.get(name or "", 0xFF)


def build_general(dev, strings):
    group = dev.findtext("GroupType") or ""
    typ = dev.find("Type")
    name = dev.findtext("Name") or (typ.text if typ is not None else "")
    mailbox = dev.find("Mailbox")
    coe = mailbox.find("CoE") if mailbox is not None else None

    payload = bytearray(32)
    payload[0] = strings.add(group)
    payload[1] = 0
    payload[2] = strings.add(typ.text if typ is not None else "")
    payload[3] = strings.add(name)
    payload[5] = coe_details(coe)
    payload[6] = 1 if mailbox is not None and mailbox.find("FoE") is not None else 0
    payload[7] = 1 if mailbox is not None and mailbox.find("EoE") is not None else 0
    flags = 0x04 if mailbox is not None and mailbox.attrib.get("DataLinkLayer") == "1" else 0
    if (dev.findtext("./Info/IdentificationReg134") or "").strip().lower() == "true":
        flags |= 0x10
        struct.pack_into("<H", payload, 0x12, 0x0134)
    payload[11] = flags
    payload[14] = payload[0]

    physics = dev.attrib.get("Physics", "")
    phys_word = 0
    for i, port in enumerate(physics[:4]):
        phys_word |= (1 if port == "Y" else 0) << (i * 4)
    struct.pack_into("<H", payload, 0x10, phys_word)
    return bytes(payload)


def build_fmmu(dev):
    values = [fmmu_value(node.text) for node in dev.findall("Fmmu")]
    if not values:
        return b""
    while len(values) % 2:
        values.append(0xFF)
    return bytes(values)


def build_syncm(dev):
    data = bytearray()
    for sm in dev.findall("Sm"):
        name = sm.text or ""
        data.extend(struct.pack(
            "<HHBBBB",
            parse_int(sm.attrib.get("StartAddress")),
            parse_int(sm.attrib.get("DefaultSize") or sm.attrib.get("MinSize")),
            parse_int(sm.attrib.get("ControlByte")),
            0,
            parse_int(sm.attrib.get("Enable")),
            sm_type(name),
        ))
    return bytes(data)


def find_default_modules(root, dev):
    modules = []
    slots = dev.find("Slots")
    if slots is None:
        return modules
    slot_index_increment = parse_int(slots.attrib.get("SlotIndexIncrement"), 0)
    slot_pdo_increment = parse_int(slots.attrib.get("SlotPdoIncrement"), 0)
    module_by_id = {}
    for module in root.findall("./Descriptions/Modules/Module"):
        typ = module.find("Type")
        if typ is not None and typ.attrib.get("ModuleIdent"):
            module_by_id[parse_int(typ.attrib.get("ModuleIdent"))] = module
    for slot_number, slot in enumerate(slots.findall("Slot")):
        module_ident = slot.find("ModuleIdent")
        if module_ident is None or module_ident.attrib.get("Default") != "1":
            continue
        module = module_by_id.get(parse_int(module_ident.text))
        if module is not None:
            modules.append((module, slot_number * slot_index_increment, slot_number * slot_pdo_increment))
    return modules


def add_pdo_strings(sources, strings):
    for source, _, _ in sources:
        for pdo in list(source.findall("RxPdo")) + list(source.findall("TxPdo")):
            strings.add(pdo.findtext("Name") or "")
            for entry in pdo.findall("Entry"):
                strings.add(entry.findtext("Name") or "")


def adjusted_index(node, attr, index_offset):
    value = parse_int(node.attrib.get(attr) or node.findtext(attr))
    if node.find(attr) is not None and node.find(attr).attrib.get("DependOnSlot") == "true":
        value += index_offset
    return value


def build_pdo_category(sources, tag, sm_default, strings):
    data = bytearray()
    for source, index_offset, pdo_offset in sources:
        for pdo in source.findall(tag):
            entries = pdo.findall("Entry")
            data.extend(struct.pack(
                "<HBBBBH",
                adjusted_index(pdo, "Index", pdo_offset),
                len(entries),
                parse_int(pdo.attrib.get("Sm"), sm_default),
                0,
                strings.add(pdo.findtext("Name") or ""),
                0,
            ))
            for entry in entries:
                data.extend(struct.pack(
                    "<HBBBBH",
                    adjusted_index(entry, "Index", index_offset),
                    parse_int(entry.attrib.get("SubIndex") or entry.findtext("SubIndex")),
                    strings.add(entry.findtext("Name") or ""),
                    0,
                    parse_int(entry.attrib.get("BitLen") or entry.findtext("BitLen")),
                    0,
                ))

    return bytes(data)


def build_image(xml_path, product_code, revision_no, output_path):
    root = ET.parse(xml_path).getroot()
    vendor_id = parse_int(root.findtext("./Vendor/Id"))
    dev = find_device(root, product_code, revision_no)
    typ = dev.find("Type")
    eeprom = dev.find("Eeprom")
    byte_size = parse_int(eeprom.findtext("ByteSize"), 2048) if eeprom is not None else 2048
    buf = bytearray([0xFF] * byte_size)
    for i in range(0x80):
        buf[i] = 0

    config = bytes.fromhex(eeprom.findtext("ConfigData")) if eeprom is not None and eeprom.findtext("ConfigData") else b""
    buf[:len(config)] = config
    put_u32(buf, 0x10, vendor_id)
    put_u32(buf, 0x14, product_code)
    put_u32(buf, 0x18, revision_no)
    put_u32(buf, 0x1C, 0)

    bootstrap = bytes.fromhex(eeprom.findtext("BootStrap")) if eeprom is not None and eeprom.findtext("BootStrap") else b""
    buf[0x28:0x28 + len(bootstrap)] = bootstrap

    sms = dev.findall("Sm")
    if len(sms) >= 2:
        put_u16(buf, 0x30, parse_int(sms[0].attrib.get("StartAddress")))
        put_u16(buf, 0x32, parse_int(sms[0].attrib.get("DefaultSize") or sms[0].attrib.get("MinSize")))
        put_u16(buf, 0x34, parse_int(sms[1].attrib.get("StartAddress")))
        put_u16(buf, 0x36, parse_int(sms[1].attrib.get("DefaultSize") or sms[1].attrib.get("MinSize")))
    put_u16(buf, 0x38, mailbox_protocol(dev.find("Mailbox")))
    put_u16(buf, 0x7C, max((byte_size * 8 // 1024) - 1, 0))
    put_u16(buf, 0x7E, 1)
    put_u16(buf, 0x0E, sii_crc(buf))

    pdo_sources = [(dev, 0, 0)] + find_default_modules(root, dev)
    strings = StringTable()
    build_general(dev, strings)
    add_pdo_strings(pdo_sources, strings)

    categories = bytearray()
    categories.extend(category(CAT_STRINGS, strings.payload()))
    categories.extend(category(CAT_GENERAL, build_general(dev, strings)))
    fmmu = build_fmmu(dev)
    if fmmu:
        categories.extend(category(CAT_FMMU, fmmu))
    syncm = build_syncm(dev)
    if syncm:
        categories.extend(category(CAT_SYNCM, syncm))
    rxpdo = build_pdo_category(pdo_sources, "RxPdo", 2, strings)
    if rxpdo:
        categories.extend(category(CAT_RXPDO, rxpdo))
    txpdo = build_pdo_category(pdo_sources, "TxPdo", 3, strings)
    if txpdo:
        categories.extend(category(CAT_TXPDO, txpdo))
    categories.extend(struct.pack("<H", CAT_END))

    end = 0x80 + len(categories)
    if end > len(buf):
        raise SystemExit(f"EEPROM image too small: need {end} bytes, have {len(buf)} bytes")
    buf[0x80:end] = categories

    with open(output_path, "wb") as fh:
        fh.write(buf)

    print(f"wrote {output_path}")
    print(f"  device        : {typ.text if typ is not None else ''}")
    print(f"  vendor        : 0x{vendor_id:08x}")
    print(f"  product       : 0x{product_code:08x}")
    print(f"  revision      : 0x{revision_no:08x}")
    print(f"  checksum      : 0x{sii_crc(buf):02x}")
    print(f"  std mailbox   : rx=0x{struct.unpack_from('<H', buf, 0x30)[0]:04x}/{struct.unpack_from('<H', buf, 0x32)[0]} tx=0x{struct.unpack_from('<H', buf, 0x34)[0]:04x}/{struct.unpack_from('<H', buf, 0x36)[0]}")
    print(f"  mbx protocol  : 0x{struct.unpack_from('<H', buf, 0x38)[0]:04x}")
    print(f"  default modules: {len(pdo_sources) - 1}")
    print(f"  categories end: 0x{end:04x}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("xml")
    parser.add_argument("-p", "--product-code", type=lambda x: int(x, 0), required=True)
    parser.add_argument("-r", "--revision-no", type=lambda x: int(x, 0), required=True)
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()
    build_image(args.xml, args.product_code, args.revision_no, args.output)


if __name__ == "__main__":
    main()
