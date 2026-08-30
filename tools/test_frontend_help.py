"""Synthetic extraction fixtures: no original game data is checked in."""
import struct
import unittest
from extract_frontend_help import BASE, TABLE, ROUTES, extract, record_at


def fixture():
    data = bytearray(0xc0000)
    tables = {12: BASE+0x100, 34: BASE+0x120,
              35: BASE+0x140, 36: BASE+0x160}
    for state, index, address in ROUTES:
        struct.pack_into('<I', data, TABLE-BASE+state*4, tables[state])
        struct.pack_into('<I', data, tables[state]-BASE+index*4, address)
        header = struct.pack('<hhhBBBB', 121, 70, 270, 140, 0, 2, 0)
        body = b'\x01SYNTHETIC\0\x00\x15\x9a\x9b\x1f\x12test\0'
        data[address-BASE:address-BASE+len(header+body)] = header+body
    return data


class HelpExtractionTests(unittest.TestCase):
    def test_routes_and_encoded_payload(self):
        data = fixture()
        result = extract(data)
        self.assertEqual(result[:8], b'N97H\x01\0\x05\0')
        self.assertIn(b'\x9a\x9b\x1f\x12test', result)
        self.assertEqual(result.count(b'SYNTHETIC'), 5)

    def test_pointer_bounds_and_wrong_route(self):
        for pointer in (0, 0xffffffff, BASE+1):
            data = fixture()
            struct.pack_into('<I', data, TABLE-BASE+12*4, pointer)
            with self.assertRaises(ValueError):
                extract(data)

    def test_descriptor_bounds(self):
        for address in (BASE-1, BASE+0xc0000, BASE+0xc0000-9):
            with self.assertRaises(ValueError):
                record_at(fixture(), address)

    def test_bad_descriptor(self):
        at = ROUTES[0][2]-BASE
        for offset, value in ((0,255), (7,1), (8,0), (8,17), (9,1), (10,2)):
            data = fixture()
            data[at+offset] = value
            with self.assertRaises(ValueError):
                extract(data)

    def test_unterminated_and_unsupported_controls(self):
        at = ROUTES[0][2]-BASE
        data = fixture()
        data[at+11:at+300] = b'A'*289
        with self.assertRaises(ValueError):
            extract(data)
        for control in (0x1e, 1, 10):
            data = fixture()
            data[at+11] = control
            with self.assertRaises(ValueError):
                extract(data)
        data = fixture()
        data[at+11:at+13] = b'\x1f\0'
        with self.assertRaises(ValueError):
            extract(data)


if __name__ == '__main__':
    unittest.main()
