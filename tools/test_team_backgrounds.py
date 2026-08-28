import struct
import unittest
from decode_team_backgrounds import indexed_pack, PALETTE_TAGS


class IndexedPackTests(unittest.TestCase):
    def fixtures(self):
        palettes = [(tag, bytes([i, 128])*160) for i, tag in enumerate(PALETTE_TAGS)]
        strips = [(bytes(range(256))*120, bytes([160+i, 0])*96) for i in range(4)]
        return palettes, strips

    def test_raw_preservation(self):
        palettes, strips = self.fixtures()
        out = indexed_pack(palettes, strips)
        self.assertEqual(len(out), 134356)
        self.assertEqual(struct.unpack_from('<4s6H', out), (b'N97P', 1, 4, 33, 128, 240, 0))
        at = 16
        for tag, palette in palettes:
            self.assertEqual(out[at:at+324], tag.encode()+palette)
            at += 324
        for indices, local in strips:
            self.assertEqual(out[at:at+30912], indices+local)
            at += 30912
        self.assertEqual(at, len(out))

    def test_reject_bad_records(self):
        palettes, strips = self.fixtures()
        for bad in [palettes[:-1], palettes[::-1], [(palettes[0][0], b'')]+palettes[1:]]:
            with self.assertRaises(ValueError): indexed_pack(bad, strips)
        for bad in [strips[:-1], [(b'', strips[0][1])]+strips[1:], [(strips[0][0], b'')]+strips[1:]]:
            with self.assertRaises(ValueError): indexed_pack(palettes, bad)


if __name__ == '__main__': unittest.main()
