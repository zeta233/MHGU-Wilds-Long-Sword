/* Host tests execute the shipping controller, including its private helpers. */
#include <assert.h>
#include <stdio.h>
#define ML_WILDS_MOVES 1
#define ML_WORLD_INPUT_VM 1
#include "world_core.c"
static WObservation obs(void){return (WObservation){.base={.actor=1,.epoch=1,.gauge=100,.heading=0,.simulation_delta=1},.state=16,.alive=1,.drawn=1,.grounded=1,.resources_ready=1,.rank=3};}
static WDecision tick(WState *s,WObservation *o,unsigned held,float frame){o->base.held=held;o->base.animation_frame=frame;o->base.sample_sequence++;return wilds_step(s,o);}
int main(void){
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
  assert(stage==2?(d.kind==W_NATIVE&&d.native_action==23):d.source==(stage==3?902:916));
 }
 o=obs();w_reset(&s);w_wilds_enter(&s,&o,W_SLIDE);
 tick(&s,&o,ML_R,60);o.base.simulation_delta=7;d=tick(&s,&o,ML_R,67);
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
 d=wilds_bridge(&s,&o,108);assert(d.kind==W_ENTER&&d.source==916&&s.move==W_CHARGE);
 o=obs();o.base.held=ML_R;w_reset(&s);wilds_charge(&s,&o,2);
 assert(tick(&s,&o,ML_R|ML_B,10).kind==W_ROLL);
 /* A direction is not a chord button: wait for B before committing R. */
 o=obs();w_reset(&s);o.base.magnitude=100;o.base.input_heading=16384;o.base.held=ML_R;o.base.sample_sequence=1;
 assert(wilds_bridge(&s,&o,1).kind==W_KEEP);
 o.base.held=ML_R|ML_B;o.base.sample_sequence++;
 assert(wilds_bridge(&s,&o,1).move==W_SHEATHE);
 o=obs();w_reset(&s);o.base.magnitude=100;o.base.input_heading=16384;o.base.held=ML_R;
 for(int i=0;i<8;i++){o.base.sample_sequence++;d=wilds_bridge(&s,&o,1);if(d.kind!=W_KEEP)break;}
 assert(d.move==W_SLIDE&&d.source==903&&d.heading==16384);
 for(int i=0;i<40;i++)d=tick(&s,&o,ML_R,30);
 d=tick(&s,&o,ML_R,53);assert(d.move==W_CHARGE&&s.wilds_stage==2);
 o=obs();w_reset(&s);s.move=W_LAND;s.wilds_helm_red=1;s.rank=2;o.rank=2;
 o.base.magnitude=100;o.base.input_heading=16384;o.base.heading=1234;
 d=w_wilds_enter(&s,&o,W_RELEASE);assert(d.heading==1234);
 o=obs();w_reset(&s);wilds_round(&s,&o);wilds_phase(&s,&o,917);
 o.base.magnitude=100;
 assert(tick(&s,&o,0,110).kind==W_KEEP);
 assert(tick(&s,&o,0,175).kind==W_IDLE);
 /* A closed weapon during the authored recovery is not an interruption. */
 o=obs();w_reset(&s);wilds_round(&s,&o);wilds_phase(&s,&o,917);
 s.actor=1;s.epoch=1;o.drawn=0;o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);o.base.animation_frame=151;
 d=w_step(&s,&o);assert(d.kind==W_KEEP&&s.source==917);
 o.base.animation_frame=175;o.base.sample_sequence++;assert(w_step(&s,&o).kind==W_IDLE);
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
   o.base.sample_sequence++;o.base.simulation_delta=7;
   d=world_command_step(&s,&o,s.source,w_event(s.source,47));
  }
  assert(button==0?d.move==W_SHEATHE:button==1?d.source==W_CRIMSON_STATIONARY:d.kind==W_NATIVE&&d.native_action==26);
 }
 o=obs();w_reset(&s);w_debug_enter(&s,&o,W_BIG);
 assert(world_route(&s,&o,2).kind==W_KEEP); /* A missed counter cannot route through Spirit III. */
 o.base.animation_frame=40;o.base.held=ML_R;o.base.sample_sequence++;
 assert(world_command_step(&s,&o,s.source,w_event(s.source,40)).kind==W_KEEP);
 for(unsigned i=0;i<3;i++){
  o.base.animation_frame+=16;o.base.simulation_delta=16;o.base.sample_sequence++;
  d=world_command_step(&s,&o,s.source,w_event(s.source,o.base.animation_frame));
  if(i<2)assert(d.kind==W_KEEP);
 }
 assert(d.kind==W_ENTER&&d.move==W_CHARGE&&s.wilds_stage==2);
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
 s.actor=1;s.epoch=1;o.counter=1;o.base.held=ML_X;o.base.animation_frame=37;
 o.base.motion=0x500+s.slot;o.base.native_action=w_action(s.move);
 d=w_step(&s,&o);assert(d.source==W_CRIMSON_STATIONARY&&d.rank_delta==1&&d.gauge_fill&&s.rank==3);
 o=obs();o.rank=1;o.base.gauge=0;w_reset(&s);w_debug_enter(&s,&o,W_BIG);
 s.counter_success=1;o.base.held=ML_R;o.base.animation_frame=40;
 o.base.sample_sequence++;
 assert(world_command_step(&s,&o,s.source,w_event(s.source,40)).kind==W_KEEP);
 o.base.sample_sequence++;o.base.simulation_delta=7;
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
 assert(d.kind==W_MOTION&&d.source==W_FOCUS_FLURRY&&d.rank_delta==1);
 o.base.motion=0x500+d.slot;o.base.animation_frame=124;o.base.sample_sequence++;
 assert(w_step(&s,&o).kind==W_KEEP);
 o.base.animation_frame=125;o.base.sample_sequence++;
 d=w_step(&s,&o);
 assert(d.kind==W_MOTION&&d.source==913&&!d.rank_delta);
 o.base.motion=0x500+d.slot;o.base.animation_frame=83;o.base.sample_sequence++;
 assert(w_step(&s,&o).kind==W_KEEP);
 o.base.animation_frame=84;o.base.sample_sequence++;
 d=w_step(&s,&o);
 assert(d.kind==W_MOTION&&d.source==914&&!d.rank_delta);
 puts("PASS: chains, holds, chords, sheath recovery, Iai, red draw, five Spirit I branches, red Foresight");
}


