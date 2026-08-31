"""Synthetic format and privacy guards; no original game bytes are used."""
import importlib.util
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('music_extract', Path(__file__).resolve().parents[1]/'tools/extract_music_resources.py')
music = importlib.util.module_from_spec(spec)
spec.loader.exec_module(music)


def synthetic_cnk():
    data = bytearray(128)
    data[:4] = b'SCHl'
    struct.pack_into('<I', data, 4, 128)
    data[12:16] = b'PATl'
    data[68:72] = b'TMxl'
    data[72:76] = bytes((0, 16, 2, 6))
    struct.pack_into('<H', data, 78, 44100)
    struct.pack_into('<I', data, 84, 28)
    return data + b'SCDl'+struct.pack('<I', 48)+bytes(40)+b'SCEl'+struct.pack('<I', 8)


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.private = self.root/'private'
        self.private.mkdir()
        self.disc, self.overlay = self.root/'disc.bin', self.root/'overlay.bin'
        self.output = self.private/'audio'
        cnk = synthetic_cnk()
        names = ('ZTMENU1.CNK', 'ZTMENU2.CNK', 'ZTMENU3.CNK', 'ZTMENU4.CNK', 'ZTPAUSE.CNK')
        slots = b''.join((names[(i*3)%5].encode()+bytes(13-len(names[(i*3)%5]))) for i in range(16))
        data = bytearray(6*2352)
        tracks = []
        for i, name in enumerate(names, 1):
            data[i*2352+24:i*2352+24+len(cnk)] = cnk
            tracks.append((name, i, len(cnk), music.digest(cnk)))
        self.disc.write_bytes(data)
        self.overlay.write_bytes(slots)
        p = patch.multiple(music, PRIVATE=self.private, DISC_SIZE=len(data), DISC_SHA256=music.digest(data),
                           OVERLAY_SHA256=music.digest(slots), SLOT_OFFSET=0, SLOT_SHA256=music.digest(slots), TRACKS=tracks)
        p.start()
        self.addCleanup(p.stop)

    def extract(self):
        return music.extract_music(self.disc, self.overlay, self.output)

    def test_extract_and_idempotent_preservation(self):
        result = self.extract()
        self.assertEqual(len(result['tracks']), 5)
        self.assertEqual(result['slots'][1], 'ZTMENU4.CNK')
        self.assertEqual(result['tracks'][0]['sample_frames'], 28)
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.output.iterdir()}
        self.assertEqual(self.extract(), result)
        self.assertEqual({p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.output.iterdir()}, before)

    def test_source_identity_before_output(self):
        for source in (self.disc, self.overlay):
            old = source.read_bytes()
            source.write_bytes(bytes([old[0]^1])+old[1:])
            with self.assertRaisesRegex(ValueError, 'SHA256'):
                self.extract()
            self.assertFalse(self.output.exists())
            source.write_bytes(old)

    def test_non_private_output_refused(self):
        self.output = self.root/'public'
        with self.assertRaisesRegex(ValueError, 'repository .local'):
            self.extract()
        self.assertFalse(self.output.exists())

    def test_source_output_hardlink_refused(self):
        self.output.mkdir()
        target = self.output/'ZTMENU1.CNK'
        os.link(self.disc, target)
        before = self.disc.read_bytes()
        with self.assertRaisesRegex(ValueError, 'distinct'):
            self.extract()
        self.assertEqual(self.disc.read_bytes(), before)
        self.assertEqual(len(list(self.output.iterdir())), 1)

    def test_output_output_hardlink_refused(self):
        self.output.mkdir()
        first = self.output/'ZTMENU1.CNK'
        first.write_bytes(synthetic_cnk())
        os.link(first, self.output/'ZTMENU2.CNK')
        with self.assertRaisesRegex(ValueError, 'distinct'):
            self.extract()

    def test_existing_conflict_is_not_partly_published(self):
        self.output.mkdir()
        (self.output/'music_routing.json').write_bytes(b'preserve prior evidence')
        with self.assertRaisesRegex(ValueError, 'existing music output differs'):
            self.extract()
        self.assertEqual(list(p.name for p in self.output.iterdir()), ['music_routing.json'])
        self.assertEqual((self.output/'music_routing.json').read_bytes(), b'preserve prior evidence')

    def test_cnk_format_failures(self):
        base = synthetic_cnk()
        variants = []
        bad = bytearray(base); bad[75] = 7; variants.append(bad)
        bad = bytearray(base); struct.pack_into('<I', bad, 132, 47); variants.append(bad)
        bad = bytearray(base); struct.pack_into('<I', bad, 84, 29); variants.append(bad)
        variants += [base[:-8], base+b'extra', base[:-1]]
        for bad in variants:
            with self.subTest(length=len(bad)), self.assertRaises(ValueError):
                music.inspect_cnk(bad)


if __name__ == '__main__':
    unittest.main()
