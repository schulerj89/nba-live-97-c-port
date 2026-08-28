"""Synthetic checks only: no original portraits or palette bytes in fixtures."""
import struct
import unittest

from decode_reorder_portraits import restore_texture_alpha


class TextureAlphaTests(unittest.TestCase):
    def test_zero_word_not_zero_index(self):
        palette = [0x1234] * 256
        palette[0], palette[137], palette[255] = 0x8000, 0, 0
        indices = bytes([0, 137, 255, 42])
        rgba = bytes([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 8, 16, 24, 0])
        result = restore_texture_alpha(rgba, indices, struct.pack('<256H', *palette))
        self.assertEqual(result[3::4], bytes([255, 0, 0, 255]))
        self.assertEqual(result[0::4], rgba[0::4])
        self.assertEqual(result[1::4], rgba[1::4])
        self.assertEqual(result[2::4], rgba[2::4])

    def test_all_palette_words_and_immutable_inputs(self):
        indices = bytes(range(256))
        rgba = bytearray([17, 33, 49, 127] * 256)
        original = bytes(rgba)
        for base in range(0, 65536, 256):
            words = list(range(base, base + 256))
            palette = struct.pack('<256H', *words)
            result = restore_texture_alpha(rgba, indices, palette)
            self.assertEqual(result[3::4], bytes(0 if word == 0 else 255 for word in words))
            self.assertEqual(bytes(rgba), original)

    def test_padding_and_bad_extents(self):
        palette = struct.pack('<256H', *([0x8000] * 255 + [0]))
        indices = bytes([0] * 87 + [255]) * 51
        rgba = bytes([0, 0, 0, 255]) * len(indices)
        result = restore_texture_alpha(rgba, indices, palette)
        self.assertEqual(result[3::4], bytes([255] * 87 + [0]) * 51)
        for bad_palette in (b'', palette[:-1], palette + b'\x00'):
            with self.assertRaises(ValueError):
                restore_texture_alpha(rgba, indices, bad_palette)
        with self.assertRaises(ValueError):
            restore_texture_alpha(rgba[:-4], indices, palette)


if __name__ == '__main__':
    unittest.main()
