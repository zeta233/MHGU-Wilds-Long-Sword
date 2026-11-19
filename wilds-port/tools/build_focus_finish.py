"""Extend a v0.3.67 weapon ARC and its matching C tables with Wilds focus and crimson finishers."""
import argparse
import re
from build_combat import (Motlist, read_arc, arc_replace, ANIM_ENTRY, Lmt, unpack,
                          gu_skeleton, source_pose, retarget, matrixq, Path, np, struct)

# source, Wilds motion, free GU weapon slot, LMT donor, cancel, recovery
MOVES = [(926,465,88,144,125,132),(927,468,89,144,47,98),
         (928,469,90,144,47,96),(929,473,91,144,42,90),(930,293,92,148,65,114)]
# Reuse native finite blade-contact rows, changing only motion value.
CUTS = {905:(164,26,[(34,59)]),913:(165,10,[(8,16)]),914:(167,40,[(13,20),(24,35)]),
        926:(165,26,[(44,55),(67,76),(95,104)]),
        927:(167,40,[(9,23),(33,42)]),928:(167,40,[(10,19),(33,42)]),929:(167,40,[(11,19),(26,37)]),
        930:(161,60,[(37,56)])}

def build(base, headers, out):
    out.mkdir(exist_ok=True,parents=True)
    _,entries=read_arc(base)
    anim=next(e.data() for e in entries if e.name==ANIM_ENTRY)
    hit=next(e for e in entries if e.name.endswith('hitdata'))
    hdp=bytearray(hit.data())
    assert len(hdp)==16+192*56
    mot=Motlist()
    clips={mot.clip(i)['name']:mot.clip(i) for i in mot.inventory()}
    gu,remap=gu_skeleton()
    reverse={r:v for v,r in remap.items()}
    tables={n:(headers/n).read_text(encoding='utf8') for n in ('wilds_root_data.h','wilds_combat_data.h','world_events.h')}
    root=tables['wilds_root_data.h']
    meta=tables['wilds_combat_data.h']
    # A checkout can already contain the generated extension. Trim only its
    # appended slots/samples, preserving all earlier local motion data.
    previous=re.search(r'^ \{926u,88u,\d+u,(\d+)u,',meta,re.M)
    if previous:
        keep=int(previous[1])
        for tag in ('root','weapon','blade'):
            start=root.index('\n',root.index('static const float wilds_'+tag+'_samples'))+1
            end=root.index('};',start)
            root=root[:start]+''.join(root[start:end].splitlines(True)[:keep])+root[end:]
        for source,*_ in MOVES:
            meta=re.sub(r'^ \{'+str(source)+r'u,.*?\},\n','',meta,flags=re.M)
            tables['world_events.h']=re.sub(r'^ \{'+str(source)+r'u,.*?\},\n','',tables['world_events.h'],flags=re.M)
    count=root.split('static const float wilds_root_samples[][3]={')[1].split('};')[0].count('},')
    records=[];events=[];samples=[[],[],[]]
    for source,node,slot,donor,cancel,recovery in MOVES:
        assert not unpack('I',anim,8+4*slot)[0]
        clip=clips['wp03_00_'+str(node)]
        frames=int(clip['frames'])+1
        rotations={v:[] for v in remap};rotations[255]=[]
        positions={0:[],255:[]}
        for f in range(frames):
            pose=source_pose(mot,clip,f)
            weapon=np.linalg.inv(pose[mot.by_name['R_Hand']])@pose[mot.by_name['R_Wep']]
            rot,hip,_=retarget(mot,pose,gu,remap,reverse)
            raw=pose[mot.by_name['root']][:3,3]*100
            if f==0:origin=raw.copy()
            delta=raw-origin
            # GU keeps the broken part at the point of contact during its recoil.
            # Shorten the chase on the three cuts to keep the blade near it.
            positions[255].append(delta*([.50,0,.16] if source==926 else [1,0,1]))
            positions[0].append(hip)
            for v in remap:rotations[v].append(rot[v])
            rotations[255].append([0,0,0,1])
            samples[0].append(delta)
            samples[1].append(np.r_[weapon[:3,3]*100,matrixq(weapon)])
            samples[2].append(np.concatenate([(weapon@[0,0,z,1])[:3]*100 for z in (.2,.5,.8,1.1)]))
        anim=Lmt(anim).replace(slot,rotations,positions,frames=frames,template=donor)
        records.append(f' {{{source}u,{slot}u,{frames}u,{count}u,{cancel}u,{recovery}u}},')
        events.append(f' {{{source}u,{slot}u,{frames}u,0u,0u}},')
        count+=frames
    for name,rows in zip(('root','weapon','blade'),samples):
        i=root.index('};',root.index('static const float wilds_'+name+'_samples'))
        root=root[:i]+''.join(' {'+','.join(f'{v:.6f}f' for v in row)+'},\n' for row in rows)+root[i:]
    def append(text,decl,lines):
        i=text.index('};',text.index(decl))
        return text[:i]+'\n'.join(lines)+'\n'+text[i:]
    meta=append(meta,'WildsClip wilds_clips[]',records)
    cuts=[]
    for source,(donor,mv,windows) in CUTS.items():
        meta=re.sub(r'^ \{'+str(source)+r'u,\d+u,\d+u,\d+u,0u\},\n','',meta,flags=re.M)
        row=(len(hdp)-16)//56
        record=hdp[16+56*donor:72+56*donor];struct.pack_into('<h',record,4,mv);hdp.extend(record)
        cuts.extend(f' {{{source}u,{start}u,{end}u,{row}u,0u}},' for start,end in windows)
    n=(len(hdp)-16)//56;struct.pack_into('<I',hdp,8,n)
    meta=append(meta,'WildsStrike wilds_strikes[]',cuts).replace('WILDS_HIT_COUNT 192u',f'WILDS_HIT_COUNT {n}u')
    tables.update({'world_events.h':append(tables['world_events.h'],'WClip world_clips[]',events),
                   'wilds_root_data.h':root,'wilds_combat_data.h':meta})
    for name,data in tables.items():(out/name).write_bytes(data.encode())
    arc_replace(base,out/'w07.arc',{ANIM_ENTRY:anim,hit.name:hdp})

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    for arg in ('base','headers','out'):parser.add_argument(arg,type=Path)
    args=parser.parse_args();build(args.base,args.headers,args.out)
