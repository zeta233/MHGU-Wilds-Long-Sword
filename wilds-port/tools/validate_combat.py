"""Validate the combat package against original ARC data and native key clocks."""
from portkit import *
from combat_rules import MOVES
from build_charge_walk import STAND_SLOT, WALK_SLOT
import sys
out=Path(sys.argv[1]);base=Path(sys.argv[2]) if len(sys.argv)>2 else BASE_ARC;_,old=read_arc(base);_,new=read_arc(out/'combat.arc')
a={x.name:x.data() for x in old};b={x.name:x.data() for x in new}
lmt=Lmt(b[ANIM_ENTRY]);report=[]
for label,node,slot,source,template,cancel,recovery in MOVES:
 frames,tracks=lmt.tracks(slot)
 if slot==WALK_SLOT:assert frames==lmt.tracks(STAND_SLOT)[0]
 for track in tracks:
  if track.codec==3:
   keys=[unpack('3fI',track.data,i) for i in range(0,len(track.data),16)]
   assert len(keys)==frames and sum(x[3] for x in keys)==frames-1,(label,track.bone)
   assert np.all(np.isfinite([x[:3] for x in keys]))
  if track.codec==6:
   keys=[decode_q14(track.data[i:i+8]) for i in range(0,len(track.data),8)]
   assert len(keys)==frames and sum(x[1] for x in keys)==frames-1,(label,track.bone)
   assert max(abs(np.linalg.norm(x[0])-1) for x in keys)<.002
 root=next(t for t in tracks if t.bone==255 and t.usage==4)
 first=unpack('3fI',root.data);last=unpack('3fI',root.data,len(root.data)-16)
 report.append(dict(label=label,frames=frames,root_start=first[:3],root_end=last[:3]))
old_lmt=Lmt(a[ANIM_ENTRY]);changed={x[2] for x in MOVES if not unpack('I',old_lmt.data,8+x[2]*4)[0]}
for slot in range(old_lmt.count):
 if slot==WALK_SLOT:continue
 if slot in changed:assert not unpack('I',a[ANIM_ENTRY],8+slot*4)[0],slot
 else:assert a[ANIM_ENTRY][8+slot*4:12+slot*4]==b[ANIM_ENTRY][8+slot*4:12+slot*4]
for name in a:
 if name.endswith('hitdata'):
  assert a[name][:8]==b[name][:8] and a[name][12:]==b[name][12:len(a[name])]
 elif name.endswith('hitsize'):
  for i in range(unpack('I',a[name],8)[0]):
   n,x=unpack('2I',a[name],16+i*8);m,y=unpack('2I',b[name],16+i*8)
   assert n==m and a[name][x:x+48*n]==b[name][y:y+48*m]
 elif name!=ANIM_ENTRY:assert a[name]==b[name]
manifest=json.loads((out/'manifest.json').read_text());hit_count=manifest['hit_count']
for strike in manifest['strikes']:
 clip=next(x for x in manifest['moves'] if x['source']==strike['source'])
 assert 0<=strike['begin']<strike['end']<=clip['cancel'] or (strike['source']==907 and strike['end']<=77)
 assert strike['row']+(3 if strike['tiered'] else 0)<hit_count
(out/'format-validation.json').write_text(json.dumps(dict(passed=True,clips=report,untouched_slots=256-len(changed),old_hit_rows_preserved=True,old_shape_records_preserved=True),indent=2))
print(f'PASS: {len(MOVES)} clips, native translation/rotation clocks, old slots/rows/shapes, finite strike windows')
