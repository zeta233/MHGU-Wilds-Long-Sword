"""Add stock GU roundslash timelines and weapon banks to the existing mod assets."""
import argparse
from pathlib import Path
import struct, zlib, hashlib
from portkit import read_arc, arc_replace, ArcEntry

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stock', type=Path, help='GU nativeNX/arc directory')
parser.add_argument('mod', type=Path, help='base mod nativeNX/arc before adding roundslash audio')
parser.add_argument('out', type=Path, help='output directory for both archives and gu_round_banks.h')
args = parser.parse_args()
stock, mod, out = args.stock, args.mod, args.out
src = out
out.mkdir(exist_ok=True, parents=True)
def pack(header,entries,dest):
 b=bytearray(header+bytes(80*len(entries)));struct.pack_into('<H',b,6,len(entries))
 for i,e in enumerate(entries):
  b+=bytes(-len(b)%16);struct.pack_into('<64s4I',b,12+i*80,e.name.encode(),e.kind,len(e.compressed),e.flags,len(b));b+=e.compressed
 dest.write_bytes(b)
def item(e,n,b):return ArcEntry(n,e.kind,(e.flags&0xe0000000)|len(b),zlib.compress(b))
def mss(b):
 cnt,cap=struct.unpack_from('<HH',b,8);at=16+2*cap
 rows=dict(struct.iter_unpack('<II',b[at:at+8*cnt]));assert sorted(rows.values())==list(rows.values())
 blobs={m:b[k:end] for (m,k),end in zip(rows.items(),list(rows.values())[1:]+[len(b)])}
 for slot in (148,219):
  m=5000+slot;assert m not in blobs;blobs[m]=struct.pack('<I',m)+blobs[5127][4:]
 n=len(blobs);header=bytearray(b[:at]);struct.pack_into('<H',header,8,n)
 assert struct.unpack_from('<%dh'%cap,b,16)==tuple(list(range(cnt))+[-1]*(cap-cnt))
 struct.pack_into('<%dh'%cap,header,16,*range(n),*([-1]*(cap-n)))
 data=bytearray();table=bytearray()
 for m,r in sorted(blobs.items()):
  table+=struct.pack('<II',m,at+8*n+len(data));data+=r
 return header+table+data
motion=mod/'player/quest/w07.arc';e=next(e for e in read_arc(motion)[1] if e.name.endswith('pl_wp07'));arc_replace(motion,out/'w07.arc',{e.name:mss(e.data())})
header,es=read_arc(mod/'forge/world_audio.arc');mapping=[0]*256;waves={}
for f in sorted((stock/'weapon/w07/snd').glob('*.arc')):
 num=int(f.stem) if f.stem.isdigit() else 255
 bank=read_arc(f)[1];rename={};group=[]
 for e in bank:
  if e.kind!=0x79c47b59:continue
  b=e.data();digest=hashlib.sha256(b).digest();name=waves.get(digest)
  if not name:
   name=r'sound\se\pl\wp\swr\wav\w%03d'%len(waves);waves[digest]=name;es.append(item(e,name,b))
  rename[e.name]=name
 # Replace variable-length wave references. Their binary metadata
 # follows the path terminator.
 for e in bank:
  if e.kind==0x79c47b59:continue
  b=e.data();old=e.name
  if e.kind==0x15d782fb:
   for k,v in rename.items():b=b.replace(k.encode()+b'\0',v.encode()+b'\0')
  group.append((e,b))
 # Paths in the SRQR footer are absolute-offset indexed; rename same length.
 # Distinct request banks preserve equipment-specific metadata.
 for e,b in group:
  name=e.name.replace('\\swo\\','\\swr\\')
  if e.kind==0x1bcc4966:b=b.replace(b'\\swo\\',b'\\swr\\');mapping[num]=len(es)
  es.append(item(e,name,b))
pack(header,es,out/'world_audio.arc')
(src/'gu_round_banks.h').write_text('#pragma once\n#define GU_ROUND_RESOURCE_COUNT %du\nstatic const unsigned short gu_round_request_index[256]={\n'%len(es)+''.join(','.join(map(str,mapping[i:i+16]))+',\n' for i in range(0,256,16))+'};\n')
print(len(es),len(waves),len([v for v in mapping if v]))
