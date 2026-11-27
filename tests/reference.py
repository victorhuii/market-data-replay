"""Independent Python wire/reference book; does not import C++ event logic."""
import struct

def decode(packets):
    orders={};expected=1;session=None;ended=False
    for packet in packets:
        sid,seq,count=struct.unpack('>10sQH',packet[:20]);assert session in (None,sid);session=sid
        if count in (0,65535):assert seq==expected;ended=count==65535;continue
        pos=20
        for i in range(count):
            length=struct.unpack('>H',packet[pos:pos+2])[0];pos+=2;msg=packet[pos:pos+length];pos+=length
            if seq+i<expected:continue
            assert seq+i==expected;expected+=1;kind=chr(msg[0]);locate=struct.unpack('>H',msg[1:3])[0]
            if kind=='S':continue
            order=struct.unpack('>Q',msg[11:19])[0]
            if kind in 'AF':
                _,side,quantity,stock,price=struct.unpack('>QcI8sI',msg[11:36]);orders[order]=dict(id=order,locate=locate,side=side.decode(),quantity=quantity,price=price,stock_hex=stock.hex(),mpid_hex=(msg[36:40] if kind=='F' else b'    ').hex())
            elif kind=='D':del orders[order]
            elif kind=='U':
                new,quantity,price=struct.unpack('>QII',msg[19:35]);row=orders.pop(order);row.update(id=new,quantity=quantity,price=price);orders[new]=row
            else:
                amount=struct.unpack('>I',msg[19:23])[0];orders[order]['quantity']-=amount
                if not orders[order]['quantity']:del orders[order]
    return dict(next_sequence=expected,ended=ended,orders=[orders[k] for k in sorted(orders)])
