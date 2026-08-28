import unittest
import extract_roster_reset as reset

class ResetExtraction(unittest.TestCase):
    def fixture(self):
        prefix=bytearray(reset.ADDRESS-0x80015000)
        record=reset.HEADER+b''.join(b'\x01fixture\0' for _ in range(7))
        return prefix+record,record
    def test_extract_exact_private_record(self):
        data,record=self.fixture()
        self.assertEqual(reset.extract(data+b'other data'),record)
    def test_bad_header_line_and_truncation(self):
        data,_=self.fixture()
        start=reset.ADDRESS-0x80015000
        for i in (start,start+7,start+10):
            changed=data[:];changed[i]^=3
            with self.assertRaises(ValueError): reset.extract(changed)
        for length in range(start,len(data)):
            with self.assertRaises(ValueError): reset.extract(data[:length])

if __name__=='__main__':
    unittest.main()
