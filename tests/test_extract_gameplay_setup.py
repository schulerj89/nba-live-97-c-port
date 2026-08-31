import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import extract_gameplay_setup as extract

class SetupExtractionTests(unittest.TestCase):
    def test_table_windows_and_wire_layout(self):
        game=bytearray(1009196)
        for i in range(len(game)):game[i]=(i*17+23)&255
        pack=extract.build_period_pack(game)
        self.assertEqual(pack[:8],b'NBA97PER')
        self.assertEqual(len(pack),2132)
        self.assertEqual(struct.unpack_from('<III',pack,8),(1,2112,zlib.crc32(pack[20:])))
        at=0xb891c-0x15000
        self.assertEqual(pack[20:84],game[at:at+64])
        for table,address in enumerate([0xb895c,0xb8970]):
            self.assertEqual(pack[84+table*1024:84+(table+1)*1024],game[address-0x15000:address-0x15000+1024])
        with self.assertRaises(ValueError):extract.build_period_pack(game[:-1])

    def run_synthetic(self,folder):
        disc=folder/'disc.bin'
        with disc.open('wb') as stream:stream.truncate(726939696)
        overlay=folder/'game.bin';game=bytes(1009196);overlay.write_bytes(game)
        output=folder/'out';motion=b'synthetic motion bytes'
        patches=[patch.object(extract,'PRIVATE',folder),patch.object(extract,'GAME_SHA',hashlib.sha256(game).hexdigest()),
                 patch.object(extract,'MOCAP_SHA',hashlib.sha256(motion).hexdigest()),patch.object(extract,'read_extent',return_value=motion),
                 patch.object(extract.hashlib,'file_digest')]
        from contextlib import ExitStack
        with ExitStack() as stack:
            mocks=[stack.enter_context(p) for p in patches]
            mocks[-1].return_value.hexdigest.return_value=extract.DISC_SHA
            extract.extract(disc,overlay,output)
            before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in output.iterdir()}
            extract.extract(disc,overlay,output)
            self.assertEqual(before,{p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in output.iterdir()})
            (output/'period_setup.bin').write_bytes(b'keep existing conflict')
            with self.assertRaisesRegex(ValueError,'differs'):extract.extract(disc,overlay,output)
            self.assertEqual((output/'period_setup.bin').read_bytes(),b'keep existing conflict')
            with self.assertRaisesRegex(ValueError,'below'):extract.extract(disc,overlay,folder.parent/'outside')
            with self.assertRaisesRegex(ValueError,'alias'):extract.extract(overlay,overlay,output)

    def test_preserves_existing_and_refuses_escapes_aliases(self):
        with tempfile.TemporaryDirectory(prefix='setup-extractor-test-') as name:
            self.run_synthetic(Path(name))

if __name__=='__main__':unittest.main()
