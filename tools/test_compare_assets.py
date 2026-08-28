import struct
import unittest
from extract_compare_assets import BASE, HEADERS, STARTERS, TABLES, extract


def fixture():
    data = bytearray(0xB0000)
    def word(address, value):
        struct.pack_into('<I', data, address-BASE, value)
    def text(address):
        data[address-BASE:address-BASE+10] = b'Synthetic\0'
    word(0x800A5360+0x34, 10)
    word(0x8009AF40+40, 0x8009D34C)
    for p in HEADERS+STARTERS:
        text(p)
    for i in range(4):
        word(0x8009D34C+i*4, 0x800B4000+i*16)
        text(0x800B4000+i*16)
    word(0x8009D598+31*4, 0x800B4100)
    text(0x800B4100)
    for address, count in TABLES:
        for i in range(count):
            word(address+i*8, 0x800B4100)
            word(address+i*8+4, i)
    return data


class CompareExtraction(unittest.TestCase):
    def test_synthetic_pack_and_determinism(self):
        a=extract(fixture())
        self.assertEqual(a[:8],b'N97C\1\0\r\0')
        self.assertEqual(a,extract(fixture()))
        self.assertLess(len(a),8192)

    def test_truncated_overlay(self):
        for n in (0,64,0x80000):
            with self.assertRaises(ValueError): extract(fixture()[:n])

    def test_wrong_table_route(self):
        a=fixture(); struct.pack_into('<I',a,0x800A5360+0x34-BASE,11)
        with self.assertRaises(ValueError): extract(a)

    def test_bad_pointer_and_field(self):
        for offset,value in ((0,0),(4,67),(0,0xFFFFFFFF)):
            a=fixture(); struct.pack_into('<I',a,TABLES[0][0]-BASE+offset,value)
            with self.assertRaises(ValueError): extract(a)

    def test_string_controls_and_termination(self):
        a=fixture(); a[HEADERS[0]-BASE]=31
        with self.assertRaises(ValueError): extract(a)
        a=fixture(); a[0x800B4100-BASE:0x800B4100-BASE+129]=b'x'*129
        with self.assertRaises(ValueError): extract(a)


if __name__=='__main__': unittest.main()
