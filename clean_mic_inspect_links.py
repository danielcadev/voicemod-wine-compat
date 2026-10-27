"""Read-only, bounded inspection of known Clean Mic SDK parameter objects.

Requires PID supplied by operator; never attaches/stops/writes a process.
Only reports known public definition IDs and native link addresses/indices.
The layouts are specific to the studied 3.17.40 executable.
"""
import re
import struct
import sys
from pathlib import Path

pid=int(sys.argv[1])
mem=open(f'/proc/{pid}/mem','rb',buffering=0)
ranges=[]
for line in Path(f'/proc/{pid}/maps').read_text().splitlines():
    span,permissions,*_=line.split()
    lo,hi=(int(x,16) for x in span.split('-'))
    if permissions.startswith('rw') and hi-lo<=1024**3:
        ranges.append((lo,hi))

def read(address,size):
    try:
        mem.seek(address)
        data=mem.read(size)
        return data if len(data)==size else None
    except (OSError,OverflowError):
        return None

def q(address):
    data=read(address,8)
    return struct.unpack('<Q',data)[0] if data else 0

seen=set()
identifiers=[b'NoReverb',b'Reverb Volume',b'NaturalVoice',b'Mix']
for lo,hi in ranges:
    for start in range(lo,hi,4*1024**2):
        data=read(start,min(4*1024**2+64,hi-start))
        if not data:
            continue
        for name in identifiers:
            for match in re.finditer(re.escape(name+b'\0'),data):
                string=start+match.start()
                if q(string+16)!=len(name) or q(string+24)!=15:
                    continue
                obj=string-0x330
                vtable=q(obj)
                if not 0x140000000 <= vtable < 0x143246000 or obj in seen:
                    continue
                seen.add(obj)
                head=q(obj+0x400)
                count=q(obj+0x408)
                if not head or count>1024:
                    continue
                value=read(obj+0x4c8,4)
                print(name.decode(),f'object={obj:#x} vtable={vtable:#x}',
                      'valueSlot='+repr(struct.unpack('<f',value)[0] if value else None),
                      'vectorLinks='+str(count))
                node=q(head)
                visited=set()
                while node and node!=head and node not in visited and len(visited)<min(count,32):
                    visited.add(node)
                    link=q(node+16)
                    record=read(link,16)
                    if not record:
                        break
                    target,parameter,element=struct.unpack('<Qii',record)
                    print(f'  target={target:#x} targetVtable={q(target):#x} parameter={parameter} element={element}')
                    if q(target)==0x14260e368:
                        vector=q(target+0x578)
                        length=q(target+0x590)
                        if length<=16:
                            values=read(vector,length*4)
                            if values:
                                print('  MixN runtime volumes=',struct.unpack('<'+'f'*length,values))
                    node=q(node)
