"""Add stock GU roundslash timelines and weapon banks to the existing mod assets."""
import argparse
from pathlib import Path
import struct, zlib, hashlib
from portkit import read_arc, arc_replace, ArcEntry
from build_wilds_native_banks import native_slots

def pack(header,entries,dest):
 b=bytearray(header+bytes(80*len(entries)));struct.pack_into('<H',b,6,len(entries))
 for i,e in enumerate(entries):
  b+=bytes(-len(b)%16);struct.pack_into('<64s4I',b,12+i*80,e.name.encode(),e.kind,len(e.compressed),e.flags,len(b));b+=e.compressed
 dest.write_bytes(b)
def item(e,n,b):return ArcEntry(n,e.kind,(e.flags&0xe0000000)|len(b),zlib.compress(b))
def append_gu_follow_audio(entries, common_request_index):
    """Reuse the stock Long Sword impact wave in the overlapping contact mixer."""
    stock_entries = entries[common_request_index-2:common_request_index+1]
    slots = native_slots(stock_entries)
    stock_wave = slots[50]
    assert stock_wave and slots[51] == stock_wave == slots[52]
    wave_entry, = (e for e in entries if e.name == stock_wave and e.kind == 0x79c47b59)
    wave = r'sound\se\pl\wp\swo\world\wav\gu_follow'

    world_path = r'sound\se\pl\wp\swo\world\snd_world'
    bank_index, = (i for i, e in enumerate(entries)
                   if e.name == world_path and e.kind == 0x15d782fb)
    request_index, = (i for i, e in enumerate(entries)
                      if e.name == world_path and e.kind == 0x1bcc4966)
    bank_entry, request_entry = entries[bank_index], entries[request_index]
    bank = bytearray(bank_entry.data())
    requests = request_entry.data()
    count = struct.unpack_from('<I', requests, 8)[0]
    template_offset = 52 + 132 * 75
    template = requests[template_offset:template_offset+132]

    bank_count = struct.unpack_from('<I', bank, 12)[0]
    bank_group = struct.unpack_from('<I', bank, 20)[0]
    assert (bank_group >> 17) & 1023 == bank_count
    bank_end = 32
    keys = set()
    donor = None
    for _ in range(bank_count):
        end = bank.index(0, bank_end)
        row = bank[end+1:end+81]
        keys.update(row[75:78])
        if template[2] in row[75:78]:
            donor = row
        bank_end = end + 81
    key = max(keys) + 1
    assert donor is not None
    donor[75:78] = bytes([key] * 3)
    bank[bank_end:bank_end] = wave.encode('ascii') + b'\0' + donor
    struct.pack_into('<I', bank, 12, bank_count+1)
    struct.pack_into('<I', bank, 20, (bank_group & ~(1023 << 17)) | ((bank_count+1) << 17))

    stock_requests = stock_entries[-1].data()
    stock_count = struct.unpack_from('<I', stock_requests, 8)[0]
    variants = {}
    for i in range(stock_count):
        row = stock_requests[52+132*i:52+132*(i+1)]
        variants[struct.unpack_from('<H', row)[0]] = row[36:44]
    added = bytearray()
    for i, stock_id in enumerate((50, 51, 52)):
        request = count + i
        row = bytearray(template)
        struct.pack_into('<HBB', row, 0, request, key, template[3])
        row[14] = request
        row[36:44] = variants[stock_id]
        added.extend(row)
    header = bytearray(requests[:52])
    struct.pack_into('<I', header, 8, count+3)
    for offset in (32, 36, 40, 44, 48):
        original = struct.unpack_from('<I', requests, offset)[0]
        struct.pack_into('<I', header, offset, original+len(added))
    end = 52 + 132 * count
    footer = bytearray(requests[end:])
    path_offset = struct.unpack_from('<I', footer)[0]
    assert requests[path_offset:].startswith(world_path.encode('ascii') + b'\0')
    struct.pack_into('<I', footer, 0, path_offset+len(added))
    requests = header + requests[52:end] + added + footer
    entries[bank_index] = item(bank_entry, bank_entry.name, bank)
    entries[request_index] = item(request_entry, request_entry.name, requests)
    # ARC loading resolves the bank's wave paths in resource order.
    entries.insert(bank_index, ArcEntry(wave, wave_entry.kind, wave_entry.flags, wave_entry.compressed))
    return bank_index, count

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
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stock', type=Path, help='GU nativeNX/arc directory')
    parser.add_argument('mod', type=Path, help='base mod nativeNX/arc before adding roundslash audio')
    parser.add_argument('out', type=Path, help='output directory for both archives and gu_round_banks.h')
    args = parser.parse_args()
    stock, mod, out = args.stock, args.mod, args.out
    src = out
    out.mkdir(exist_ok=True, parents=True)
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
    follow_wave_index,follow_sound=append_gu_follow_audio(es,mapping[255])
    mapping=[index+(index>=follow_wave_index) for index in mapping]
    pack(header,es,out/'world_audio.arc')
    (src/'gu_round_banks.h').write_text(
        '#pragma once\n'
        f'#define GU_ROUND_RESOURCE_COUNT {len(es)}u\n'
        f'#define GU_FOLLOW_SOUND {follow_sound}u\n'
        f'#define GU_FOLLOW_FINISH_SOUND {follow_sound+2}u\n'
        'static const unsigned short gu_round_request_index[256]={\n'
        + ''.join(','.join(map(str,mapping[i:i+16]))+',\n' for i in range(0,256,16))
        + '};\n'
    )
    print(len(es),len(waves),len([v for v in mapping if v]))
