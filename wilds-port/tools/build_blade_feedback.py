"""Extend the v0.3.68 GU effects ARC with swept blade segments and contact slashes."""
from pathlib import Path
import struct
import sys
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'x/tools'))
from build_effect_assets import entries, pel_groups, pack

def build(base,dest):
    data=entries(base.read_bytes())
    keys={k[0]:k for k in data}
    def write(name,donor,payload):
        key=keys[donor]
        data[(name,key[1])]=(data[key][0]&0xe0000000|len(payload),payload)
    texture='effect/world/crimson_edge'
    tex=data[keys[texture]][1]
    # Retain the source texture layout and feathering. Lift the dim body
    # of the streak as well as the white-hot edge of its core.
    rgba=np.frombuffer(tex[24:],dtype=np.uint8).reshape(-1,4).copy()
    light=np.sqrt(rgba[:,0].astype(float)/255)
    rgba[:,0]=255*light
    rgba[:,1]=128*light**3
    rgba[:,2]=98*light**3
    rgba[:,3]=np.clip(105+rgba[:,3].astype(float)*1.3,0,255)
    write(texture,texture,tex[:24]+rgba.tobytes())
    pel='effect/pel/pl/world_simple';raw=data[keys[pel]][1];groups=pel_groups(raw)
    for uid,name,template in [(0x3fd1,'crimson_mid',0x3fd0),(0x3fd2,'crimson_tail',0x3fd0),
                              (0x3fd3,'release_cut_up',0x3f85),(0x3fd4,'release_cut_down',0x3f85),
                              (0x3fd5,'release_cut_end',0x3f85)]:
        original=next(r['raw'] for r in groups[2] if r['uid']==template)
        donor=original[1:1+original[0]].decode()
        name='effect/world/'+name
        write(name,donor,data[keys[donor]][1])
        path=name.encode()
        row=bytearray(bytes([len(path)])+path+original[1+original[0]:])
        struct.pack_into('<H',row,len(row)-160+48,uid)
        groups[2].append({'raw':row,'uid':uid})
    write(pel,pel,raw[:8]+b''.join(struct.pack('<I',len(g))+b''.join(r['raw'] for r in g) for g in groups))
    assert len(data)==165
    dest.write_bytes(pack(data))

if __name__=='__main__':build(*map(Path,sys.argv[1:]))
