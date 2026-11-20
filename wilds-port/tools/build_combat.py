"""Build a GU combat adaptation from traced, user-owned Wilds clips."""
from portkit import *
from combat_rules import MOVES,STRIKES,SHARED_STRIKES,CRIMSON_STRIKES,RETIRED_CRIMSON_ROWS,SPIRIT_NATIVE_ROWS,SPIRIT_ROUND_ROWS

def fromto(a,b):
 a=np.asarray(a);b=np.asarray(b);a=a/np.linalg.norm(a);b=b/np.linalg.norm(b);d=np.dot(a,b)
 if d>.999999:return np.array([0,0,0,1.])
 if d<-.999999:
  axis=np.cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]);return np.r_[axis/np.linalg.norm(axis),0]
 return qnorm(np.r_[np.cross(a,b),1+d])

def retarget(mot,sw,gu,remap,rev):
 root=sw[mot.by_name['root']][:3,3]*100
 desired={v:matrixq(sw[mot.by_name[name]]) for v,name in BODY_MAP.items()}
 hip=sw[mot.by_name['Hip']][:3,3]*100-root
 def locals():
  return {r:qnorm(qmul(qinv(desired[rev[gu[r].parent]]) if gu[r].parent is not None else np.array([0,0,0,1]),desired[v])) for v,r in remap.items()}
 rots=locals();gw=world_pose(gu,rots,{remap[0]:hip})
 # Preserve close two-hand contacts after changing skeleton proportions. Never
 # pull the free left arm into the sword during an authored one-hand stroke.
 delta=(sw[mot.by_name['L_Hand']][:3,3]-sw[mot.by_name['R_Hand']][:3,3])*100
 distance=np.linalg.norm(delta);weight=float(np.clip((38-distance)/12,0,1))
 error=0
 if weight>0:
  shoulder,elbow,hand=[gw[remap[v]][:3,3] for v in [6,7,8]]
  target=gw[remap[12]][:3,3]+delta
  target=hand*(1-weight)+target*weight
  upper=np.linalg.norm(elbow-shoulder);lower=np.linalg.norm(hand-elbow)
  direction=target-shoulder;length=np.linalg.norm(direction);direction/=max(length,1e-8)
  length=np.clip(length,abs(upper-lower)+.01,upper+lower-.01)
  target=shoulder+direction*length
  bend=elbow-shoulder-direction*np.dot(elbow-shoulder,direction)
  if np.linalg.norm(bend)<1e-5:bend=np.cross(direction,[0,1,0])
  bend/=max(np.linalg.norm(bend),1e-8)
  along=(upper*upper-lower*lower+length*length)/(2*length)
  new_elbow=shoulder+direction*along+bend*math.sqrt(max(0,upper*upper-along*along))
  desired[6]=qmul(fromto(elbow-shoulder,new_elbow-shoulder),desired[6])
  desired[7]=qmul(fromto(hand-elbow,target-new_elbow),desired[7])
  rots=locals();gw=world_pose(gu,rots,{remap[0]:hip})
  error=float(np.linalg.norm(gw[remap[8]][:3,3]-target))
 # Match contact height using the retarget's feet rather than source limb length.
 # Keep the source's lowest-foot height, including intentional jumps.
 source_floor=min(sw[mot.by_name[n]][1,3]*100-root[1] for n in ['L_Foot','R_Foot'])
 target_floor=min(gw[remap[v]][1,3] for v in [16,19])
 hip[1]+=source_floor-target_floor
 return {v:rots[r] for v,r in remap.items()},hip,error

def append_crimson_strikes(hdp,strikes):
 row_count=unpack('I',hdp,8)[0]
 assert len(hdp)==16+56*row_count
 crimson_sources={source for source,_,_ in CRIMSON_STRIKES}
 result=[strike for strike in strikes if strike['source'] not in crimson_sources]
 def clone(donor,mv):
  donor_row=next(strike['row'] for strike in strikes if strike['source']==donor)
  offset=16+56*donor_row
  record=bytearray(hdp[offset:offset+56])
  struct.pack_into('<h',record,4,mv)
  row=(len(hdp)-16)//56
  hdp.extend(record)
  return row
 for source,donor,cuts in CRIMSON_STRIKES:
  for begin,end,mv in cuts:
   row=clone(donor,mv)
   result.append(dict(source=source,begin=begin,end=end,row=row,tiered=False,motion_values=[mv]))
 for donor,mv in RETIRED_CRIMSON_ROWS:
  clone(donor,mv)
 row_count=(len(hdp)-16)//56
 struct.pack_into('<I',hdp,8,row_count)
 return result,row_count

