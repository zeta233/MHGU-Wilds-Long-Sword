"""Append traced Wilds events to GU's private sound bank, preserving old rows.

Inputs are the user's extracted/decoded event WAVs and MOT timeline cue export.
DSP encoder: jackoalan/gc-dspadpcm-encode (MIT), supplied as a local DLL.
"""
from portkit import *
import ctypes,wave

def encode(samples,dll):
 n=len(samples);padded=np.pad(samples,(0,(-n)%14)).astype(np.int16)
 coeff=(ctypes.c_short*16)();pcm=(ctypes.c_short*len(padded)).from_buffer_copy(padded)
 dll.DSPCorrelateCoefs(pcm,n,coeff)
 frame=(ctypes.c_short*16)();block=(ctypes.c_ubyte*8)();result=bytearray()
 for i in range(0,len(padded),14):
  for k in range(14):frame[k+2]=int(padded[i+k])
  dll.DSPEncodeFrame(frame,14,block,coeff);result.extend(block)
  frame[0]=frame[14];frame[1]=frame[15]
 # Independent decoder, using the same integer arithmetic as Nintendo DSP.
 h1=h2=0;decoded=[]
 for i in range(0,len(result),8):
  scale=1<<(result[i]&15);pred=result[i]>>4;c1,c2=coeff[2*pred:2*pred+2]
  for k in range(14):
   v=(result[i+1+k//2]>>(4 if k%2==0 else 0))&15;v=v-16 if v>=8 else v
   value=max(-32768,min(32767,(v*scale*2048+c1*h1+c2*h2+1024)>>11))
   decoded.append(value);h2=h1;h1=value
 error=np.asarray(decoded[:n],dtype=float)-samples
 snr=10*np.log10(max(1.,np.dot(samples.astype(float),samples.astype(float)))/max(1.,np.dot(error,error)))
 assert snr>18,('low DSP SNR',snr)
 return bytes(result),list(coeff),float(snr)

# Shared weapon layers are mixed specifically for consecutive Wilds moves.
# Leave their original requests and banks available to the earlier moveset.
SHARED_WEAPON_REQUESTS = (6, 7, 9, 10, 11, 33)

def decode_wav(raw):
 n,rate=unpack('2I',raw,96)
 assert rate==48000
 coeff=unpack('16h',raw,24)
 h1=h2=0;decoded=[]
 for at in range(120,120+((n+13)//14)*8,8):
  scale=1<<(raw[at]&15);pred=raw[at]>>4;c1,c2=coeff[2*pred:2*pred+2]
  for k in range(14):
   v=(raw[at+1+k//2]>>(4 if k%2==0 else 0))&15;v=v-16 if v>=8 else v
   value=max(-32768,min(32767,(v*scale*2048+c1*h1+c2*h2+1024)>>11))
   decoded.append(value);h2=h1;h1=value
 return np.asarray(decoded[:n],dtype=float)

def mix_personal(data,target,peak=.9):
 # Use the louder 20 ms windows: the source files have dissimilar tails
 # and adding gain by whole-file RMS made quiet blade transients inconsistent.
 blocks=np.pad(data,(0,(-len(data))%960)).reshape(-1,960)/32768
 active=np.quantile(np.sqrt(np.mean(blocks*blocks,axis=1)),.9)
 assert active>0
 gain=max(1.,min(10**(27/20),target/active))
 shaped=data/32768*gain
 # Smooth only the amplified peaks while reserving mixer headroom.
 knee=.62;headroom=peak-knee
 shaped=np.where(np.abs(shaped)>knee,np.sign(shaped)*(knee+headroom*np.tanh((np.abs(shaped)-knee)/headroom)),shaped)
 return np.round(shaped*32767).astype(np.int16),float(20*np.log10(gain))

def build(work,out):
 work=Path(work);out=Path(out);out.mkdir(parents=True,exist_ok=True)
 dll=ctypes.CDLL(str(work/'dsp-encoder/dspencode.dll'))
 dll.DSPCorrelateCoefs.argtypes=[ctypes.POINTER(ctypes.c_short),ctypes.c_int,ctypes.POINTER(ctypes.c_short)]
 dll.DSPEncodeFrame.argtypes=[ctypes.POINTER(ctypes.c_short),ctypes.c_int,ctypes.POINTER(ctypes.c_ubyte),ctypes.POINTER(ctypes.c_short)]
 source=PROJECT.parent/'package/Wilds-LS-World/romfs/nativeNX/arc/forge/world_audio.arc'
 header,entries=read_arc(source);bank=bytearray(entries[-2].data());req=entries[-1].data()
 assert unpack('I',bank,12)[0]==48 and unpack('I',req,8)[0]==54
 # SBKR v4: 32-byte header followed by a NUL resource path + 80-byte row.
 at=32;rows=[]
 for i in range(48):
  stop=bank.index(0,at);rows.append(bytes(bank[stop+1:stop+81]));at=stop+81
 footer=bank[at:];assert len(footer)==8;bank=bank[:at]
 # SRQR v5: 52-byte header, 132-byte rows and a resource path footer.
 assert unpack('I',req,32)[0]==52+54*132
 request_rows=bytearray(req[52:52+54*132]);template=request_rows[6*132:7*132]
 assert template[14]==6
 original_wave=entries[0].data();events={};report=[];local={}
 pcm_sources={}
 def append_wave(name,data,donor,request_row,target,peak=.9):
  nonlocal bank, request_rows
  if target is None:
   samples,gain=np.rint(data).astype(np.int16),0.
  else:
   samples,gain=mix_personal(data,target,peak)
  payload,coeff,snr=encode(samples,dll);n=len(samples)
  index=len(entries)-2;request=len(request_rows)//132
  pcm_sources[request]=(data,donor,request_row)
  wave_header=bytearray(original_wave[:120]);struct.pack_into('<I',wave_header,8,len(payload)-1)
  struct.pack_into('<f',wave_header,12,n/48000);struct.pack_into('<16h',wave_header,24,*coeff)
  struct.pack_into('<6I',wave_header,96,n,48000,0,0,n-1,0)
  raw=bytes(wave_header)+payload+bytes(80)
  entries.insert(-2,ArcEntry(name,0x79c47b59,0x40000000|len(raw),zlib.compress(raw)))
  # Sound bank keys select the waves. Request byte14 identifies its own row.
  key=61+index;bank_row=bytearray(donor);bank_row[75:78]=bytes([key]*3)
  bank+=name.encode()+b'\0'+bank_row
  row=bytearray(request_row);struct.pack_into('<H',row,0,request)
  struct.pack_into('<H',row,2,0x7f00|key);row[14]=request
  struct.pack_into('<2f',row,36,0.,0.)
  request_rows+=row
  return dict(request=request,bank_index=index,samples=n,snr_db=snr,gain_db=gain)
 def event_samples(path):
  with wave.open(str(path)) as source:
   assert source.getsampwidth()==2 and source.getframerate()==48000
   pcm=np.frombuffer(source.readframes(source.getnframes()),dtype='<i2')
   return pcm.reshape(-1,source.getnchannels()).astype(float).mean(axis=1)
 stem_dir=work/'swing-stems'
 event_levels=json.loads((stem_dir/'levels.json').read_text())
 def event_level(event):
  return event_levels[str(event)]['db']
 def levelled(data,db):
  result=data*10**(db/20)
  assert np.abs(result).max()<=32767,('audio peak',db,np.abs(result).max())
  return result
 def swing_wave(speed,variant):
  # wp03_cmn 2235073740, default equipment switch 3589793960.
  # Velocity RTPC 3811160216 acts on the first of two random containers.
  pitch_cents,velocity_db=((-400,-3.),(0,0.),(400,1.2787))[speed]
  primary=(819315046,15974554)[variant//2]
  accent=(1051305084,948422101)[variant%2]
  base=event_samples(stem_dir/f'{primary}.wav')
  cursor=np.arange(0,len(base),2**(pitch_cents/1200))
  base=np.interp(cursor,np.arange(len(base)),base)*10**(velocity_db/20)
  layer=event_samples(stem_dir/f'{accent}.wav')*10**(-3./20)
  mixed=np.zeros(max(len(base),len(layer)))
  mixed[:len(base)]+=base
  mixed[:len(layer)]+=layer
  return levelled(mixed,event_level(2235073740)),[primary,accent]
 swing_ids=[[0]*4 for _ in range(3)]
 def add_swing(label,speed,variant):
  data,media=swing_wave(speed,variant)
  result=append_wave(rf'sound\se\pl\wp\swo\world\wav\{label}',data,rows[6],template,None)
  swing_ids[speed][variant]=result['request']
  return dict(phase=label,speed=speed,variant=variant,media=media,**result)
 for wav in sorted((work/'event-wav').glob('*.wav'),key=lambda p:int(p.stem)):
  event=int(wav.stem)
  data=event_samples(wav)
  row=append_wave(rf'sound\se\pl\wp\swo\world\wav\wilds{event}',levelled(data,event_level(event)),rows[6],template,None)
  events[event]=row['request'];report.append(dict(event=event,bank_level_db=event_level(event),**row))
 def original_weapon_wave(request):
  row=request_rows[request*132:(request+1)*132]
  key=row[2]
  index=next(i for i,r in enumerate(rows) if key in r[75:78])
  return decode_wav(entries[index].data()),rows[index],row
 for request in SHARED_WEAPON_REQUESTS:
  data,donor,row=original_weapon_wave(request)
  result=append_wave(rf'sound\se\pl\wp\swo\world\wav\wilds_local{request}',data,donor,row,.35)
  local[request]=result['request']
  report.append(dict(shared_request=request,**result))
 # Native Long Sword velocity-controlled swings and weapon-specific sheath.
 # Restore the complete envelopes and static event levels from the source bank.
 native_dir=work/'release-wav'
 native_sources=json.loads((native_dir/'source-events.json').read_text(encoding='utf8'))
 def release_sample(label,level_db,length=None,fade=0):
  data=event_samples(native_dir/f'{label}.wav')
  if length is not None:
   data=data[:length].copy()
   data[-fade:]*=np.linspace(1,0,fade)
  data*=10**(level_db/20)
  assert np.max(np.abs(data))<=32767
  result=append_wave(rf'sound\se\pl\wp\swo\world\wav\release_{label}',data,rows[6],template,None)
  return dict(phase=label,level_db=level_db,source_info=native_sources[label],**result)
 release=[]
 for cut,frame in ((1,19),(2,33),(3,74)):
  result=add_swing(f'release_cut{cut}',1,cut-1)
  result['frame']=frame
  release.append(result)
  report.append(result)
 sheath=release_sample('sheath',event_level(1617378905))
 sheath['frame']=111
 release.append(sheath)
 report.append(sheath)
 release_impacts=[]
 blade_impacts=[]
 for label,requests,level,length,fade in (
     ('impact',release_impacts,event_level(2034526013),None,0),
     ('blade',blade_impacts,event_level(385433533),None,0),
 ):
  for i in range(3):
   result=release_sample(f'{label}{i}',level,length,fade)
   requests.append(result['request'])
   report.append(result)
 # These two locally available events are mapped to this move by its MOT
 # timeline. Apply only fixed level matching, without compressing the envelope.
 release_native={}
 for event,frame,level_db in ((893441629,23,event_level(893441629)),(1387136258,78,event_level(1387136258))):
  if event not in events:continue
  data=event_samples(work/'event-wav'/f'{event}.wav')*10**(level_db/20)
  assert np.max(np.abs(data))<=32767
  result=append_wave(rf'sound\se\pl\wp\swo\world\wav\release_native_f{frame}',data,rows[6],template,None)
  release_native[event]=result['request']
  report.append(dict(release_native_event=event,frame=frame,level_db=level_db,**result))
 # Append the freshly traced equipment events after the stable release selectors.
 action_dir=work/'action-wav'
 action_sources=json.loads((action_dir/'source-events.json').read_text(encoding='utf8'))
 for event in sorted(action_sources,key=int):
  data=event_samples(action_dir/f'{event}.wav')
  row=append_wave(rf'sound\se\pl\wp\swo\world\wav\wilds{event}',levelled(data,event_level(event)),rows[6],template,None)
  events[int(event)]=row['request']
  report.append(dict(event=int(event),bank_level_db=event_level(event),source_info=action_sources[event],**row))
 # Helm Breaker and Iai both use the stock weapon HitEffect=9 bank.
 # Preserve the full source envelopes, including overlapping tails.

 helm_iai_dir=work/'helm-iai-wav'
 for event,source_info in json.loads((helm_iai_dir/'source-events.json').read_text()).items():
  data=event_samples(helm_iai_dir/f'{event}.wav')
  row=append_wave(rf'sound\se\pl\wp\swo\world\wav\wilds{event}',levelled(data,event_level(event)),rows[6],template,None)
  events[int(event)]=row['request']
  report.append(dict(event=int(event),bank_level_db=event_level(event),source_info=source_info,**row))
 follow_requests={}
 for move,level,length,fade in (('helm',event_level(2034526013),None,0),('iai',event_level(2034526013),None,0)):
  follow_requests[move]=[]
  for i in range(3):
   label=f'impact{i}'
   data=event_samples(native_dir/f'{label}.wav')
   if length is not None:
    data=data[:length].copy()
    data[-fade:]*=np.linspace(1,0,fade)
   data*=10**(level/20)
   row=append_wave(rf'sound\se\pl\wp\swo\world\wav\{move}_{label}',data,rows[6],template,None)
   follow_requests[move].append(row['request'])
   report.append(dict(phase=f'{move}_{label}',level_db=level,source_info=native_sources[label],**row))
 confirmations={}
 for label,source_name,level_db in (('iai_counter','counter-source.json',event_level(415932214)),
                                    ('foresight_counter','foresight-source.json',event_level(1000549333))):
  source_info=json.loads((helm_iai_dir/source_name).read_text(encoding='utf8'))
  pcm=event_samples(helm_iai_dir/(label.replace('_','-')+'.wav'))*10**(level_db/20)
  assert np.max(np.abs(pcm))<=32767
  row=append_wave(rf'sound\se\pl\wp\swo\world\wav\{label}',pcm,rows[6],template,None)
  confirmations[label]=row['request']
  report.append(dict(phase=label,level_db=level_db,source_info=source_info,**row))
 # Exact source clips and matched contact envelopes from Wilds' weapon bank.
 contact_dir=work/'native-hit-wav'
 contact_sources=json.loads((contact_dir/'source-events.json').read_text(encoding='utf8'))
 contact={}
 for label,source_info in contact_sources.items():
  data=event_samples(contact_dir/f'{label}.wav')
  db={'thrust_charge':-4.,'thrust_cut':3.}.get(label,0.)
  result=append_wave(rf'sound\se\pl\wp\swo\world\wav\wilds_{label}',levelled(data,db),rows[6],template,None)
  contact[label]=result['request']
  report.append(dict(phase=label,bank_level_db=db,source_info=source_info,**result))
 for speed in range(3):
  for variant in range(4):
   if not swing_ids[speed][variant]:report.append(add_swing(f'wind_{speed}_{variant}',speed,variant))
 # These moves run over GU's combat mix. Keep their original samples and
 # timing, but match active loudness to clearly audible weapon effects.
 def louder(request,label,db):
  data,donor,row=pcm_sources[request]
  sample=append_wave(rf'sound\se\pl\wp\swo\world\wav\loud_{label}',data,donor,row,10**(db/20),peak=.84)
  report.append(dict(phase=label,source_request=request,target_active_dbfs=db,**sample))
  return sample['request']
 release_speed_swings=[[louder(request,f'rel_wind{s}_{v}',-14)
                         for v,request in enumerate(group)] for s,group in enumerate(swing_ids)]
 blade_loud=[louder(request,f'blade{i}',-13) for i,request in enumerate(blade_impacts)]
 follow_requests['helm']=[louder(request,f'helm_blade{i}',-13)
                          for i,request in enumerate(follow_requests['helm'])]
 for label,request in list(contact.items()):
  target=-16 if label=='thrust_charge' else -14 if label=='thrust_cut' else -12
  contact[label]=louder(request,label,target)
 helm_levels={1900417456:-16,2000639191:-17,3748751524:-14,962180282:-20}
 helm_loud={event:louder(events[event],f'helm_{event}',db) for event,db in helm_levels.items()}
 release_loud={request:louder(request,f'rel_{request}',-16) for request in release_native.values()}
 release_loud[sheath['request']]=louder(sheath['request'],'rel_sheath',-19)
 count=len(report);struct.pack_into('<I',bank,12,48+count)

 # Native sSoundBank bounds its group using a second packed 10-bit count.
 # Updating just the outer count loads all waves but refuses new selectors.
 group=unpack('I',bank,20)[0];assert (group>>17)&1023==48
 struct.pack_into('<I',bank,20,(group&~(1023<<17))|((48+count)<<17));bank+=footer
 request_header=bytearray(req[:52]);struct.pack_into('<I',request_header,8,54+count)
 for at in (32,36,40,44,48):struct.pack_into('<I',request_header,at,unpack('I',req,at)[0]+132*count)
 request_footer=bytearray(req[52+54*132:])
 # The bank path table stores absolute file offsets too, including its own
 # first string pointer. A stale pointer silently leaves every bank NULL.
 string_offset=unpack('I',request_footer)[0]
 assert req[string_offset:].startswith(b'sound\\se\\pl\\wp\\swo\\world\\snd_world\0')
 struct.pack_into('<I',request_footer,0,string_offset+132*count)
 request=bytes(request_header)+request_rows+request_footer
 relocated=unpack('I',request,unpack('I',request,32)[0])[0]
 assert request[relocated:].startswith(b'sound\\se\\pl\\wp\\swo\\world\\snd_world\0')
 for index,raw in ((-2,bank),(-1,request)):
  old=entries[index];entries[index]=ArcEntry(old.name,old.kind,(old.flags&0xe0000000)|len(raw),zlib.compress(raw))
 arc=bytearray(header+bytes(80*len(entries)));struct.pack_into('<H',arc,6,len(entries))
 for i,e in enumerate(entries):
  arc+=bytes((-len(arc))%16);offset=len(arc)
  struct.pack_into('<64s4I',arc,12+80*i,e.name.encode(),e.kind,len(e.compressed),e.flags,offset);arc+=e.compressed
 dest=out/'world_audio.arc';dest.write_bytes(arc);_,check=read_arc(dest)
 assert len(check)==50+count and all(check[i].data()==read_arc(source)[1][i].data() for i in range(48))
 swings=[item['request'] for item in release[:3]]
 events[1617378905]=sheath['request']
 cues=[]
 trace=json.loads((work/'wilds-audio-cues.json').read_text())
 for c in trace:
  event=c['event']
  if event not in events:continue
  # The focus opener's F150 gear cue belongs to recovery beyond its GU clip.
  if c['source']==905 and c['frame']==150:continue
  frame=150 if c['source']==918 and c['frame']==165 else c['frame']
  request=events[event]
  if c['source']==904:
   if event==1617378905:continue  # The release table already plays this cue.
   request=release_native.get(event,request)
  if c['source']==904:request=release_loud.get(request,request)
  cues.append((c['source'],frame,request))
  if c['source']==901:
   # Keep the existing opener's gear and spirit layers on the other variants.
   cues.extend((source,frame,request) for source in (920,921))
 # The native blade system selects a whoosh by sword speed. Select its
 # variants at each GU swing frame and keep the traced special layers above.
 swing_frames={
  901:(15,33), 902:(13,33,84), 903:(30,), 905:(44,), 906:(27,41), 907:(20,44,59),
  910:(26,), 911:(26,), 912:(25,60), 913:(8,36), 914:(10,), 917:(25,),
  918:(15,40), 920:(13,31), 921:(25,), 922:(28,42), 923:(20,56), 924:(20,56), 925:(22,60),
 }
 for source,frames in swing_frames.items():
  for i,frame in enumerate(frames):
   cues.append((source,frame,swings[((901 if source in (920,921) else source)+i)%len(swings)]))
 cues.append((908,12,events[3638200284]))
 for source,effect in ((922,40),(923,49),(924,53),(925,52)):
  cues.append((source,effect,events[286588833 if source==922 else 4027867090]))
 cues.append((925,39,events[1720684994]))
 cues.extend((904,item['frame'],release_loud.get(item['request'],item['request'])) for item in release)
 assert len(cues)==len(set(cues))<=128
 text='#pragma once\n/* Wilds sound requests and ported motion trigger frames. */\n'
 text+=f'#define WILDS_AUDIO_RESOURCE_COUNT {50+count}u\n#define WILDS_AUDIO_REQUEST_COUNT {54+count}u\n'
 text+='static const unsigned wilds_speed_swings[3][4]={'+','.join('{'+','.join(map(str,row))+'}' for row in swing_ids)+'};\n'
 text+='static const unsigned wilds_release_speed_swings[3][4]={'+','.join('{'+','.join(map(str,row))+'}' for row in release_speed_swings)+'};\n'
 text+='static const unsigned wilds_combat_blade_impact_requests[3]={'+','.join(map(str,blade_loud))+'};\n'
 text+='static const unsigned wilds_swing_requests[]={'+','.join(map(str,swings))+'};\n'
 text+='static const unsigned wilds_release_impact_requests[]={'+','.join(map(str,release_impacts))+'};\n'
 text+='static const unsigned wilds_release_blade_impact_requests[]={'+','.join(map(str,blade_impacts))+'};\n'
 for name,event in dict(SHEATH=1617378905,DRAW=2550263896,HANDLING=1500054012,
                        SPIRIT=4027867090,COUNTER=1387136258,
                        EQUIPMENT=3638200284).items():
  text+=f'#define WILDS_SOUND_{name} {events[event]}u\n'
 for move,requests in follow_requests.items():
  text+=f'static const unsigned wilds_{move}_impact_requests[3]={{'+','.join(map(str,requests))+'};\n'
 for name,event in dict(HELM=1900417456,HELM_JUMP=2000639191,HELM_CUT=3748751524,
                        HELM_LAND_GEAR=962180282,IAI_CUT=1387136258,IAI_RECOVERY=1720684994).items():
  text+=f'#define WILDS_SOUND_{name} {release_native[event] if name=="IAI_CUT" else helm_loud.get(event,events[event])}u\n'
 for label,request in confirmations.items():
  text+=f'#define WILDS_SOUND_{label.upper()} {request}u\n'
 for label in ('thrust_charge','thrust_cut'):
  text+=f'#define WILDS_SOUND_{label.upper()} {contact[label]}u\n'
 for group in ('body','shell'):
  ids=','.join(str(contact[f'{group}{i}']) for i in range(3))
  text+=f'static const unsigned wilds_{group}_impact_requests[3]={{{ids}}};\n'
 text+=f'#define WILDS_SOUND_IAI_WHOOSH {swings[2]}u\n'
 text+='typedef struct {unsigned source;float frame;unsigned request;} WildsAudioCue;\nstatic const WildsAudioCue wilds_audio_cues[]={\n'
 text+=''.join(f'{{{s},{f:.6f}f,{r}}},\n' for s,f,r in sorted(cues))+'};\n'
 (out/'wilds_audio_data.h').write_text(text)
 (out/'audio-manifest.json').write_text(json.dumps(dict(resources=len(entries),events=report,cues=cues,unresolved=[c for c in trace if c['event'] is not None and c['event'] not in events]),indent=2))
 print('Built',dest,'events',count,'cues',len(cues),'minimum DSP SNR',min(r['snr_db'] for r in report))

if __name__=='__main__':
 import sys
 build(sys.argv[1],sys.argv[2])

