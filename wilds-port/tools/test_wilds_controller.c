/* Host tests execute the shipping controller, including its private helpers. */
#include <assert.h>
#include <stdio.h>
#define ML_WILDS_MOVES 1
#define ML_WORLD_INPUT_VM 1
#define ML_ASCENDANCE_PROTOTYPE 1
#include "world_core.c"
static WObservation obs(void){return (WObservation){.base={.actor=1,.epoch=1,.gauge=100,.heading=0,.simulation_delta=1},.state=16,.alive=1,.drawn=1,.grounded=1,.resources_ready=1,.rank=3};}
static WDecision tick(WState *s,WObservation *o,unsigned held,float frame)
{
 o->base.held=held;o->base.animation_frame=frame;o->base.sample_sequence++;
 if(s->move==W_CHIJIN){
  o->base.eligible=1;
  s->actor=o->base.actor;s->epoch=o->base.epoch;
  o->base.native_action=w_action(s->move);o->base.motion=0x500+s->slot;
  WDecision d=w_foresight_press(s,o);
  if(d.kind!=W_KEEP)return d;
 }
 return wilds_step(s,o);
}

static void valor_routes(void)
{
 const unsigned recovery[][3]={
  {86,190,32},{86,184,22},{87,130,38},{99,103,38},{100,129,50},{101,128,50},
  {93,103,38},{94,103,38},{95,129,50},{96,129,50},{97,128,50},{98,128,50},
  {110,103,38},{111,129,50},{112,128,50},{113,103,38},{114,129,50},{115,128,50},
  {120,103,38},{121,103,38},{122,103,38},{123,129,50},{124,129,50},{125,129,50},
  {126,128,50},{127,128,50},{128,128,50},
  {104,106,38},{23,106,44},{24,106,44},{105,108,95},{119,107,38},{102,130,46},{103,131,46},
  {22,105,38},{118,105,38},{143,103,76},{144,103,76},
  {130,192,44},{130,104,62},{116,191,34},{116,185,22},{117,186,110},
  {134,190,32},{134,184,22},{135,194,50},{135,185,22},{136,191,34},{136,185,22},
  {137,194,50},{137,185,22},{138,191,34},{138,185,22},{139,194,50},{139,185,22},
  {140,191,34},{140,185,22},{141,194,50},{141,185,22}
 };
 const unsigned keys[]={ML_X,ML_R|ML_X,ML_R|ML_A,ML_R|ML_Y,ML_R,ML_R|ML_B,ML_X|ML_A};
 const unsigned expected[]={W_CHIJIN,W_THRUST,W_FORESIGHT,W_FOCUS,0,0,0};
 for(unsigned i=0;i<sizeof(recovery)/sizeof(*recovery);i++){
  const unsigned *row=recovery[i];
  for(unsigned k=0;k<sizeof(keys)/sizeof(*keys);k++){
   WState s={0};WObservation o=obs();o.valor=1;
   o.base.native_action=row[0];o.base.motion=1280+row[1];o.base.animation_frame=row[2];
   o.base.held=keys[k];
   assert(!w_valor_chord(16,row[0],o.base.motion,row[2]-1,keys[k],1));
   o.valor_move=w_valor_chord(16,row[0],o.base.motion,row[2],keys[k],1);
   assert(o.valor_move==expected[k]);
   if(keys[k]==(ML_R|ML_Y)){o.base.held=0;o.valor_move=0;o.focus_strike=1;}
   assert(world_command_step(&s,&o,0,NULL).move==expected[k]);
  }
 }
 WState guard={0};
 WObservation gp=obs();
 gp.valor=1;gp.rank=0;gp.base.held=ML_R;
 gp.base.native_action=108;gp.base.motion=0x583;gp.base.animation_frame=55;
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_SECOND).kind==W_KEEP);
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_THIRD).kind==W_KEEP);
 gp.base.native_action=116;gp.base.motion=0x5b9;gp.base.animation_frame=37;
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_SECOND).kind==W_KEEP);
 gp.base.animation_frame=38;
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_THIRD).kind==W_KEEP);
 WDecision release=w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_SECOND);
 assert(release.move==W_RELEASE&&release.rank_delta==0&&guard.drawn_release);
 gp.base.motion=0x500+guard.slot;gp.base.native_action=w_action(W_RELEASE);
 assert(tick(&guard,&gp,0,W_DRAWN_RELEASE_END-1).kind==W_KEEP);
 assert(tick(&guard,&gp,0,W_DRAWN_RELEASE_END).kind==W_IDLE&&!guard.drawn_release);
 gp.base.native_action=117;gp.base.motion=0x5ba;gp.base.animation_frame=109;
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_THIRD).kind==W_KEEP);
 gp.base.animation_frame=110;
 assert(w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_SECOND).kind==W_KEEP);
 release=w_valor_chain_release(&guard,&gp,W_VALOR_RELEASE_THIRD);
 assert(release.move==W_RELEASE&&release.rank_delta==0&&guard.drawn_release);
 /* Each independently cancellable imported strike accepts the partner button
  * in either order. The wound-confirm and airborne phases advance separately. */
 const unsigned strikes[][2]={
  {W_CHIJIN,901},{W_CHIJIN,920},{W_CHIJIN,921},{W_CHIJIN,906},{W_CHIJIN,907},
  {W_SLIDE,903},{W_SLIDE,910},{W_SLIDE,911},{W_SLIDE,912},
  {W_SLIDE,922},{W_SLIDE,923},{W_SLIDE,924},{W_SLIDE,925},
  {W_CHARGE,902},{W_CHARGE,917},{W_RELEASE,904},{W_FOCUS,905},
  {W_FOCUS,914},{W_FOCUS,927},{W_FOCUS,928},{W_FOCUS,929},{W_FORESIGHT,918}
 };
 for(unsigned i=0;i<sizeof(strikes)/sizeof(*strikes);i++){
  for(unsigned k=0;k<2;k++)for(unsigned r_first=0;r_first<2;r_first++){
   unsigned partner=k?ML_A:ML_X;
   WObservation o=obs();o.valor=1;
   WState s={.move=strikes[i][0],.source=strikes[i][1],.wilds_previous=r_first?ML_R:partner};
   WDecision d=tick(&s,&o,ML_R|partner,wilds_clip(s.source)->cancel);
   assert(d.move==(k?W_FORESIGHT:W_THRUST));
  }
 }
 for(unsigned blue=0;blue<=1;blue++){
  WObservation o=obs();o.valor=blue;
  WState s={.move=W_SLIDE,.source=903};
  assert(wilds_spirit(&s,&o,0).native_action==(blue?116:23));
  s=(WState){.move=W_FOCUS,.source=914};
  assert(wilds_spirit(&s,&o,0).native_action==(blue?130:26));
  s=(WState){.move=W_CHARGE,.source=908,.wilds_stage=2};
  assert(tick(&s,&o,0,1).kind==W_KEEP);
  assert(tick(&s,&o,ML_R,29).kind==W_KEEP);
  assert(tick(&s,&o,ML_R,30).native_action==(blue?116:23));
 }
 const unsigned returns[][2]={{W_CHIJIN,907},{W_FOCUS,905},{W_FOCUS,914},
                              {W_FOCUS,927},{W_FOCUS,928},{W_FOCUS,929}};
 for(unsigned i=0;i<sizeof(returns)/sizeof(*returns);++i){
  unsigned move=returns[i][0],source=returns[i][1],cancel=wilds_clip(source)->cancel;
  WObservation o=obs();o.valor_style=o.valor=1;
  WState s={.move=move,.source=source};
  assert(tick(&s,&o,ML_R,cancel-12).kind==W_KEEP);
  assert(tick(&s,&o,ML_R|ML_A,cancel-10).kind==W_KEEP);
  WDecision d=tick(&s,&o,0,cancel);
  assert(d.kind==W_ENTER&&d.source==918);
  s=(WState){.move=move,.source=source};
  assert(tick(&s,&o,ML_R|ML_A,cancel-10).kind==W_KEEP);
  o.valor=0;
  assert(tick(&s,&o,0,cancel).kind==W_KEEP);
  o.valor=1;o.base.magnitude=100;o.base.input_heading=16384;
  s=(WState){.move=move,.source=source};
  assert(tick(&s,&o,ML_R,cancel-12).kind==W_KEEP);
  o.base.magnitude=0;o.base.input_heading=0;
  for(unsigned early=11;early>0;--early)
   assert(tick(&s,&o,0,cancel-early).kind==W_KEEP);
  d=tick(&s,&o,0,cancel);
  assert(d.kind==W_NATIVE&&d.native_action==130&&d.valor_slide&&d.valor_slide_input==16384);
 }
}