def append_spirit_rows(hdp):
 row_count=unpack('I',hdp,8)[0]
 native=[]; rounds={}
 def clone(donor,mv):
  offset=16+56*donor
  record=bytearray(hdp[offset:offset+56])
  struct.pack_into('<h',record,4,mv)
  row=(len(hdp)-16)//56
  hdp.extend(record)
  return row
 assert len(hdp)==16+56*row_count
 for first,last,donor,mv in SPIRIT_NATIVE_ROWS:
  native.append((first,last,donor,clone(donor,mv),mv))
 for label,donor,mv in SPIRIT_ROUND_ROWS:
  rounds[label]=clone(donor,mv)
 row_count=(len(hdp)-16)//56
 struct.pack_into('<I',hdp,8,row_count)
 return native,rounds,row_count

def build(out,combat_base=None):
 out=Path(out);out.mkdir(parents=True,exist_ok=True)
 mot=Motlist();clips={mot.clip(o)['name']:mot.clip(o) for o in mot.inventory()}
 _,entries=read_arc(BASE_ARC);original=next(e.data() for e in entries if e.name==ANIM_ENTRY);data=original
 if combat_base:
  _,existing=read_arc(combat_base)
  original=next(e.data() for e in existing if e.name==ANIM_ENTRY)
  data=original
 gu,remap=gu_skeleton();rev={r:v for v,r in remap.items()}
 manifest=[];roots=[];weapons=[];blades=[]
 for label,node,slot,source,template,cancel,recovery in MOVES:
  clip=clips['wp03_00_'+node];frames=int(clip['frames'])+1;offset=len(roots)
  rotations={v:[] for v in remap};rotations[255]=[];positions={0:[],255:[]};rawroots=[];maxerr=0
  for f in range(frames):
   sw=source_pose(mot,clip,f);weapon=np.linalg.inv(sw[mot.by_name['R_Hand']])@sw[mot.by_name['R_Wep']]
   weapons.append([*list(weapon[:3,3]*100),*list(matrixq(weapon))])
   points=[(weapon@np.array([0,0,z/100,1]))[:3]*100 for z in [20,50,80,110]]
   blades.append(np.concatenate(points));rawroots.append(sw[mot.by_name['root']][:3,3]*100)
   rot,hip,err=retarget(mot,sw,gu,remap,rev);maxerr=max(err,maxerr)
   for v in remap:rotations[v].append(rot[v])
   rotations[255].append([0,0,0,1]);positions[0].append(hip);positions[255].append([0,0,0])
  positions[255]=[(r-rawroots[0])*[1,0,1] for r in rawroots]
  if source==920:
   positions[0]=[hip+delta for hip,delta in zip(positions[0],positions[255])]
   positions[255]=[[0,0,0]]*frames
  if not combat_base or not unpack('I',original,8+slot*4)[0]:
   data=Lmt(data).replace(slot,rotations,positions,frames=frames,template=template)
  roots.extend(positions[255] if source==920 else [r-rawroots[0] for r in rawroots])
  manifest.append(dict(label=label,clip=clip['name'],source=source,slot=slot,frames=frames,offset=offset,cancel=cancel,recovery=min(recovery,frames-1),max_solved_contact_error=maxerr))
  print(label,frames,'IK residual',round(maxerr,4),flush=True)
 # Dedicated native rows: zero startup, long native duration, explicitly gated
 # by the plugin's finite frame windows. No old custom rows are overwritten.
 hdp=next(e.data() for e in entries if e.name.endswith('hitdata'));hdp=bytearray(hdp);row_count=unpack('I',hdp,8)[0];strikes=[]
 assert len(hdp)==16+56*row_count
 for source,begin,end,mvs in STRIKES:
  first=row_count
  for mv in mvs:
   record=bytearray(hdp[16:72]);struct.pack_into('<3h',record,0,0,32767,mv)
   record[8]=1;record[12]=2;record[14]=0;record[38]=3
   hdp.extend(record);row_count+=1
  strikes.append(dict(source=source,begin=begin,end=end,row=first,tiered=len(mvs)==4,motion_values=mvs))
 for source,begin,end,shared in SHARED_STRIKES:
  donor=next(row for row in strikes if row['source']==shared)
  strikes.append(dict(donor,source=source,begin=begin,end=end))
 release_row=row_count
 # Four release-shell strengths from the user's wp03_shell.rcol.28.
 for mv in [9,14,20,32]:
  record=bytearray(hdp[16:72]);struct.pack_into('<3h',record,0,0,32767,mv)
  record[8]=1;record[12]=2;record[14]=0;record[38]=3
  hdp.extend(record);row_count+=1
 struct.pack_into('<I',hdp,8,row_count)
 strikes,row_count=append_crimson_strikes(hdp,strikes)
 crimson_count=row_count
 spirit,rounds,row_count=append_spirit_rows(hdp)
 hts=next(e.data() for e in entries if e.name.endswith('hitsize'));count=unpack('I',hts,8)[0];assert count==15
 groups=[]
 for i in range(count):
  n,at=unpack('2I',hts,16+8*i);groups.append(hts[at:at+48*n])
 groups.append(b''.join(struct.pack('<HHII9f',12,12,1,0,18,0,0,z,0,0,0,0,0) for z in [20,50,80,110]))
 # An attached Release hit is local to the captured hurtpart, not the
 # 100/200-unit World Helm/Iai area sphere.
 groups.append(struct.pack('<HHII9f',255,0,0,0,18,0,0,0,0,0,0,0,0))
 hts=bytearray(hts[:16]+bytes(8*len(groups)));struct.pack_into('<I',hts,8,len(groups))
 for i,g in enumerate(groups):struct.pack_into('<2I',hts,16+8*i,len(g)//48,len(hts));hts.extend(g)
 replacements={ANIM_ENTRY:data,r'player\hit\pl_we07_hitdata':bytes(hdp),r'player\hit\pl_we07_hitsize':bytes(hts)}
 if combat_base:
  for name,expected in list(replacements.items()):
   if name==ANIM_ENTRY:continue
   prior=next(e.data() for e in existing if e.name==name)
   if prior==expected:
    del replacements[name]
   else:
    assert name.endswith('hitdata') and len(prior) in (16+56*(release_row+4),16+56*crimson_count), name
    assert prior[:8]==expected[:8] and prior[12:]==expected[12:len(prior)], name
 arc_replace(combat_base or BASE_ARC,out/'combat.arc',replacements);(out/'combat.lmt').write_bytes(data)
 for slot in range(Lmt(original).count):
  if slot not in [x[2] for x in MOVES if not combat_base or not unpack('I',original,8+x[2]*4)[0]]:assert original[8+slot*4:12+slot*4]==data[8+slot*4:12+slot*4]
 meta=['#pragma once','typedef struct {unsigned source,slot,frames,offset,cancel,recovery;} WildsClip;','static const WildsClip wilds_clips[]={']
 meta+=[' {%du,%du,%du,%du,%du,%du},'%(x['source'],x['slot'],x['frames'],x['offset'],x['cancel'],x['recovery']) for x in manifest]
 meta+=['};','typedef struct {unsigned source,begin,end,row,tiered;} WildsStrike;','static const WildsStrike wilds_strikes[]={']
 meta+=[' {%du,%du,%du,%du,%du},'%(x['source'],x['begin'],x['end'],x['row'],x['tiered']) for x in strikes]
 meta+=['};',f'#define WILDS_HIT_COUNT {row_count}u','#define WILDS_SHAPE 15u','static const WildsClip *wilds_clip(unsigned source){for(unsigned i=0;i<sizeof(wilds_clips)/sizeof(*wilds_clips);++i)if(wilds_clips[i].source==source)return wilds_clips+i;return 0;}']
 meta += [f'#define WILDS_RELEASE_ROW {release_row}u','#define WILDS_RELEASE_SHAPE 16u']
 meta+=['typedef struct {unsigned first,last,donor,row;} WildsSpiritRow;',
         'static const WildsSpiritRow wilds_spirit_rows[]={']
 meta+=[' {%du,%du,%du,%du},'%(first,last,donor,row) for first,last,donor,row,_ in spirit]
 meta+=['};']+[f'#define WILDS_ROUND_{name}_ROW {row}u' for name,row in rounds.items()]
 (out/'wilds_combat_data.h').write_text('\n'.join(meta)+'\n')
 header=['#pragma once','#include "wilds_combat_data.h"']
 for name,array,width in [('wilds_root_samples',roots,3),('wilds_weapon_samples',weapons,7),('wilds_blade_samples',blades,12)]:
  header.append(f'static const float {name}[][{width}]={{');header+=[' {'+','.join(f'{v:.6f}f' for v in r)+'},' for r in array];header.append('};')
 (out/'wilds_root_data.h').write_text('\n'.join(header)+'\n')
 (out/'world_clip_entries.txt').write_text('\n'.join(' {%du,%du,%du,0u,0u},'%(x['source'],x['slot'],x['frames']) for x in manifest))
 (out/'manifest.json').write_text(json.dumps(dict(status='combat integration under validation',moves=manifest,strikes=strikes,spirit_rows=spirit,round_rows=rounds,hit_count=row_count,shape_count=len(groups),arc_sha256=hashlib.sha256((out/'combat.arc').read_bytes()).hexdigest()),indent=2))
if __name__=='__main__':
 import sys
 build(sys.argv[1], sys.argv[2] if len(sys.argv)>2 else None)
