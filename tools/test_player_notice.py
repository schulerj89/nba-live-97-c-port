import unittest
from extract_player_notice import extract, ADDRESS, HEADER

class PlayerNoticeExtraction(unittest.TestCase):
    def fixture(self):
        data=bytearray(ADDRESS-0x80015000+64)
        at=ADDRESS-0x80015000
        body=HEADER+b'\1A\0\1B\0'
        data[at:at+len(body)]=body
        prompt=0x8002502C-0x80015000
        data[prompt:prompt+2]=b'C\0'
        return data,body+b'\1C\0'
    def test_exact_source_records(self):
        data,expected=self.fixture()
        self.assertEqual(extract(data),expected)
    def test_rejects_bad_header_body_and_footer(self):
        data,_=self.fixture()
        for at in (ADDRESS-0x80015000+7,ADDRESS-0x80015000+10,0x8002502C-0x80015000):
            bad=data.copy();bad[at]=0x1f
            with self.assertRaises(ValueError):extract(bad)
        with self.assertRaises(ValueError):extract(data[:20])

if __name__=='__main__':unittest.main()