static void red_x_recovery(void)
{
 const unsigned native[][3]={
  {16,104,56},{17,104,56},{4,105,24},{25,107,24},{22,105,38},
  {23,106,44},{26,108,24},{99,103,38},{130,192,44},{143,103,76},
 };
 for(unsigned i=0;i<sizeof(native)/sizeof(*native);i++){
  for(unsigned overlap=0;overlap<=(i<2);overlap++){
   WObservation o=obs();
   WState s={.spirit_crimson_held=overlap?ML_R:0};
   o.base.native_action=native[i][0];o.base.motion=0x500+native[i][1];
   o.base.held=overlap?(ML_R|ML_X):ML_X;o.base.animation_frame=3;
   assert(world_command_step(&s,&o,0,NULL).kind==W_KEEP);
   o.base.held=0;o.base.animation_frame=native[i][2]-1;
   o.base.simulation_delta=5;o.base.sample_sequence++;
   assert(world_command_step(&s,&o,0,NULL).kind==W_KEEP);
   o.base.animation_frame++;o.base.sample_sequence++;
   WDecision d=world_command_step(&s,&o,0,NULL);
   assert(d.kind==W_ENTER&&d.source==W_CRIMSON_STATIONARY);
  }
 }
 /* GU sends the X thrust request at F55.5 even when the pad is
  * already neutral. Preserve its steering snapshot until F56. */
 WObservation reserve=obs();WState buffered={0};
 reserve.base.native_action=16;reserve.base.motion=0x568;reserve.base.animation_frame=55.5f;
 w_reserve_spirit_crimson(&buffered,&reserve.base);
 assert(world_command_step(&buffered,&reserve,0,NULL).kind==W_KEEP);
 reserve.base.animation_frame=56;reserve.base.sample_sequence++;
 assert(world_command_step(&buffered,&reserve,0,NULL).source==W_CRIMSON_STATIONARY);

 reserve=obs();buffered=(WState){0};
 reserve.base.native_action=16;reserve.base.motion=0x501;
 reserve.base.held=ML_X;
 assert(world_command_step(&buffered,&reserve,0,NULL).kind==W_KEEP);
 reserve.base.held=0;reserve.base.motion=0x568;
 reserve.base.animation_frame=56;reserve.base.sample_sequence++;
 assert(world_command_step(&buffered,&reserve,0,NULL).source==W_CRIMSON_STATIONARY);
 const unsigned custom[][2]={
  {W_SLIDE,903},{W_SLIDE,W_SPIRIT_TWO_FORWARD},{W_CHARGE,902},
  {W_FOCUS,905},{W_FOCUS,914},{W_FORESIGHT,918},
 };
 for(unsigned i=0;i<sizeof(custom)/sizeof(*custom);i++){
  WObservation o=obs();WState s={.move=custom[i][0],.source=custom[i][1]};
  unsigned frame=wilds_clip(s.source)->cancel;
  assert(tick(&s,&o,ML_X,frame-20).kind==W_KEEP);
  assert(tick(&s,&o,0,frame-1).kind==W_KEEP);
  assert(tick(&s,&o,0,frame).source==W_CRIMSON_STATIONARY);
 }
 WObservation o=obs();
 WState s={.move=W_CHIJIN,.source=W_CRIMSON_STATIONARY,.spirit_crimson_repeat=1};
 o.base.held=ML_R|ML_X;o.base.animation_frame=4;
 assert(world_command_step(&s,&o,s.source,w_event(s.source,4)).kind==W_KEEP);
 o=obs();s=(WState){0};o.base.native_action=16;o.base.motion=0x568;
 o.valor=1;o.base.held=ML_R|ML_X;o.base.animation_frame=3;o.valor_move=W_THRUST;
 assert(world_command_step(&s,&o,0,NULL).move==W_THRUST);
}

static void directional_spirit(void)
{
 WObservation o=obs();WState s={0};
 o.drawn=0;o.state=2;o.base.native_action=2;o.base.magnitude=100;
 o.base.held=ML_R;
 assert(!w_sheathed_slide(&o)&&w_wilds_enter(&s,&o,W_SLIDE).kind==W_KEEP);
 o.base.held=ML_R|ML_X|ML_A;
 assert(w_sheathed_slide(&o)&&w_wilds_enter(&s,&o,W_SLIDE).move==W_SLIDE);
 o.base.magnitude=0;
 assert(!w_sheathed_slide(&o));
 o=obs();o.rank=2;o.base.magnitude=100;
 s=(WState){.move=W_CHARGE,.source=915,.wilds_stage=2,.wilds_previous=ML_R};
 WDecision d=tick(&s,&o,0,50);
 assert(d.move==W_SLIDE&&d.source==W_SPIRIT_TWO_FORWARD&&d.gauge_delta==-15);

 for(unsigned direction=0;direction<2;direction++){
  o=obs();o.rank=2;o.base.magnitude=direction?100:0;
  s=(WState){.move=W_CHARGE,.source=902,.wilds_stage=3};
  assert(tick(&s,&o,ML_R,95).kind==W_KEEP);
  o.base.simulation_delta=6;
  d=tick(&s,&o,0,105);
  assert(d.source==(direction?903:917)&&d.gauge_delta==(direction?-10:-25));
 }
}

