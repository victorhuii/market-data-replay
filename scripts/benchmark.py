"""Fresh-process repeated synthetic replay evidence; no production-feed claim."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import sys
from fixtures import workload,capture
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tests'))
from reference import decode


def main():
    p=argparse.ArgumentParser();p.add_argument('--count',type=int,default=20000);p.add_argument('--repetitions',type=int,default=3);p.add_argument('--output',type=Path,default=ROOT/'docs/benchmark.json');a=p.parse_args()
    if not 1<=a.repetitions<=10:raise ValueError('Repetitions must be 1..10')
    result=dict(schema_version=1,synthetic=True,environment=dict(platform=platform.platform(),python=platform.python_version(),compiler=subprocess.run(['c++','--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0]),source_sha256=hashlib.sha256(b''.join(p.read_bytes() for p in sorted((ROOT/'src').glob('*')))).hexdigest(),timing_scope='C++ elapsed includes capture reads, bounds/sequence checks, event decoding and staged map copies; excludes fixture generation, checkpoint serialization and output. Fresh process per repetition; no affinity or cold-cache control.',profiles=[])
    identities={}
    for depth in (100,3000):
        for batch in (1,32):
            packets=workload(a.count,depth,batch);path=capture(ROOT/'runtime'/f'd{depth}-b{batch}.mdr',packets);expected=decode(packets)
            full=json.loads(subprocess.run([str(ROOT/'build/replay'),'--input',str(path)],capture_output=True,text=True,check=True).stdout)
            for k in expected:
                if full[k]!=expected[k]:raise ValueError('Independent reference disagreement')
            samples=[]
            for i in range(a.repetitions):
                row=json.loads(subprocess.run([str(ROOT/'build/replay'),'--input',str(path),'--summary'],capture_output=True,text=True,check=True).stdout)
                if row['new_messages']!=a.count or row['state_fnv64']!=full['state_fnv64']:raise ValueError('Repeated replay diverged')
                samples.append(row)
            if depth in identities and identities[depth]!=full['state_fnv64']:raise ValueError('Packetization changed final state')
            identities[depth]=full['state_fnv64'];rates=[s['messages_per_second'] for s in samples]
            result['profiles'].append(dict(target_active_depth=depth,max_messages_per_packet=batch,count=a.count,capture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),independent_reference_equal=True,samples=samples,median_messages_per_second=statistics.median(rates),minimum_messages_per_second=min(rates),maximum_messages_per_second=max(rates)))
            print(f'depth {depth} batch {batch}: median {statistics.median(rates):.0f} messages/s',flush=True)
    a.output.parent.mkdir(exist_ok=True,parents=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
