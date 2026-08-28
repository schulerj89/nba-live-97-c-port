"""Synthetic Trade extraction fixtures; no game bytes committed."""
import struct
import unittest
from extract_trade_assets import BASE,TEXTS,DIALOGS,HELP,extract
from extract_frontend_help import extract as extract_help,TABLE

def fixture():
    data=bytearray(0xc0000)
    for a in TEXTS:data[a-BASE:a-BASE+2]=b'T\0'
    for a in DIALOGS:
        choices=2 if a in (0x800AF4F8,0x800AEE88,0x800AEEF6) else 0
        record=struct.pack('<hhhBBBB',121,70,270,100,1,1,choices)+b'\1test\0'*(1+choices)
        data[a-BASE:a-BASE+len(record)]=record
    for state,index,a in HELP:
        pointer=BASE+0x100+state*8
        struct.pack_into('<I',data,TABLE-BASE+state*4,pointer)
        struct.pack_into('<I',data,pointer-BASE+index*4,a)
        record=struct.pack('<hhhBBBB',121,70,270,140,0,1,0)+b'\1test\0'
        data[a-BASE:a-BASE+len(record)]=record
    return data

class TradeAssetsTests(unittest.TestCase):
    def test_sign_routes_and_required_records(self):
        data=fixture()
        data[0x8009D83A-BASE:0x8009D83A-BASE+5]=b'free\0'
        for a in (0x800AED20,0x800AEC72,0x800AED88):
            record=struct.pack('<hhhBBBB',121,70,270,100,1,1,0)+b'\1test\0'
            data[a-BASE:a-BASE+len(record)]=record
        packed=extract(data,True)
        self.assertEqual(packed[:8],b'N97T\1\0\x19\0')
        at=8;addresses=[]
        for _ in range(25):
            a,n=struct.unpack_from('<II',packed,at);addresses.append(a);at+=8+n
        self.assertEqual(at,len(packed))
        self.assertTrue({0x8009D83A,0x800AED20,0x800AEC72,0x800AED88}<=set(addresses))
        data[0x800AED88-BASE+7]=0
        with self.assertRaises(ValueError):extract(data,True)

    def test_routes(self):
        data=fixture();self.assertEqual(extract(data)[:8],b'N97T\1\0\x15\0')
        self.assertEqual(extract_help(data,HELP)[:8],b'N97H\1\0\4\0')
    def test_truncated(self):
        for size in (0,0x1000,0x115ac,0x115ac+24,0x99eee):
            with self.assertRaises(ValueError):extract(fixture()[:size])
    def test_invalid_preference(self):
        data=fixture();data[0x800265AC-BASE]=5
        with self.assertRaises(ValueError):extract(data)
    def test_dialog_controls_and_geometry(self):
        for offset,value in ((0,255),(7,0),(8,0),(8,9),(9,1),(10,0)):
            data=fixture();data[DIALOGS[0]-BASE+offset]=value
            with self.assertRaises(ValueError):extract(data)
    def test_text_bounds(self):
        for content in (b'\x1f\0',b'A'*258):
            data=fixture();at=TEXTS[0]-BASE;data[at:at+len(content)]=content
            with self.assertRaises(ValueError):extract(data)

if __name__=='__main__':unittest.main()