static void draw_and_floor_routes(void)
{
 for(unsigned action=20;action<=21;action++){
  WObservation o=obs();WState s={0};
  o.base.native_action=action;o.base.motion=0x57e;o.base.held=ML_X;
  assert(world_command_step(&s,&o,0,NULL).kind==W_KEEP);
  o.base.motion=0x568;o.base.held=0;o.base.animation_frame=55;
  o.base.sample_sequence++;
  assert(world_command_step(&s,&o,104,w_event(104,55)).kind==W_KEEP);
  o.base.animation_frame=56;o.base.sample_sequence++;
  assert(world_command_step(&s,&o,104,w_event(104,56)).source==W_CRIMSON_STATIONARY);
 }
 for(unsigned move=W_FORESIGHT;move<=W_FOCUS;move++)
  assert(w_ground_step_move(move)==(move<W_RISE||move>W_DOWN));
 const float steps[]={-.6f,-.5f,0.f,29.f,30.f,72.f,120.f,120.1f,180.f};
 for(unsigned i=0;i<sizeof(steps)/sizeof(*steps);i++)
  assert(w_ground_step_height(steps[i])==(i>0&&i<7));
 WObservation o=obs();WState s={0};
 WDecision d=w_debug_enter(&s,&o,W_LAND);
 s.actor=1;s.epoch=1;s.helm_cost_pending=1;s.rank=2;
 o.state=4;o.grounded=0;o.base.native_action=1;o.base.motion=0x500+d.slot;
 d=w_step(&s,&o);
 assert(d.kind==W_ABORT&&d.rank_delta==-1&&!s.move);
 assert(!w_step(&s,&o).rank_delta);
}

