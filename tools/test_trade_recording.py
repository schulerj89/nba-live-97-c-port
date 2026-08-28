"""Synthetic adversarial cases for the bounded live Trade evidence validator."""
import copy
import unittest
from inspect_trade_recording import validate_selection_cancel


class TradeRecordingTests(unittest.TestCase):
    def setUp(self):
        base = dict(boot=4,page=9,team=2,phase=0,child=0,help=0,cursor0=0,cursor1=0,
                    top0=0,top1=0,player0=24,player1=300,transition=0)
        down = dict(base,cursor0=1,player0=25)
        self.rows = [dict(s,index=i,ns=(i+1)*100) for i,s in
                     enumerate((base,base,down,down,dict(down,phase=1),dict(down,phase=1),down,down))]
        self.inputs = []
        for frame,code in ((2,40),(4,67),(6,88)):
            self.inputs += [dict(ns=frame*100+10,next_frame=frame,message=256,code=code,data=1),
                            dict(ns=frame*100+20,next_frame=frame,message=257,code=code,data=0)]

    def test_observed_sequence_is_not_full_roster_or_original_proof(self):
        result=validate_selection_cancel(self.rows,self.inputs)
        self.assertEqual(result['input_frames'],dict(down=2,pick=4,cancel=6))
        for key in ('right_team_verified','full_roster_unchanged_verified','original_parity','timing_parity'):
            self.assertFalse(result[key])

    def test_stale_state_or_wrong_screen(self):
        for field,value in (('page',8),('player0',65535),('child',35),('help',1),('transition',1)):
            rows=copy.deepcopy(self.rows)
            for row in rows: row[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):
                validate_selection_cancel(rows,self.inputs)
        with self.assertRaises(ValueError):
            validate_selection_cancel([dict(self.rows[0],ns=r['ns']) for r in self.rows],self.inputs)

    def test_every_segment_invariant(self):
        for index in range(len(self.rows)):
            for field,value in (('team',5),('player1',301),('cursor1',1),('top0',1),('phase',2)):
                rows=copy.deepcopy(self.rows);rows[index][field]=value
                with self.subTest(index=index,field=field),self.assertRaises(ValueError):
                    validate_selection_cancel(rows,self.inputs)

    def test_missing_extra_or_unreleased_keys(self):
        for i in range(len(self.inputs)):
            with self.subTest(i=i),self.assertRaises(ValueError):
                validate_selection_cancel(self.rows,self.inputs[:i]+self.inputs[i+1:])
        for event in (dict(ns=790,next_frame=8,message=256,code=70,data=1),
                      dict(ns=790,next_frame=8,message=8,code=0,data=0)):
            with self.assertRaises(ValueError):validate_selection_cancel(self.rows,self.inputs+[event])

    def test_boundaries_and_repeat(self):
        for field,value in (('next_frame',0),('next_frame',4),('next_frame',8),('data',1<<30)):
            events=copy.deepcopy(self.inputs);events[0][field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):
                validate_selection_cancel(self.rows,events)


if __name__=='__main__': unittest.main()
