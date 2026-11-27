"""Owned synthetic ITCH subset fixtures; no licensed feed data."""
from pathlib import Path
import random
import struct

SESSION=b'DEMO000001'

def common(kind,locate=1,timestamp=1):return kind.encode()+struct.pack('>HH',locate,0)+timestamp.to_bytes(6,'big')
def add(order,side='B',quantity=100,price=1000000,stock='DEMO',locate=1,mpid=None):
    return common('F' if mpid else 'A',locate)+struct.pack('>QcI8sI',order,side.encode(),quantity,stock.encode().ljust(8,b' '),price)+(mpid.encode().ljust(4,b' ') if mpid else b'')
def reduce(kind,order,quantity=1,locate=1):
    payload=common(kind,locate)+struct.pack('>Q',order)
    if kind!='D':payload+=struct.pack('>I',quantity)
    if kind in ('E','C'):payload+=struct.pack('>Q',order+1000000)
    if kind=='C':payload+=b'N'+struct.pack('>I',1000100)
    return payload
def replace(old,new,quantity=20,price=999000,locate=1):return common('U',locate)+struct.pack('>QQII',old,new,quantity,price)
def packet(sequence,messages=(),session=SESSION,count=None):
    count=len(messages) if count is None else count
    return session+struct.pack('>QH',sequence,count)+b''.join(struct.pack('>H',len(m))+m for m in messages)
def capture(path,packets):
    Path(path).parent.mkdir(parents=True,exist_ok=True);Path(path).write_bytes(b''.join(struct.pack('>I',len(p))+p for p in packets));return Path(path)
def workload(count=20000,depth=1000,batch=32,seed=42):
    if not 1<=depth<=10000 or not depth<=count<=1000000 or not 1<=batch<=100:raise ValueError('Invalid fixture dimensions')
    rng=random.Random(seed);events=[];active={};next_id=1
    for i in range(count):
        if len(active)<depth:
            order=next_id;next_id+=1;qty=rng.randint(10,100);events.append(add(order,side='B' if order%2 else 'S',quantity=qty,price=1000000+(order%101)*100,mpid='TEST' if order%7==0 else None));active[order]=qty
        else:
            order=rng.choice(tuple(active));qty=active[order];choice=rng.randrange(4)
            if choice==0:
                events.append(replace(order,next_id));del active[order];active[next_id]=20;next_id+=1
            elif choice==1:events.append(reduce('D',order));del active[order]
            else:
                amount=rng.randint(1,qty);events.append(reduce('E' if choice==2 else 'C',order,amount));active[order]-=amount
                if not active[order]:del active[order]
    packets=[packet(i+1,events[i:i+batch]) for i in range(0,len(events),batch)]
    packets.append(packet(count+1,count=65535));return packets

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=Path('runtime/demo.mdr'));p.add_argument('--count',type=int,default=20000);p.add_argument('--depth',type=int,default=1000);p.add_argument('--batch',type=int,default=32);a=p.parse_args();capture(a.output,workload(a.count,a.depth,a.batch));print(a.output)