static void charge_routes(void)
{
 const unsigned sources[]={901,920,921,906,907,903,910,911,912,922,923,924,925,902,914,927,928,929,930};
 for(unsigned i=0;i<sizeof(sources)/sizeof(*sources);i++){
  WObservation o=obs();WState s={.source=sources[i],.move=W_CHARGE};
  o.rank=2;
  unsigned frame=wilds_charge_window(&s,&o);assert(frame);
  s.wilds_bridge_hold=.15f;
  assert(tick(&s,&o,ML_R,frame-1).kind==W_KEEP);
  o.base.simulation_delta=2;
  WDecision d=tick(&s,&o,ML_R,frame);
  assert(d.source==908);
 }
 WObservation o=obs();WState s={0};w_debug_enter(&s,&o,W_BIG);
 s.counter_success=1;o.base.held=ML_R;o.base.animation_frame=40;o.base.sample_sequence++;
 assert(wilds_iai_follow(&s,&o).kind==W_KEEP);
 for(unsigned i=0;i<3;i++){o.base.animation_frame+=10;o.base.sample_sequence++;o.base.simulation_delta=10;
  WDecision d=wilds_iai_follow(&s,&o);assert(i==2?d.source==908:d.kind==W_KEEP);}
}
static void native_third_charge_recovery(void)
{
 WObservation art=obs();WState after_art={0};
 art.rank=2;art.base.gauge=0;art.base.held=ML_R;art.state=1;
 WDecision art_charge=w_art_spirit(&after_art,&art,0,1);
 assert(art_charge.kind==W_ENTER&&art_charge.source==908&&after_art.wilds_stage==1);
 w_reset(&after_art);art.base.magnitude=100;
 art_charge=w_art_spirit(&after_art,&art,1,1);
 assert(art_charge.kind==W_ENTER&&art_charge.source==908&&after_art.wilds_stage==1);
 for(unsigned gauge=0;gauge<=25;gauge+=25){
  for(unsigned after_idle=0;after_idle<=1;after_idle++){
   WState s={0};WObservation o=obs();o.rank=2;o.base.gauge=gauge;
   o.base.animation_frame=155;o.base.sample_sequence=100;
   assert(wilds_bridge(&s,&o,108).kind==W_KEEP);
   for(unsigned i=1;i<=11;i++){
    unsigned idle=i>4||after_idle;
    o.state=idle?1:16;o.base.held=ML_R;o.base.sample_sequence+=2;
    WDecision d=wilds_bridge(&s,&o,idle?1:108);
    if(i<11)assert(d.kind==W_KEEP);
    else assert(d.kind==W_ENTER&&d.source==908&&s.wilds_stage==4);
   }
  }
  WState s={0};WObservation o=obs();o.rank=2;o.base.gauge=gauge;
  o.base.animation_frame=155;o.base.sample_sequence=100;
  assert(wilds_bridge(&s,&o,108).kind==W_KEEP);
  o.state=1;o.base.held=ML_R;o.base.sample_sequence+=2;
  assert(wilds_bridge(&s,&o,1).kind==W_KEEP);
  o.base.held=0;o.base.sample_sequence+=2;
  WDecision d=wilds_bridge(&s,&o,1);
  assert(gauge?d.source==917&&d.gauge_delta==-25:d.kind==W_KEEP);
 }
}
static void charge_foresight_and_round_spin(void)
{
 const unsigned charge_sources[]={908,909,915,902,917};
 const unsigned gates[]={20,0,0,105,55};
 for(unsigned i=0;i<sizeof(charge_sources)/sizeof(*charge_sources);i++){
  WObservation o=obs();WState s={.actor=1,.epoch=1,.move=W_CHARGE,.source=charge_sources[i],.consumed=ML_R};
  s.slot=w_clip(s.source)->slot;
  o.base.eligible=1;o.base.motion=0x500+s.slot;o.base.native_action=w_action(W_CHARGE);
  o.base.held=ML_R|ML_A;
  if(gates[i]){
   o.base.animation_frame=gates[i]-5;
   assert(w_foresight_press(&s,&o).kind==W_KEEP);
   o.base.held=ML_R;
  }
  o.base.animation_frame=gates[i];
  assert(w_foresight_press(&s,&o).move==W_FORESIGHT);
 }
 WObservation o=obs();
 WState s={.move=W_GROUND,.source=109,.slot=148,.red_foresight_chain=1,.wilds_previous=ML_R,.consumed=ML_R};
 o.base.motion=0x500+148;o.base.magnitude=100;o.base.held=ML_R;
 o.base.animation_frame=8;
 assert(wilds_round_spin(&s,&o).kind==W_KEEP&&!s.round_follow.pending);
 s.consumed=o.base.held=0;
 assert(wilds_round_spin(&s,&o).kind==W_KEEP);
 o.base.held=ML_R;o.base.animation_frame=13;
 assert(wilds_round_spin(&s,&o).kind==W_KEEP&&s.round_follow.pending);
 o.base.held=0;o.base.animation_frame=50;
 assert(wilds_round_spin(&s,&o).source==W_CRIMSON_SPIN);
 s=(WState){.move=W_GROUND,.source=109,.slot=148,.red_foresight_chain=1};
 o.base.animation_frame=106;o.base.sample_sequence=100;
 assert(wilds_red_round_step(&s,&o).kind==W_IDLE);
 o.state=1;o.base.native_action=0;o.base.held=ML_R;o.base.sample_sequence=106;
 assert(wilds_round_spin(&s,&o).source==W_CRIMSON_SPIN);
}
static void red_foresight_focus_toggle(void)
{
 for(unsigned enable_after_r=0;enable_after_r<2;enable_after_r++){
  WObservation o=obs();
  WState s={.move=W_FORESIGHT,.source=918,.counter_success=1};
  o.ascendance_active=1;
  o.base.magnitude=100;
  o.focus_active=!enable_after_r;
  assert(tick(&s,&o,0,22).kind==W_KEEP);
  assert(tick(&s,&o,ML_R,25).kind==W_KEEP);
  o.focus_active=enable_after_r;
  assert(tick(&s,&o,0,30).kind==W_KEEP);
  o.focus_active=0;
  assert(tick(&s,&o,0,40).kind==W_KEEP);
  float cancel=wilds_clip(918)->cancel;
  WDecision d=tick(&s,&o,0,cancel);
  assert(d.kind==W_ENTER&&d.source==109&&s.red_foresight_chain);
 }
 WObservation o=obs();
 WState s={.move=W_FORESIGHT,.source=918,.counter_success=1};
 o.ascendance_active=o.focus_active=1;
 assert(tick(&s,&o,0,20).kind==W_KEEP);
 o.focus_active=0;
 for(unsigned i=25;i<40;i++)assert(tick(&s,&o,ML_R,i).kind==W_KEEP);
 assert(tick(&s,&o,ML_R,40).move==W_CHARGE);
}
static void iai_held_entry(void)
{
 WState s={0};WObservation o=obs();o.base.held=ML_R;
 w_debug_enter(&s,&o,W_BIG);s.consumed=ML_R;
 for(unsigned i=1;i<=37;i++){
  o.base.sample_sequence++;o.base.animation_frame=i+10;
  WDecision d=world_command_step(&s,&o,s.source,w_event(s.source,o.base.animation_frame));
  if(d.move==W_CHARGE){assert(i>=36);return;}
  assert(d.kind==W_KEEP);
 }
 assert(0 && "sustained Iai input failed to charge");
}
static void iai_thrust_priority(void)
{
 const unsigned thrust=ML_R|ML_X;
 const struct {
  unsigned opener,first,second;
  float frame;
  unsigned finish;
 } cases[]={
  {0,thrust,0,40,0},
  {0,ML_R,thrust,40,0},
  {0,ML_X,thrust,40,0},
  {ML_R,thrust,0,40,0},
  {0,thrust,0,36,0},
  {0,ML_R,thrust,40,1},
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
  WState s={.actor=1,.epoch=1};WObservation o=obs();o.base.held=cases[i].opener;
  w_debug_enter(&s,&o,W_BIG);s.consumed=cases[i].opener;s.counter_success=1;
  o.base.native_action=w_action(W_BIG);o.base.motion=0x500+s.slot;
  o.base.held=cases[i].first;o.base.animation_frame=cases[i].frame;o.base.sample_sequence++;
  WDecision d=w_step(&s,&o);
  if(cases[i].second||cases[i].frame<37){
   assert(d.kind==W_KEEP);
   o.base.held=cases[i].second;o.base.animation_frame+=2;o.base.sample_sequence++;
   o.base.simulation_delta=2;o.base.motion_finished=cases[i].finish;
   d=w_step(&s,&o);
  }
  assert(d.kind==W_ENTER&&d.move==W_THRUST);
 }
}
static void valor_foresight_routes(void)
{
 for(unsigned active=0;active<=1;active++)for(unsigned hold=0;hold<=1;hold++){
  WObservation o=obs();WState s={.actor=1,.epoch=1};
  o.valor_style=1;o.valor=active;o.rank=active?3:1;
  o.base.held=ML_R|ML_A;s.consumed=ML_R|ML_A;s.previous_held=o.base.held;
  w_debug_enter(&s,&o,W_FORESIGHT);
  o.base.motion=0x500+s.slot;o.base.native_action=w_action(W_FORESIGHT);
  unsigned cancel=active?80:90;
  o.base.held=ML_R;o.counter=1;o.base.animation_frame=28;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP&&!s.valor_foresight_r); /* R from the opener. */
  o.base.held=0;o.base.sample_sequence++;o.counter=0;
  assert(w_step(&s,&o).kind==W_KEEP);
  o.base.held=ML_R;o.base.animation_frame=35;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP&&s.valor_foresight_r);
  o.base.held=hold?ML_R:0;o.base.animation_frame=cancel-1;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP&&s.valor_foresight_r);
  o.base.animation_frame=cancel;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP); /* Counter without a return hit. */
  o.contact=1;o.base.sample_sequence++;
  WDecision d=w_step(&s,&o);
  assert(d.kind==W_NATIVE&&d.native_action==(active?116:26));
  assert(d.valor_foresight_second==active&&s.wilds_native_r_spent==hold&&!s.valor_foresight_r);
  if(!active&&hold){
   o.contact=0;o.base.native_action=26;o.base.motion=0x56c;
   for(unsigned sample=0;sample<150;sample++){
    o.base.animation_frame=(float)sample;o.base.sample_sequence++;
    assert(w_step(&s,&o).kind==W_KEEP);
   }
  }
 }
 for(unsigned active=0;active<=1;active++){
  WObservation o=obs();WState s={.actor=1,.epoch=1};
  o.valor_style=1;o.valor=active;o.rank=active?3:1;
  w_debug_enter(&s,&o,W_FORESIGHT);
  o.base.motion=0x500+s.slot;o.base.native_action=w_action(W_FORESIGHT);
  o.base.held=ML_R;o.contact=1;o.base.animation_frame=91;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP&&!s.valor_foresight_r); /* Hit without a counter. */
 }
}
static void valor_radiance_bound_slots(void)
{
 WState s={0};WObservation o=obs();o.valor_style=o.valor=1;
 assert(w_radiance_enter(&s,&o,W_SHEATHE).kind==W_KEEP);
 assert(w_radiance_enter(&s,&o,W_SMALL).kind==W_KEEP);
 assert(w_radiance_enter(&s,&o,W_BIG).kind==W_KEEP);
 assert(w_valor_radiance_enter(&s,&o,239).kind==W_KEEP);
 WDecision d=w_valor_radiance_enter(&s,&o,RADIANCE_FLASH_SLOT);
 assert(d.move==W_BIG&&d.slot==RADIANCE_FLASH_SLOT&&!s.counter_success);
 o.valor=0;
 d=w_valor_radiance_enter(&s,&o,PURSUIT_FIRST_SLOT);
 assert(d.move==W_SMALL&&d.slot==PURSUIT_FIRST_SLOT&&!s.counter_success);
}
static void valor_guard_recovery_routes(void)
{
 for(unsigned action=108;action<=109;action++){
  WObservation o=obs();WState s={.actor=1,.epoch=1};
  o.valor_style=o.valor=1;o.base.eligible=1;
  o.base.native_action=action;o.base.motion=0x583;
  o.base.held=ML_X;o.base.animation_frame=20;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP&&s.spirit_crimson_pending);
  o.base.held=0;o.base.animation_frame=45;o.base.sample_sequence++;
  assert(w_step(&s,&o).kind==W_KEEP);
  o.base.animation_frame=46;o.base.sample_sequence++;
  WDecision d=w_step(&s,&o);
  assert(d.kind==W_ENTER&&d.move==W_CHIJIN&&d.source==W_CRIMSON_STATIONARY);
 }
 const unsigned alternate_requests[]={137,139,141};
 for(unsigned i=0;i<sizeof(alternate_requests)/sizeof(*alternate_requests);i++){
  WObservation o=obs();WState s={0};o.valor_style=o.valor=1;
  o.base.native_action=alternate_requests[i];
  assert(w_valor_second_action(o.state,o.base.native_action));
  o.base.motion=0x5c2;o.base.animation_frame=55;
  assert(w_valor_chain_release(&s,&o,W_VALOR_RELEASE_THIRD).kind==W_KEEP);
  o.base.motion=0x5b9;o.base.animation_frame=60;
  assert(w_valor_chain_release(&s,&o,W_VALOR_RELEASE_THIRD).kind==W_KEEP);
 }
 const unsigned seconds[]={116,135,136,138,140};
 for(unsigned i=0;i<sizeof(seconds)/sizeof(*seconds);i++){
  WObservation o=obs();WState s={0};o.valor_style=o.valor=1;
  o.base.native_action=seconds[i];o.base.motion=0x5b9;o.base.animation_frame=99;
  assert(w_valor_chain_release(&s,&o,W_VALOR_RELEASE_SECOND).move==W_RELEASE&&s.drawn_release);
 }
 WObservation o=obs();WState s={0};o.valor_style=o.valor=1;o.state=1;
 assert(w_valor_chain_release(&s,&o,W_VALOR_RELEASE_SECOND).kind==W_KEEP);
 assert(w_valor_chain_release(&s,&o,W_VALOR_RELEASE_TAIL).move==W_RELEASE&&s.drawn_release);
}
static void short_spirit_costs(void)
{
 const unsigned sources[]={104,106,108},frames[]={56,54,110},costs[]={15,20,25};
 for(unsigned i=0;i<3;i++)for(unsigned paid=0;paid<=1;paid++){
  WObservation o=obs();WState s={0};
  o.rank=1;o.base.gauge=100;o.base.animation_frame=frames[i];o.base.held=ML_R;
  o.base.sample_sequence++;
  assert(wilds_bridge(&s,&o,sources[i]).kind==W_KEEP);
  o.base.sample_sequence++;o.base.held=0;o.base.gauge=costs[i]-1+paid;
  WDecision d=wilds_bridge(&s,&o,sources[i]);
  if(!paid)assert(d.kind==W_KEEP);
  else if(i==2)assert(d.source==917&&d.gauge_delta==-25);
  else assert(d.kind==W_NATIVE&&d.native_action==(i==0?23:26));
  assert(!s.wilds_r_pending);
 }
 const unsigned dividing_gauges[]={0,20,25,60};
 for(unsigned rank=2;rank<=3;rank++)for(unsigned i=0;i<sizeof(dividing_gauges)/sizeof(*dividing_gauges);i++){
  WObservation o=obs();WState s={0};
  o.rank=rank;o.base.gauge=dividing_gauges[i];
  WDecision d=w_dividing_round(&s,&o);
  int cost=o.base.gauge<25?(int)o.base.gauge:25;
  assert(d.kind==W_ENTER&&d.source==917&&d.gauge_delta==(rank==3?0:-cost));
 }
}
int main(void){
 valor_routes();
 WObservation adept=obs();
 WState adept_state={0};
 adept.adept_style=1;
 adept.rank=0;
 assert(w_adept_chain_release(&adept_state,&adept).kind==W_KEEP);
 adept.rank=3;
 assert(w_debug_enter(&adept_state,&adept,W_FORESIGHT).kind==W_KEEP);
 WDecision adept_release=w_adept_chain_release(&adept_state,&adept);
 assert(adept_release.kind==W_ENTER&&adept_release.move==W_RELEASE&&
        adept_release.rank_delta==-1&&adept_state.drawn_release);
 adept.base.held=ML_R;
 adept.base.motion=0x500+adept_state.slot;
 adept.base.native_action=w_action(W_RELEASE);
 assert(tick(&adept_state,&adept,ML_R,W_DRAWN_RELEASE_END).kind==W_IDLE);
 assert(w_wilds_round(&adept_state,&adept).kind==W_KEEP);
 assert(w_wilds_round_charge(&adept_state,&adept).kind==W_KEEP);
 assert(w_debug_enter(&adept_state,&adept,W_BIG).kind==W_ENTER);
 adept_state.counter_success=1;
 adept_state.iai_follow_keys=ML_R;
 adept_state.iai_follow_wait=0;
 adept.base.held=0;
 adept.base.animation_frame=37;
 WDecision adept_follow=wilds_iai_follow(&adept_state,&adept);
 assert(adept_follow.kind==W_NATIVE&&adept_follow.native_action==W_ADEPT_FOLLOW_ACTION);
 assert(w_adept_native_cancel(16,W_ADEPT_COUNTER_ACTION,0x500+185)==90);
 assert(w_adept_native_cancel(16,W_ADEPT_RECKONING_ACTION,0x500+186)==180);
 assert(w_adept_native_cancel(16,W_ADEPT_COUNTER_ACTION,0x500+186)==0);
 assert(w_adept_chain_release(&adept_state,&adept).move==W_RELEASE);
 assert(w_adept_recovery_follow(&adept_state,&adept,ML_R|ML_B,0).move==W_SHEATHE);
 w_leave(&adept_state);
 assert(w_debug_enter(&adept_state,&adept,W_THRUST).kind==W_ENTER);
 assert(w_adept_recovery_follow(&adept_state,&adept,ML_B,0).kind==W_ROLL);
 red_x_recovery();
 directional_spirit();
 draw_and_floor_routes();
 charge_routes();
 native_third_charge_recovery();
 charge_foresight_and_round_spin();
 red_foresight_focus_toggle();
 short_spirit_costs();
 iai_held_entry();
 iai_thrust_priority();
 valor_foresight_routes();
 valor_radiance_bound_slots();
 valor_guard_recovery_routes();
 WObservation o=obs();WState s={0};WDecision d;
 assert(w_wilds_enter(&s,&o,W_RELEASE).kind==W_KEEP);
 s.move=W_LAND;s.wilds_helm_red=0;assert(w_wilds_enter(&s,&o,W_RELEASE).kind==W_KEEP);
 s.wilds_helm_red=1;s.helm_cost_pending=1;s.rank=3;o.rank=3;
 d=w_wilds_enter(&s,&o,W_RELEASE);assert(d.kind==W_ENTER&&d.rank_delta==-2);
 s.move=W_LAND;s.wilds_helm_red=1;s.helm_cost_pending=0;o.rank=2;
 d=w_wilds_enter(&s,&o,W_RELEASE);assert(d.kind==W_ENTER&&d.rank_delta==-1);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_X,44).source==906);tick(&s,&o,0,1);
 assert(tick(&s,&o,ML_X,53).source==907);tick(&s,&o,0,1);
 assert(tick(&s,&o,ML_X,77).source==W_CRIMSON_STATIONARY);
 o=obs();w_reset(&s);o.state=1;o.drawn=0;o.base.gauge=0;
 o.rank=2;assert(w_wilds_enter(&s,&o,W_CHIJIN).kind==W_KEEP);
 o.rank=3;o.grounded=0;assert(w_wilds_enter(&s,&o,W_CHIJIN).kind==W_KEEP);
 o.grounded=1;o.base.native_action=6;assert(w_wilds_enter(&s,&o,W_CHIJIN).kind==W_KEEP);
 o.base.native_action=1;
 assert(w_wilds_enter(&s,&o,W_CHIJIN).source==W_CRIMSON_STATIONARY);
 for(unsigned direction=1;direction<=3;++direction){
  o=obs();w_reset(&s);o.base.magnitude=100;
  o.has_focus_heading=1;o.focus_camera_heading=1000;
  o.base.input_heading=(o.focus_camera_heading+16384*direction)&65535u;
  o.base.heading=o.base.input_heading; /* Already moving before X. */
  d=w_wilds_enter(&s,&o,W_CHIJIN);
  assert(d.source==(direction==2?W_CRIMSON_RETREAT:W_CRIMSON_MOVING));
  assert(d.heading==(direction==2?o.focus_camera_heading:o.base.input_heading));
  assert(d.crimson_direction==(direction==2?W_CRIMSON_DIRECTIONAL:W_CRIMSON_LATERAL));
  o.has_focus_heading=0;o.base.heading=1000;w_reset(&s);
  d=w_wilds_enter(&s,&o,W_CHIJIN);
  assert(d.source==W_CRIMSON_MOVING&&d.heading==o.base.input_heading);
 }
 const unsigned headings[]={0,0,16384,49152,32768};
 const unsigned sources[]={0,903,911,910,912};
 for(unsigned direction=0;direction<5;direction++){
  o=obs();w_reset(&s);o.has_focus_heading=1;o.base.gauge=0;w_wilds_enter(&s,&o,W_CHIJIN);
  o.base.magnitude=direction?100:0;o.base.input_heading=headings[direction];
  assert(tick(&s,&o,ML_R,38).kind==W_KEEP);
  o.base.magnitude=0;o.base.input_heading=0;o.base.simulation_delta=7;
  d=tick(&s,&o,0,50);
  assert(direction?(d.move==W_SLIDE&&d.source==sources[direction]&&!d.gauge_delta):
                   (d.kind==W_NATIVE&&d.native_action==16));
 }
 o=obs();w_reset(&s);o.rank=2;o.base.gauge=9;
 assert(w_wilds_enter(&s,&o,W_SLIDE).kind==W_KEEP);
 o.base.gauge=10;assert(w_wilds_enter(&s,&o,W_SLIDE).gauge_delta==-10);
 for(unsigned stage=2;stage<=4;stage++){
  o=obs();o.base.held=ML_R;w_reset(&s);wilds_charge(&s,&o,stage);
  s.wilds_elapsed=.4f;d=tick(&s,&o,0,30);
  assert(stage==2?(d.kind==W_NATIVE&&d.native_action==23):d.source==(stage==3?902:917));
 }
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_SLIDE);
 tick(&s,&o,ML_R,60);o.base.simulation_delta=7;assert(tick(&s,&o,ML_R,67).kind==W_KEEP);
 d=tick(&s,&o,0,68);
 assert(d.kind==W_NATIVE&&d.native_action==23);
 for(unsigned focus=0;focus<=1;++focus)for(unsigned direction=0;direction<4;++direction){
  static const unsigned inputs[]={0,16384,49152,32768};
  o=obs();w_reset(&s);o.rank=2;
  w_wilds_enter(&s,&o,W_SLIDE);
  o.base.magnitude=100;o.base.input_heading=inputs[direction];o.has_focus_heading=focus;
  assert(tick(&s,&o,ML_R,53).kind==W_KEEP);
  o.base.simulation_delta=7;o.base.magnitude=0;
  d=tick(&s,&o,0,59);
  assert(d.kind==W_MOTION&&d.source==W_SPIRIT_TWO_FORWARD+(focus?direction:0)&&d.gauge_delta==-15);
  assert(tick(&s,&o,ML_R,76).kind==W_KEEP);
  d=tick(&s,&o,0,81);
  assert(d.kind==W_NATIVE&&d.native_action==26);
 }
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_FOCUS);
 tick(&s,&o,ML_R,70);o.base.simulation_delta=7;d=tick(&s,&o,ML_R,77);
 assert(d.kind==W_NATIVE&&d.native_action==26);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_R|ML_X,50).move==W_THRUST);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_R|ML_A,50).move==W_FORESIGHT);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_R|ML_A,32).kind==W_KEEP);
 assert(tick(&s,&o,0,43).kind==W_KEEP);
 assert(tick(&s,&o,0,44).source==918);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_R|ML_B,50).move==W_SHEATHE);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 assert(tick(&s,&o,ML_B,50).kind==W_ROLL);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 d=tick(&s,&o,ML_ZR|ML_Y,50);assert(d.kind==W_KEEP);
 o=obs();w_reset(&s);o.base.held=ML_R;o.base.animation_frame=56;
 for(int i=0;i<40;i++){o.base.sample_sequence++;d=wilds_bridge(&s,&o,104);if(d.kind!=W_KEEP)break;}
 assert(d.move==W_CHARGE&&s.wilds_stage==2);
 o=obs();w_reset(&s);s.move=W_SMALL;o.base.held=ML_R;o.base.animation_frame=30;o.base.sample_sequence=1;
 assert(wilds_bridge(&s,&o,307).kind==W_KEEP);
 o.base.held=0;o.base.animation_frame=50;o.base.sample_sequence++;
 d=wilds_bridge(&s,&o,307);assert(d.kind==W_NATIVE&&d.native_action==23);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 d=tick(&s,&o,ML_X|ML_A,50);assert(d.kind==W_NATIVE&&d.native_action==8);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);o.rank=2;
 d=tick(&s,&o,ML_X,50);assert(d.kind==W_NATIVE&&d.native_action==0);
 o=obs();w_reset(&s);o.base.held=ML_R;o.base.animation_frame=95;o.base.sample_sequence=1;
 assert(wilds_bridge(&s,&o,108).kind==W_KEEP);
 o.base.held=0;o.base.animation_frame=110;o.base.sample_sequence++;
 d=wilds_bridge(&s,&o,108);assert(d.kind==W_ENTER&&d.source==917&&s.move==W_CHARGE);
 o=obs();o.base.held=ML_R;w_reset(&s);wilds_charge(&s,&o,2);
 assert(tick(&s,&o,ML_B,10).kind==W_ROLL);
 /* A direction is not a chord button: wait for B before committing R. */
 o=obs();w_reset(&s);o.base.magnitude=100;o.base.input_heading=16384;o.base.held=ML_R;o.base.sample_sequence=1;
 assert(wilds_bridge(&s,&o,1).kind==W_KEEP);
 o.base.held=ML_R|ML_B;o.base.sample_sequence++;
 assert(wilds_bridge(&s,&o,1).move==W_SHEATHE);
 o=obs();w_reset(&s);o.base.magnitude=100;o.base.input_heading=16384;o.base.held=ML_R;
 for(int i=0;i<8;i++){
  if(i==4)o.base.held=0;
  o.base.sample_sequence++;d=wilds_bridge(&s,&o,1);if(d.kind!=W_KEEP)break;
 }
 assert(d.move==W_SLIDE&&d.source==903&&d.heading==16384);
 o.base.held=ML_R;
 assert(w_wilds_enter(&s,&o,W_SLIDE).kind==W_ENTER);
 for(int i=0;i<40;i++)d=tick(&s,&o,ML_R,30);
 assert(tick(&s,&o,ML_R,56).kind==W_KEEP);
 o.base.magnitude=0;
 assert(tick(&s,&o,0,57).kind==W_KEEP);
 for(unsigned frame=58;frame<=97;frame++){
  d=tick(&s,&o,ML_R,frame);
  if(d.kind!=W_KEEP)break;
 }
 assert(d.move==W_CHARGE&&s.wilds_stage==2);
 for(unsigned follow=0;follow<2;follow++){
  o=obs();w_reset(&s);o.base.magnitude=100;o.base.input_heading=16384;
  if(follow)assert(w_wilds_enter(&s,&o,W_SLIDE).kind==W_ENTER);
  o.base.held=ML_R;
  for(unsigned i=0;i<16;i++){
   if(follow)d=tick(&s,&o,ML_R,59+i);
   else {o.base.sample_sequence++;d=wilds_bridge(&s,&o,1);}
   if(d.kind!=W_KEEP)break;
  }
  assert(d.kind==W_ENTER&&d.move==W_CHARGE&&s.wilds_stage==2);
 }
 o=obs();w_reset(&s);s.move=W_LAND;s.wilds_helm_red=1;s.rank=2;o.rank=2;
 o.base.magnitude=100;o.base.input_heading=16384;o.base.heading=1234;
 d=w_wilds_enter(&s,&o,W_RELEASE);assert(d.heading==1234);
 /* A roll tapped during Helm Breaker release seating waits for the sheath. */
 assert(tick(&s,&o,ML_B,126).kind==W_KEEP);
 for(unsigned frame=127;frame<W_RELEASE_SHEATHE_FRAME;++frame)
  assert(tick(&s,&o,0,frame).kind==W_KEEP);
 o.drawn=0;
 d=tick(&s,&o,0,W_RELEASE_SHEATHE_FRAME);
 assert(d.kind==W_ROLL&&d.sheathed_roll&&d.heading==16384);
 o=obs();w_reset(&s);w_wilds_round(&s,&o);wilds_phase(&s,&o,917);
 o.base.magnitude=100;
 assert(tick(&s,&o,0,wilds_clip(917)->frames-2).kind==W_KEEP);
 assert(tick(&s,&o,0,wilds_clip(917)->frames-1).kind==W_IDLE);
 /* A closed weapon during the authored recovery is not an interruption. */
 o=obs();w_reset(&s);w_wilds_round(&s,&o);wilds_phase(&s,&o,917);
 s.actor=1;s.epoch=1;o.drawn=0;o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);o.base.animation_frame=W_ROUND_SHEATHE_FRAME;
 d=w_step(&s,&o);assert(d.kind==W_KEEP&&s.source==917);
 o.base.animation_frame=wilds_clip(917)->frames-1;o.base.sample_sequence++;assert(w_step(&s,&o).kind==W_IDLE);
 for(unsigned rank=0;rank<4;rank++){
  o=obs();o.rank=rank;w_reset(&s);w_debug_enter(&s,&o,W_BIG);
  s.actor=1;s.epoch=1;o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);o.base.animation_frame=37;o.counter=1;
  d=w_step(&s,&o);assert(d.rank_delta==(rank<3)&&s.rank_paid&&!d.red_recovery);
  o.counter=0;o.contact=1;o.base.sample_sequence++;d=w_step(&s,&o);
  assert(d.rank_delta==0&&d.red_recovery==(rank==3?W_RED_IAI:0));
  o.base.sample_sequence++;assert(!w_step(&s,&o).red_recovery);
  o=obs();o.rank=rank;w_reset(&s);w_debug_enter(&s,&o,W_BIG);
  assert(w_leave(&s).rank_delta==0);
 }
 for(unsigned button=0;button<3;button++){
  o=obs();w_reset(&s);w_debug_enter(&s,&o,W_BIG);s.counter_success=1;s.consumed=0;
  o.base.animation_frame=40;o.base.held=button==0?ML_R|ML_B:button==1?ML_X:ML_R;
  o.base.sample_sequence++;
  d=world_command_step(&s,&o,s.source,w_event(s.source,40));
  if(button==2){
   assert(d.kind==W_KEEP);
   o.base.sample_sequence++;o.base.simulation_delta=7;o.base.held=0;
   d=world_command_step(&s,&o,s.source,w_event(s.source,47));
  }else if(button==1){
   assert(d.kind==W_KEEP);
   o.base.sample_sequence++;o.base.simulation_delta=3;o.base.held=0;o.base.animation_frame=43;
   d=world_command_step(&s,&o,s.source,w_event(s.source,43));
  }
  assert(button==0?d.move==W_SHEATHE&&d.counter_resheathe:button==1?d.source==W_CRIMSON_STATIONARY:d.kind==W_NATIVE&&d.native_action==26);
 }
 o=obs();w_reset(&s);w_debug_enter(&s,&o,W_BIG);
 assert(world_route(&s,&o,2).kind==W_KEEP); /* A missed counter cannot route through Spirit III. */
 o.base.animation_frame=40;o.base.held=ML_R;o.base.sample_sequence++;
 assert(world_command_step(&s,&o,s.source,w_event(s.source,40)).kind==W_KEEP);
  for(unsigned i=0;i<2;i++){
  o.base.animation_frame+=5;o.base.simulation_delta=5;o.base.sample_sequence++;
  d=world_command_step(&s,&o,s.source,w_event(s.source,o.base.animation_frame));
   if(!i)assert(d.kind==W_KEEP);
 }
 assert(d.kind==W_ENTER&&d.move==W_CHARGE&&s.wilds_stage==2);
 assert(tick(&s,&o,ML_R|ML_B,2).kind==W_KEEP);
 assert(tick(&s,&o,0,19).kind==W_KEEP);
 assert(tick(&s,&o,0,20).move==W_SHEATHE);
 for(unsigned i=0;i<5;i++){
  o=obs();o.base.held=ML_R;o.valor=i==4;w_reset(&s);wilds_charge(&s,&o,2);
  if(i==2||i==3)wilds_phase(&s,&o,i==2?915:909);
  d=tick(&s,&o,ML_R|ML_B,i==0?49:i==1?50:0);
  if(i==0||i==2||i==3)assert(d.move==W_SHEATHE);
  else if(i==4)assert(d.kind==W_ROLL);
  else assert(d.kind==W_KEEP);
 }
 o=obs();w_reset(&s);s.actor=1;s.epoch=1;
 d=w_debug_enter(&s,&o,W_SHEATHE);s.consumed=0;
 o.base.motion=0x500+d.slot;o.base.native_action=w_action(W_SHEATHE);
 o.base.held=ML_B;o.base.animation_frame=20;
 assert(world_command_step(&s,&o,306,w_event(306,20)).kind==W_KEEP);
 o.base.animation_frame=90;o.base.sample_sequence++;
 d=world_command_step(&s,&o,306,w_event(306,90));
 assert(d.kind==W_ROLL&&d.sheathed_roll);
 for(unsigned large=0;large<2;large++){
  o=obs();w_reset(&s);s.actor=1;s.epoch=1;
  d=w_debug_enter(&s,&o,W_SHEATHE);
  s.controls.initialized=1;s.controls.actor=1;s.controls.epoch=1;
  o.base.motion=0x500+d.slot;o.base.native_action=w_action(W_SHEATHE);
  o.base.held=large?ML_R:ML_X;
  assert(!w_iai_ready(&s,&o));
  assert(w_iai_press(&s,&o).kind==W_KEEP); /* Sheath startup is vulnerable. */
  o.base.animation_frame=90;
  assert(w_iai_ready(&s,&o));
  s.controls.buttons[large?2:0].raw=1;
  assert(w_iai_press(&s,&o).kind==W_KEEP); /* A held key is not a fresh Iai. */
  s.controls.buttons[large?2:0].raw=0;
  d=world_command_step(&s,&o,306,w_event(306,90));
  assert(d.kind==W_ENTER&&d.move==(large?W_BIG:W_SMALL));
  s.iai_focus_locked=1;
  o.base.motion=0x500+d.slot;o.base.animation_frame=15;o.base.magnitude=100;
  o.base.input_heading=16384;o.base.sample_sequence++;
  unsigned heading=0;
  assert(!w_rotation(&s,&o,&heading));
 }
 o=obs();w_reset(&s);d=w_debug_enter(&s,&o,W_FORESIGHT);assert(d.source==918&&!d.gauge_clear&&s.foresight_gauge);
 s.actor=1;s.epoch=1;o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);
 o.contact=o.counter=1;assert(!w_step(&s,&o).gauge_fill);
 o.rank=2;w_reset(&s);d=w_debug_enter(&s,&o,W_FORESIGHT);assert(d.source==140&&d.gauge_clear);
 o=obs();o.rank=2;o.base.gauge=0;w_reset(&s);w_debug_enter(&s,&o,W_BIG);
 s.actor=1;s.epoch=1;o.counter=1;o.base.held=ML_X;o.base.animation_frame=37;o.base.sample_sequence++;
 o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);
 d=w_step(&s,&o);assert(d.kind==W_KEEP&&d.rank_delta==1&&d.gauge_fill);
 o.counter=0;o.rank=3;o.base.gauge=100;o.base.held=0;o.base.animation_frame=40;
 o.base.sample_sequence++;o.base.simulation_delta=3;
 d=w_step(&s,&o);assert(d.source==W_CRIMSON_STATIONARY&&!d.rank_delta&&s.rank==3);
 o=obs();o.rank=1;o.base.gauge=0;w_reset(&s);w_debug_enter(&s,&o,W_BIG);
 s.counter_success=1;o.base.held=ML_R;o.base.animation_frame=40;
 o.base.sample_sequence++;
 assert(world_command_step(&s,&o,s.source,w_event(s.source,40)).kind==W_KEEP);
 o.base.sample_sequence++;o.base.simulation_delta=7;o.base.held=0;
 d=world_command_step(&s,&o,s.source,w_event(s.source,47));assert(d.kind==W_NATIVE&&d.native_action==27);
 for(unsigned leading=ML_R;leading<=ML_B;leading+=ML_B-ML_R){
  o=obs();w_reset(&s);w_debug_enter(&s,&o,W_BIG);s.counter_success=1;
  o.base.held=leading;o.base.sample_sequence++;o.base.animation_frame=39;
  assert(world_command_step(&s,&o,s.source,w_event(s.source,39)).kind==W_KEEP);
  o.base.held=ML_R|ML_B;o.base.sample_sequence++;o.base.animation_frame=42;o.base.simulation_delta=3;
  assert(world_command_step(&s,&o,s.source,w_event(s.source,42)).move==W_SHEATHE);
 }
 o=obs();w_reset(&s);w_debug_enter(&s,&o,W_SMALL);
 o.base.held=ML_X;o.base.sample_sequence++;o.base.animation_frame=33;
 assert(world_command_step(&s,&o,307,w_event(307,33)).kind==W_KEEP);
 o.base.held=0;o.base.sample_sequence++;o.base.animation_frame=42;o.base.simulation_delta=9;
 assert(world_command_step(&s,&o,307,w_event(307,42)).source==W_CRIMSON_STATIONARY);
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_CHIJIN);
 o.focus_strike=1;o.base.held=ML_R|ML_Y;o.base.animation_frame=38;o.base.sample_sequence++;
 assert(world_command_step(&s,&o,W_CRIMSON_STATIONARY,w_event(W_CRIMSON_STATIONARY,38)).kind==W_KEEP);
 o.base.animation_frame=50;o.base.sample_sequence++;
 assert(world_command_step(&s,&o,W_CRIMSON_STATIONARY,w_event(W_CRIMSON_STATIONARY,50)).move==W_FOCUS);
 /* An ordinary hit with the starter does not unlock the wound combo. */
 o=obs();o.rank=2;w_reset(&s);s.actor=1;s.epoch=1;
 d=w_wilds_enter(&s,&o,W_FOCUS);
 o.base.native_action=w_action(W_FOCUS);o.base.motion=0x500+d.slot;
 o.base.animation_frame=50;o.base.sample_sequence++;o.contact=1;
 d=w_step(&s,&o);
 assert(d.kind==W_KEEP&&!d.rank_delta&&s.hit_success);
 s.wilds_focus_break=1;o.base.sample_sequence++;
 d=w_step(&s,&o);
 assert(d.kind==W_MOTION&&d.source==913&&!d.rank_delta);
 o.base.motion=0x500+d.slot;o.base.animation_frame=83;o.base.sample_sequence++;
 assert(w_step(&s,&o).kind==W_KEEP);
 o.base.animation_frame=84;o.base.sample_sequence++;
 d=w_step(&s,&o);
 assert(d.kind==W_MOTION&&d.source==914&&!d.rank_delta);
 o.base.motion=0x500+d.slot;o.base.animation_frame=wilds_clip(914)->recovery-1;
 o.base.sample_sequence++;d=w_step(&s,&o);
 assert(d.kind==W_KEEP&&!d.rank_delta);
 o.base.animation_frame++;o.base.sample_sequence++;
 d=w_step(&s,&o);assert(d.kind==W_IDLE&&d.rank_delta==1);
 o.base.sample_sequence++;assert(!w_step(&s,&o).rank_delta);

 puts("PASS: chains, holds, chords, sheath recovery, Iai, red draw, five Spirit I branches, red Foresight");
}


