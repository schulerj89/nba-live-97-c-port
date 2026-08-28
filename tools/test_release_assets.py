"""Release private extractor tests with synthetic text, never game assets."""
import struct
import unittest
from test_trade_assets import fixture
from extract_trade_assets import BASE
from extract_frontend_help import TABLE
from extract_release_assets import extract, DIALOGS, ROUTES, LAYOUT


def release_fixture():
    data = fixture()
    struct.pack_into('<I', data, 0x80093330-BASE+17*4, 0x80097104)
    for index,(x,y,tag) in enumerate(LAYOUT):
        at=0x80097104-BASE+index*16
        struct.pack_into('<hh',data,at,y,x)
        data[at+8:at+12]=tag.encode('ascii')
    data[0x8009D83A-BASE:0x8009D83A-BASE+2] = b'F\0'
    for address in DIALOGS:
        record = struct.pack('<hhhBBBB', 121, 80, 270, 80, 1, 1, 0) + b'\1synthetic\0'
        data[address-BASE:address-BASE+len(record)] = record
    state, index, address = ROUTES[0]
    pointer = BASE+0x100+state*8
    struct.pack_into('<I', data, TABLE-BASE+state*4, pointer)
    struct.pack_into('<I', data, pointer-BASE+index*4, address)
    record = struct.pack('<hhhBBBB', 121, 80, 270, 125, 0, 1, 0) + b'\1synthetic\0'
    data[address-BASE:address-BASE+len(record)] = record
    return data


class ReleaseAssetsTests(unittest.TestCase):
    def test_required_records_and_single_stage_help(self):
        ui, help_pack = extract(release_fixture())
        self.assertEqual(ui[:8], b'N97T\1\0\x19\0')
        self.assertEqual(help_pack[:8], b'N97H\1\0\3\0')
        at, addresses = 8, []
        for _ in range(25):
            address, length = struct.unpack_from('<II', ui, at)
            addresses.append(address)
            at += 8+length
        self.assertEqual(at, len(ui))
        self.assertTrue(set(DIALOGS) <= set(addresses))
        self.assertEqual(help_pack[8:10], bytes((17, 0)))

    def test_refusal_descriptor_validation(self):
        for address in DIALOGS:
            for offset, value in ((7, 0), (8, 0), (9, 1), (10, 0)):
                data = release_fixture()
                data[address-BASE+offset] = value
                with self.subTest(address=address, offset=offset), self.assertRaises(ValueError): extract(data)

    def test_wrong_help_route(self):
        data = release_fixture()
        struct.pack_into('<I', data, 0x100+17*8, 0x800B1270)
        with self.assertRaises(ValueError): extract(data)

    def test_wrong_layout_pointer_geometry_and_tag(self):
        for at in (0x80093330-BASE+17*4,0x80097104-BASE+5*16+2,0x80097104-BASE+5*16+8):
            data=release_fixture();data[at]^=1
            with self.subTest(offset=at), self.assertRaises(ValueError): extract(data)

    def test_truncated(self):
        for size in (0, 32, 0x115AC+24, 0x9A52C):
            with self.subTest(size=size), self.assertRaises(ValueError): extract(release_fixture()[:size])


if __name__ == '__main__':
    unittest.main()
