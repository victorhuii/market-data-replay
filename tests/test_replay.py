import json
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fixtures import add,reduce,replace,common,packet,capture,workload
from reference import decode

ROOT=Path(__file__).resolve().parents[1];BINARY=ROOT/os.environ.get('REPLAY_BINARY','build/replay')

class ReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        flags=['-std=c++20','-O1','-g','-Wall','-Wextra','-Werror']
        if 'sanitize' in str(BINARY):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(['c++',*flags,'tests/atomic.cpp','-o','build/atomic'],cwd=ROOT,check=True,capture_output=True)

    def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
    def tearDown(self):self.temp.cleanup()
    def run_capture(self,packets,*args,success=True):
        p=capture(self.root/'capture.mdr',packets);r=subprocess.run([str(BINARY),'--input',str(p),*map(str,args)],capture_output=True,text=True,timeout=30)
        if success:self.assertEqual(r.returncode,0,r.stderr);return json.loads(r.stdout)
        self.assertEqual(r.returncode,2,r.stderr);return r

    def test_all_supported_order_events_against_reference(self):
        msgs=[common('S',0)+b'O',add(2,'S',80,1010000,mpid='FIRM'),add(1),reduce('E',1,20),reduce('X',1,10),replace(2,3,40,1005000),reduce('C',3,5),add(4,stock='NEXT',locate=2),reduce('D',4,locate=2)]
        packets=[packet(1,msgs[:4]),packet(5,msgs[4:]),packet(10,count=65535)];actual=self.run_capture(packets);expected=decode(packets)
        for key in expected:self.assertEqual(actual[key],expected[key]);self.assertEqual(actual['new_messages'],9)
        self.assertEqual(actual['orders'][1]['mpid_hex'],b'FIRM'.hex())

    def test_random_streams_independent_reference(self):
        for seed in range(5):
            packets=workload(1300,75,17,seed);actual=self.run_capture(packets);expected=decode(packets)
            for key in expected:self.assertEqual(actual[key],expected[key])

    def test_retransmission_overlap_and_checkpoint_restart(self):
        events=[add(1),add(2),reduce('E',1,5),replace(2,3),reduce('D',3)];packets=[packet(1,events[:3]),packet(2,events[1:]),packet(2,events[1:])]
        whole=self.run_capture(packets);self.assertEqual(whole['new_messages'],5);self.assertEqual(whole['orders'],decode(packets)['orders'])
        chk=self.root/'state.chk';self.run_capture(packets[:1],'--save',chk)
        resumed=self.run_capture(packets[1:],'--load',chk);self.assertEqual(resumed['state_fnv64'],whole['state_fnv64']);self.assertEqual(resumed['new_messages'],2)

    def test_completed_capture_checkpoint_is_idempotent(self):
        packets=workload(200,30,17);chk=self.root/"complete.chk"
        first=self.run_capture(packets,"--save",chk);second=self.run_capture(packets,"--load",chk)
        self.assertEqual(first["state_fnv64"],second["state_fnv64"]);self.assertEqual(second["new_messages"],0)

    def test_gap_session_switch_and_ended_session(self):
        for packets in ([packet(1,[add(1)]),packet(3,[add(2)])],[packet(1,[add(1)]),packet(2,[add(2)],session=b'OTHER00001')],[packet(1,count=65535),packet(1,[add(1)])]):self.run_capture(packets,success=False)
        actual=self.run_capture([packet(1),packet(1,[add(1)]),packet(2),packet(2,count=65535)]);self.assertTrue(actual['ended']);self.assertEqual(actual['next_sequence'],2)

    def test_binary_truncation_lengths_and_unsupported_type(self):
        p=packet(1,[add(1)])
        for end in range(len(p)):
            self.run_capture([p[:end]],success=False)
        for malformed in [p+b'x',packet(1,[add(1)[:-1]]),packet(1,[b'Z'+bytes(10)]),packet(1,[])+b'x',packet(2**64-1,[add(1)]),packet(0,[add(1)])]:self.run_capture([malformed],success=False)

    def test_semantic_rejections_are_packet_atomic_and_can_recover(self):
        valid=packet(1,[add(1)]);bad_events=[add(1),add(2,side='Q'),add(2,quantity=0),add(2,price=2000000001),add(2,stock='BAD!'),reduce('E',1,101),reduce('X',99),reduce('D',1,locate=2),replace(1,1),replace(1,2,quantity=0),common('S',0)+b'Z',add(2,stock='DIFF')]
        packets=[valid]
        # Each failed packet starts with a valid mutation: it must be rolled back.
        for bad in bad_events:packets.append(packet(2,[add(10,locate=2,stock='OTHER'),bad]))
        packets.extend([packet(3,[add(4)]),packet(2,[reduce('E',1,5)]),packet(3,[add(4)])])
        data=capture(self.root/'atomic.mdr',packets).read_bytes();r=subprocess.run([str(ROOT/'build/atomic')],input=data,capture_output=True,timeout=30)
        self.assertEqual(r.returncode,0,r.stderr);result=json.loads(r.stdout);self.assertEqual(result,dict(accepted=3,rejected=len(bad_events)+1,next_sequence=4,orders=2))

    def test_checkpoint_integrity_and_bad_save_does_not_replace_state(self):
        chk=self.root/'state.chk';self.run_capture([packet(1,[add(1)])],'--save',chk);original=chk.read_bytes()
        self.run_capture([packet(3,[add(2)])],'--load',chk,'--save',chk,success=False);self.assertEqual(chk.read_bytes(),original)
        for mutated in [original[:-1],original.replace(b'MDR1',b'MDR2'),original+b'extra\n']:
            chk.write_bytes(mutated);self.run_capture([packet(2)],'--load',chk,success=False)

    def test_framing_limits_and_active_order_bound(self):
        for raw in [b'\x00',struct.pack('>I',65508),struct.pack('>I',25)+b'x'*20]:
            p=self.root/'bad.mdr';p.write_bytes(raw);r=subprocess.run([str(BINARY),'--input',str(p)],capture_output=True);self.assertEqual(r.returncode,2)
        events=[add(i+1) for i in range(10001)];packets=[packet(i+1,events[i:i+100]) for i in range(0,len(events),100)]
        self.run_capture(packets,success=False)

    def test_seeded_malformed_packet_smoke(self):
        rng=random.Random(42);packets=[bytes(rng.randrange(256) for _ in range(rng.randrange(0,100))) for i in range(100)]
        r=subprocess.run([str(ROOT/'build/atomic')],input=capture(self.root/'fuzz.mdr',packets).read_bytes(),capture_output=True,timeout=30)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(json.loads(r.stdout)['rejected'],100)
