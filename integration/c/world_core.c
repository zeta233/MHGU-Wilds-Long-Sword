#include "world_core.h"
#if ML_ASCENDANCE_PROTOTYPE
#include "enhanced_sheathe_data.h"
#endif
#if ML_WILDS_MOVES
#include "wilds_combat_data.h"
#endif
#include "world_floats.h"
#include "world_roots.h"
#include "world_motion_vm.h"
#include "world_ground_vm.h"

int w_root_heading(unsigned source,float frame)
{
    if(!(frame>=0))return 0;
    for(unsigned i=0;i<sizeof(world_root_tracks)/sizeof(*world_root_tracks);++i){
        const WRootTrack *track=world_root_tracks+i;if(track->source!=source)continue;
        unsigned n=(unsigned)frame;if(n>=track->count)n=track->count-1;
        unsigned next=n+1<track->count?n+1:n;
        float fraction=frame-(float)n;if(fraction>1)fraction=1;
        float value=world_root_angles[track->offset+n];
        value+=(world_root_angles[track->offset+next]-value)*fraction;
        return (int)value;
    }
    return 0;
}

/* World11edc90: raw keys enter their chord wait before command permissions
 * are tested.1230f64 uses player7810+468 (0.10 s); X/A use global+110
 * (0.07 s), read from the running reference. A buffered tap keeps its timer
 * after release. Waiting is never restarted by a native action request. */
void w_keys_tick(WKeys *k,const MLObservation *o)
{
    if(k->actor!=o->actor||k->epoch!=o->epoch)
        *k=(WKeys){.actor=o->actor,.epoch=o->epoch,.held=o->held};
    if(k->sample==o->sample_sequence)return;
    k->sample=o->sample_sequence;
    float dt=o->simulation_delta/60.0f;
    if(!(dt>0&&dt<.5f))dt=0;
    for(unsigned i=0;i<3;++i){
        unsigned bit=1u<<i;
        if((o->held&bit)&&!(k->held&bit)&&!(k->waiting&bit)){
            k->waiting|=bit;k->ready&=~bit;k->remaining[i]=i==2?.10f:.07f;
        }
        if(k->waiting&bit){
            if(k->remaining[i]<=0){k->waiting&=~bit;k->ready|=bit;}
            k->remaining[i]-=dt;
        }
    }
    k->held=o->held;
}

float w_float(unsigned source,unsigned channel,float frame)
{
    for(unsigned i=0;i<sizeof(world_float_tracks)/sizeof(*world_float_tracks);++i){
        const WFloatTrack *track=world_float_tracks+i;
        if(track->source!=source||track->channel!=channel)continue;
        const WFloatKey *keys=world_float_keys+track->offset;unsigned n=0;
        while(n+1<track->count&&keys[n+1].frame<=frame)++n;
        float value=keys[n].value;
        if(n+1<track->count&&keys[n].interpolation==2&&frame>keys[n].frame)
            value+=(keys[n+1].value-value)*(frame-keys[n].frame)/(keys[n+1].frame-keys[n].frame);
        return value;
    }
    return channel==W_FLOAT_SPEED?1:0;
}

const WClip *w_clip(unsigned source)
{
    for(unsigned i=0;i<sizeof(world_clips)/sizeof(*world_clips);++i)
        if(world_clips[i].source==source)return world_clips+i;
    return 0;
}
const WEvent *w_event(unsigned source,float frame)
{
    if((source>=901&&source<=918)||(source>=W_CRIMSON_STATIONARY&&source<=W_CRIMSON_SPIN)){
        static const WEvent wilds_event={.speed=1,.hit=-1};
        return &wilds_event;
    }
    /* World landing TIMLs 149/154. On 149 the sheath remains available
     * after F130, when the Foresight window has already closed. */
    if(source==149||source==154){
        static const WEvent landing[]={
            {.input={1u<<14,0,(1u<<22)|(1u<<8)}}, {.input={3u<<14,0,(3u<<22)|(3u<<8)}}, {.input={3u<<14}}
        };
        unsigned spirit=source==154;
        if(!(frame>=(spirit?5:60)&&frame<(spirit?60:151)))return 0;
        if(!spirit&&frame>=130)return landing+2;
        return landing+(frame>=(spirit?10:70));
    }
    const WClip *c=w_clip(source);if(!c||!(frame>=0))return 0;
    const WEvent *e=world_events+c->offset;unsigned i=1;
    while(i<c->count&&e[i].frame<=frame)++i;
    return e+i-1;
}

float w_next_hit_frame(unsigned source,float frame)
{
    const WClip *clip=w_clip(source);
    const WEvent *active=w_event(source,frame);
    if(!clip||!active||active->hit>=0)return -1;
    const WEvent *events=world_events+clip->offset;
    for(unsigned i=0;i<clip->count;++i)
        if(events[i].frame>frame&&events[i].hit>=0)return events[i].frame;
    return 0;
}
unsigned w_source(unsigned motion)
{
    if((motion>>8)!=5)return 0;
    unsigned n=motion-0x500;
#if ML_ASCENDANCE_PROTOTYPE
    if(n==ENHANCED_SHEATHE_SLOT)return 306;
#endif
    for(unsigned i=0;i<sizeof(world_clips)/sizeof(*world_clips);++i)
        if(world_clips[i].slot&&world_clips[i].slot==n)return world_clips[i].source;
    if(n>=101&&n<=108)return n;
    if(n==127)return 109;
    if(n==128)return 127;
    if(n==129)return 128;
    if(n==130||n==131)return 110;
    return 0;
}
/* Stock Guild weapon commands. Hunter Arts and airborne actions have their
 * own dispatch and can share a pose with a ground attack during transitions. */
unsigned w_guild_attack(unsigned action)
{
    return action<=14 || (action>=16 && action<=30) || (action>=48 && action<=50);
}
unsigned w_guild_spirit_opener(unsigned action,unsigned motion,unsigned previous)
{
    /* Native upper 25 after ordinary thrust is the same pose as the
     * insertion after Spirit II. Only the former restarts at Spirit I. */
    return (action==4&&motion==0x569) ||
        (motion==0x56b&&(action==7||action==41||(action==25&&(previous==4||previous==22))));
}
unsigned w_guild_source(unsigned state,unsigned action,unsigned motion)
{
    if((state==1||state==2)&&action<=5)return 1;
    if(state!=16)return 0;
    if(action>=72&&action<=75&&(motion==0x59a||motion==0x59c||motion==0x568))return 154;
    if(((action>=32&&action<=34)||(action>=63&&action<=65)||(action>=69&&action<=71))&&motion==0x565)return 149;
    if(((action>=42&&action<=47)||(action>=76&&action<=81))&&motion==0x568)return 154;
    if(action==41&&motion==0x56b)return 107;
    if(!w_guild_attack(action))return 0;
    unsigned slot=motion-0x500;
    if(action>=48){
        if((action==48||action==49)&&(slot==130||slot==131))return 110;
        return action==50&&slot==108?108:0;
    }
    if(action==22&&(slot==105||slot==112))return 112;
    if(slot>=101&&slot<=108)return slot;
    
    if(slot>=127&&slot<=131)return w_source(motion);
    return 0;
}
/* Native Valor strike and recovery clips. Several share their action ID with
 * the sheathe or guard startup, which must run to completion. */
unsigned w_valor_draw_cancel(unsigned action,unsigned motion)
{
    static const unsigned char cuts[][3]={
        {86,190,32},{86,184,22},{87,130,38},{99,103,38},{100,129,50},{101,128,50},
        /* Native fade-slash variants share their striking clips. Wait for
         * the strike itself; several actions first bind a movement clip. */
        {93,103,38},{94,103,38},{95,129,50},{96,129,50},{97,128,50},{98,128,50},
        {110,103,38},{111,129,50},{112,128,50},{113,103,38},{114,129,50},{115,128,50},
        {120,103,38},{121,103,38},{122,103,38},{123,129,50},{124,129,50},{125,129,50},
        {126,128,50},{127,128,50},{128,128,50}
    };
    for(unsigned i=0;i<sizeof(cuts)/sizeof(cuts[0]);++i)
        if(action==cuts[i][0]&&motion==0x500u+cuts[i][1])return cuts[i][2];
    return 0;
}
unsigned w_valor_chord(unsigned state,unsigned action,unsigned motion,float frame,unsigned held,unsigned focus)
{
    unsigned move=held==ML_X?W_CHIJIN:held==(ML_R|ML_X)?W_THRUST:
                  held==(ML_R|ML_A)?W_FORESIGHT:focus&&held==(ML_R|ML_Y)?W_FOCUS:0;
    if(!move)return 0;
    if((state==1||state==2)&&action<=5)return move==W_FORESIGHT?0:move;
    if(state!=16)return 0;
    /* Keep the immediate native GP on a lone R. Accept the partner button
     * during the first few frames of the native opener in either key order. */
    if(move==W_THRUST&&frame<=8&&
       ((action==130&&motion==0x5c0)||((action==16||action==17)&&motion==0x568)||
        (action<=3&&(motion==0x565||motion==0x566))))return move;
    unsigned draw_cancel=w_valor_draw_cancel(action,motion);
    if(draw_cancel)return frame>=draw_cancel?move:0;
    static const unsigned char recovery[][3]={
        {104,106,38},{23,106,44},{24,106,44},{105,108,95},{119,107,38},
        {102,130,46},{103,131,46},{108,131,46},{109,131,46},{22,105,38},
        {118,105,38},{143,103,76},{144,103,76},{130,192,44},{130,104,62},
        {116,191,34},{116,185,22},{117,186,110},{134,190,32},{134,184,22},
        {135,194,50},{135,185,22},{136,191,34},{136,185,22},{137,194,50},{137,185,22},
        {138,191,34},{138,185,22},{139,194,50},{139,185,22},{140,191,34},{140,185,22},
        {141,194,50},{141,185,22}
    };
    for(unsigned i=0;i<sizeof(recovery)/sizeof(recovery[0]);++i){
        const unsigned char *row=recovery[i];
        if(row[0]==action&&0x500u+row[1]==motion)return frame>=row[2]?move:0;
    }
    unsigned source=w_guild_source(state,action,motion);
    const WEvent *e=w_event(source,frame);
    if(!source||!e)return 0;
    if(move==W_FOCUS||move==W_CHIJIN)return frame>=24?move:0;
    WInput input=move==W_SHEATHE?WI_SHEATHE:move==W_THRUST?WI_THRUST:WI_FORESIGHT;
    return ((e->input[input.word]>>input.shift)&2)?move:0;
}
unsigned w_native_roll(unsigned state,unsigned action)
{
    return state==1&&(action==6||action==13||action==14);
}
unsigned w_foresight_stage(unsigned source,const WEvent *e)
{
    if(!e||source==306||source==109||source==170)return 0;
    return (e->input[2]>>22)&3;
}
unsigned w_action(unsigned move){return move?239u+move:0;}
unsigned w_private(unsigned state,unsigned action){return state==16&&action>=w_action(W_FORESIGHT)&&action<=w_action(W_FOCUS);}
void w_reset(WState *s){*s=(WState){0};}
static unsigned source_for(unsigned move,unsigned rank)
{
    /* Dedicated Wilds sources: never replace shared World animations. */
    static const unsigned ids[]={0,140,170,171,172,173,174,306,307,308,312,312,
                                 W_CRIMSON_MOVING,908,903,904,905};
    return move==W_BIG?308+(rank>3?0:rank):ids[move];
}
static unsigned chord(unsigned held)
{
    if(!(held&ML_R))return 0;
    if(held&ML_B)return W_SHEATHE;
    if(held&ML_A)return W_FORESIGHT;
    if(held&ML_X)return W_THRUST;
    return 0;
}
static unsigned input_stage(const WEvent *e,WInput input)
{
    return e?(e->input[input.word]>>input.shift)&3:0;
}
static WInput move_input(unsigned move)
{
    return move==W_SHEATHE?WI_SHEATHE:move==W_FORESIGHT?WI_FORESIGHT:WI_THRUST;
}
static unsigned input_window_ahead(unsigned source,float frame,WInput input)
{
    const WClip *c=w_clip(source);if(!c)return 0;
    for(unsigned i=0;i<c->count;++i){
        const WEvent *e=world_events+c->offset+i;
        if(e->frame>=frame&&input_stage(e,input))return 1;
    }
    return 0;
}
static unsigned offer(WState *s,const WEvent *e,unsigned source,unsigned choice,
                      unsigned mask,WInput input,unsigned heading)
{
    if(!input_stage(e,input)&&
       (s->move||!e||!input_window_ahead(source,e->frame,input)))return 0;
    if(s->queued!=choice)s->solo_age=0;
    s->queued=choice;s->queued_mask=mask;s->queued_source=source;
    s->queued_word=input.word;s->queued_shift=input.shift;s->queued_heading=heading;
    return 1;
}
/* Raw gesture collection precedes the motion permission consumer in World
 *1232101/1232194. Keep an early R tap through the upcoming TIML window;
 * closed/end intervals and action changes still discard it. */
static unsigned r_window_ahead(unsigned source,float frame)
{
    return input_window_ahead(source,frame,WI_R);
}
/* Source118e405 consumes TIML trigger11.118b790 applies the selected W03
 * threshold/limit;1f755c0 mode2 resolves to2174bc0: t*(2-t), not linear.
 * Keep this separate from command entry: Iai triggers at10/12, FS at45. */
unsigned w_rotation(WState *s,const WObservation *o,unsigned *heading)
{
    const MLObservation *b=&o->base;
    if(!s->move||b->motion!=0x500+s->slot||s->turn_sample==b->sample_sequence)return 0;
    s->turn_sample=b->sample_sequence;
    if(s->iai_focus_locked&&(s->move==W_SMALL||s->move==W_BIG))return 0;
    float dt=b->simulation_delta/60.0f;
    if(!(dt>0&&dt<.5f))return 0;
    if(s->move==W_CHARGE&&s->source==915&&b->magnitude>=50){
        /* Wilds cKijinChargeWalk turns the authored forward root at 100 degrees/s. */
        int delta=ml_heading_delta(b->input_heading,b->heading),cap=(int)((65536.f*100.f/360.f)*dt);
        if(delta>cap)delta=cap;else if(delta< -cap)delta= -cap;
        s->heading=*heading=(b->heading+delta)&65535u;return 1;
    }
    unsigned minimum=0,maximum=0,clamp=0;float duration=0;
    if(s->move==W_THRUST){minimum=5461;maximum=8192;duration=.2f;}
    else if(s->move==W_FORESIGHT){minimum=8192;maximum=10923;duration=.08f;}
    else if(s->move==W_DOWN){minimum=8192;maximum=65536;duration=.2f;}
    else if(s->move==W_SMALL||s->move==W_BIG){minimum=3641;maximum=10923;clamp=1;duration=.02f;}
    const WEvent *e=w_event(s->source,b->animation_frame);
    if(duration&&!s->turn_seen&&e&&(e->trigger&2048u)){
        s->turn_seen=1;
        int delta=ml_heading_delta(b->input_heading,b->heading);
        unsigned magnitude=(unsigned)(delta<0?-delta:delta);
        if(b->magnitude>=50&&magnitude>=minimum){
            if(maximum<65536){
                if(!clamp||magnitude>maximum)delta=delta<0?-(int)maximum:(int)maximum;
            }
            s->turn_origin=b->heading;s->turn_delta=delta;
            /* t*(2-t) starts at twice its mean speed. Give large
             * triggered corrections enough time to expose intermediate poses. */
            float smooth=(float)(delta<0?-delta:delta)/65536.f;
            s->turn_duration=duration>smooth?duration:smooth;s->turn_age=0;
        }
    }
    if(!(s->turn_duration>0)){
        /* Common update1f6be8f reads TIML+7c only with stick input and no
         * triggered rotation in progress.1f780b0 ->1c03630 converts the
         * degrees/second * simulation seconds cap to radians. */
        float turn=w_float(s->source,W_FLOAT_TURN,b->animation_frame);
        if(!e||turn<=0||b->magnitude<50){s->turn_fraction=0;return 0;}
        unsigned target=b->input_heading;
        if(e->common&128u)target=(target+32768u)&65535u;
        int delta=ml_heading_delta(target,b->heading);
        float cap=turn*dt*(65536.0f/360.0f)+s->turn_fraction;
        int amount=(int)cap;s->turn_fraction=cap-(float)amount;
        if(delta>amount)delta=amount;
        else if(delta< -amount)delta= -amount;
        else s->turn_fraction=0;
        *heading=(b->heading+delta)&65535u;s->heading=*heading;return delta!=0;
    }
    s->turn_age+=dt;float t=s->turn_age/s->turn_duration;if(t>1)t=1;
    *heading=(s->turn_origin+(int)((float)s->turn_delta*t*(2-t)))&65535u;
    s->heading=*heading;if(t>=1)s->turn_duration=0;
    return 1;
}
static unsigned spirit_available(const WObservation *o,unsigned cost)
{
#if ML_WILDS_MOVES
    if(o->rank==3)return 1;
#endif
    return o->base.gauge>=cost;
}
static void spirit_round_reward(WDecision *d,unsigned rank)
{
#if ML_WILDS_MOVES
    if(rank==3){d->red_recovery|=W_RED_ROUND;return;}
    if(rank==2)d->gauge_fill=1;
    else
#else
    (void)rank;
#endif
        d->gauge_clear=1;
    d->rank_delta++;
}
WDecision w_leave(WState *s)
{
    WDecision d={0};
    s->foresight_press.motion=0;
    if(s->move==W_GROUND&&s->ground_node==5&&s->hit_success&&!s->rank_paid){
        spirit_round_reward(&d,s->rank);
    }
    /* Native cIaigiriH::exit settles a missed counter even when interrupted
     * before its frame-37 trigger. A successful counter does not spend a tier. */
#if !ML_WILDS_MOVES
    if(s->move==W_BIG&&!s->rank_paid&&!s->counter_success&&s->rank)d.rank_delta=-1;
#endif
    if(s->helm_cost_pending&&s->rank)d.rank_delta-=1;
    s->helm_cost_pending=0;s->red_foresight_chain=0;s->wilds_charge_link=0;
    s->wilds_three_exit_r=s->wilds_three_exit_sample=0;
    s->wilds_slide_exit_sample=s->wilds_slide_exit_r=s->wilds_slide_weak_r=0;
    s->spirit_crimson_pending=s->spirit_crimson_repeat=0;
    s->wilds_charge_carry=0;s->wilds_charge_carry_source=s->wilds_charge_carry_action=0;
    s->move=s->bound=s->queued=0;s->source=s->slot=0;s->ground_node=s->main_source=0;return d;
}
/* Free movement turns into the stick. Focus holds a camera-relative
 * facing, which makes a lateral step or a backward-facing cut meaningful. */
static unsigned directional_facing(const WObservation *o,unsigned input)
{
    return o->has_focus_heading?o->focus_camera_heading:input;
}
#if ML_WILDS_MOVES
static unsigned crimson_one(const WObservation *o,unsigned *heading,unsigned *direction)
{
    const MLObservation *b=&o->base;
    *heading=b->heading;
    *direction=0;
    if(b->magnitude<50)return W_CRIMSON_STATIONARY;
    unsigned reference=directional_facing(o,b->input_heading);
    int angle=ml_heading_delta(b->input_heading,reference);
    *heading=b->input_heading;
    *direction=W_CRIMSON_DIRECTIONAL;
    if(angle>24576||angle< -24576){
        /* MOT287 travels along local -Z while the hunter cuts toward +Z. */
        *heading=(b->input_heading+32768u)&65535u;
        return W_CRIMSON_RETREAT;
    }
    if(angle>8192||angle< -8192)*direction=W_CRIMSON_LATERAL;
    return W_CRIMSON_MOVING;
}
#endif

#if ML_WILDS_MOVES
static void big_counter_rank_reward(const WObservation *o,WDecision *d)
{
    if(o->valor_style){
        if(!o->valor)d->valor_gain=W_VALOR_COUNTER_GAIN;
    }else if(o->rank<3){
        d->rank_delta+=1;
        if(o->rank==2)d->gauge_fill=1;
    }
}
#endif

static WDecision enter(WState *s,const WObservation *o,unsigned move,unsigned mask)
{
    if(o->valor&&(move==W_SHEATHE||move==W_SMALL||move==W_BIG))return (WDecision){0};
    /* Source down::exit bypasses settlement only for land (action0x5c).
     * Land starts weapon23a8=0.5; leaving that sequence settles immediately. */
    unsigned transfer=move==W_LAND&&((s->move==W_DOWN&&s->helm_cost_pending)||(s->move==W_AIR&&o->rank));
    if(transfer)s->helm_cost_pending=0;
    unsigned previous=s->source;
    unsigned helm_red=(move==W_RISE?o->rank==3:s->wilds_helm_red);
    WDecision d=w_leave(s);d.kind=W_ENTER;d.move=move;
    s->move=move;s->rank=o->rank;s->source=source_for(move,o->rank);
#if ML_WILDS_MOVES
    if(move==W_FORESIGHT&&o->rank==3)s->source=918;
    s->valor_foresight=o->valor&&s->source==918;
    unsigned crimson_heading=0;
    if(move==W_CHIJIN)s->source=crimson_one(o,&crimson_heading,&d.crimson_direction);
#endif
    s->wilds_helm_red=(move>=W_RISE&&move<=W_LAND)?helm_red:0;
    s->wilds_stage=2;
    s->wilds_direction_pending=0;
    if(move==W_SLIDE&&o->base.magnitude>=50){
        int delta=ml_heading_delta(o->base.input_heading,directional_facing(o,o->base.input_heading));
        s->source=delta>=8192&&delta<=24576?911:delta<=-8192&&delta>=-24576?910:delta>24576||delta< -24576?912:903;
    }
    s->wilds_tier=1;s->wilds_elapsed=s->wilds_buffer=s->wilds_roll_buffer=s->wilds_r_wait=0;s->wilds_r_pending=0;
    s->wilds_thrust_buffer=0;
    s->wilds_red_three_held=0;
    s->wilds_gauge_fraction=0;s->wilds_bridge_hold=0;
    s->wilds_previous=o->base.held;s->wilds_sample=o->base.sample_sequence;
    s->slot=w_clip(s->source)->slot;s->bound=0;s->last_frame=0;
    s->counter_success=s->hit_success=s->rank_paid=s->follow_ready=0;
    s->wilds_iai_recovered=s->wilds_focus_break=s->iai_focus_locked=0;
    s->iai_follow_keys=0;s->iai_follow_wait=0;
    s->hold_seconds=0;s->consumed|=mask;s->queued=0;
    s->ground_node=s->main_source=0;s->entry_start=0;
    s->turn_seen=0;s->turn_sample=0;s->turn_duration=s->turn_fraction=0;
    s->heading=o->base.heading;
    if(o->base.magnitude>=50&&move!=W_SMALL&&move!=W_BIG&&move!=W_THRUST&&
       move!=W_DOWN&&move!=W_RISE&&move!=W_LAND&&move!=W_AIR&&move!=W_SLIDE&&move!=W_RELEASE)
        s->heading=o->base.input_heading;
    if(o->base.magnitude>=50&&move==W_AIR){
        /*17fe08f: jump entry turns toward the stick by at most W03+9e0
         *(file1472=60deg). Continuous TIML turning follows separately. */
        int delta=ml_heading_delta(o->base.input_heading,s->heading);
        if(delta>10923)delta=10923;else if(delta< -10923)delta= -10923;
        s->heading=(s->heading+delta)&65535u;
    }
#if ML_WILDS_MOVES
    if(move==W_CHIJIN)s->heading=crimson_heading;
    if(move==W_SLIDE&&o->base.magnitude>=50)s->heading=directional_facing(o,o->base.input_heading);
#endif
    d.source=s->source;d.slot=s->slot;d.heading=s->heading;
#if ML_WILDS_MOVES
    if(move==W_RELEASE&&previous==174){
        /* Keep the planted landing pose in the first part of the turn.
         * The clip still starts at F0 and its attack clock runs normally. */
        d.motion_contract=1;
        d.blend=8;
        d.speed=1;
    }
    /* Blend from the native Valor opener only after its guard window ends.
     * Start the slide clock at frame zero so movement and strikes stay in sync. */
    if(move==W_SLIDE && o->valor && o->state==16 && o->base.native_action==130 &&
       o->base.motion==0x5c0){
        d.motion_contract=1;
        d.blend=8.0f;
        d.speed=1.0f;
    }
#endif
    if(move>=W_RISE&&move<=W_LAND){
        const WMotionLink *link=w_motion_link(d.source,previous,W_BANK_W03_00,o->base.animation_frame);
        if(link){
            d.motion_contract=1;d.start=s->entry_start=link->start;
            /* Settle the upper body into recovery while the authored F6
             * landing clock and input windows continue to run. */
            d.blend=move==W_LAND&&previous==173?10:link->blend;
            d.speed=link->speed;
        }
    }
    if(move==W_FORESIGHT){
        s->foresight_gauge=o->base.gauge>=10;d.gauge_clear=1;
#if ML_WILDS_MOVES
        if(o->rank==3){s->foresight_gauge=1;d.gauge_clear=0;}
#endif
    }
    if(move==W_DOWN&&o->rank)s->helm_cost_pending=1;
    if(transfer){s->helm_cost_pending=1;s->helm_cost_seconds=.5f;}
    return d;
}
WDecision w_debug_enter(WState *s,const WObservation *o,unsigned move){return enter(s,o,move,0);}
#if ML_ASCENDANCE_PROTOTYPE
WDecision w_dividing_spirit_attack(WState *s,const WObservation *o,unsigned keys)
{
    return enter(s,o,chord(keys),keys);
}
WDecision w_dividing_sheathe(WState *s,const WObservation *o)
{
    return enter(s,o,W_SHEATHE,ML_R|ML_B);
}
WDecision w_radiance_enter(WState *s,const WObservation *o,unsigned move)
{
    return enter(s,o,move,ML_X|ML_A);
}
WDecision w_radiance_pursuit_enter(WState *s,const WObservation *o)
{
    if(s->move!=W_BIG||!s->counter_success)return (WDecision){0};
    unsigned rank_pending=!s->rank_paid;
    unsigned red_pending=s->rank==3&&o->rank==3&&!s->wilds_iai_recovered;
    WDecision d=w_radiance_enter(s,o,W_THRUST);
    if(d.kind==W_ENTER){
        if(rank_pending)big_counter_rank_reward(o,&d);
        if(red_pending)d.red_recovery|=W_RED_IAI;
    }
    return d;
}
#endif
unsigned w_iai_ready(const WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    if(s->move!=W_SHEATHE||s->actor!=b->actor||s->epoch!=b->epoch||o->state!=16||
       b->native_action!=w_action(W_SHEATHE)||b->motion!=0x500+s->slot||
       !o->alive||!o->drawn||!o->grounded||!o->resources_ready)return 0;
    const WEvent *event=w_event(s->source,b->animation_frame);
    return (input_stage(event,WI_R)|input_stage(event,WI_X))&2;
}
WDecision w_iai_attack(WState *s,const WObservation *o,unsigned key)
{
    const WEvent *event=w_event(s->source,o->base.animation_frame);
    if(key==ML_R&&(input_stage(event,WI_R)&2))return enter(s,o,W_BIG,ML_R);
    if(key==ML_X&&(input_stage(event,WI_X)&2))return enter(s,o,W_SMALL,ML_X);
    return (WDecision){0};
}
WDecision w_iai_press(WState *s,const WObservation *o)
{
#if ML_WILDS_MOVES && ML_WORLD_INPUT_VM
    const MLObservation *b=&o->base;
    if(w_iai_ready(s,o)&&s->controls.initialized){
        unsigned keys=b->held;
        if((keys==ML_R&&!(s->consumed&ML_R)&&!s->controls.buttons[2].raw)||
           (keys==ML_X&&!(s->consumed&ML_X)&&!s->controls.buttons[0].raw))
            return w_iai_attack(s,o,keys);
    }
#else
    (void)s;(void)o;
#endif
    return (WDecision){0};
}
unsigned w_red_draw(const WObservation *o)
{
    return !o->drawn&&o->rank==3&&o->alive&&o->grounded&&o->resources_ready&&
        (((o->state==1||o->state==2)&&o->base.native_action<=5)||
         w_uphill_run(o->state,o->base.native_action));
}
#if ML_WILDS_MOVES
unsigned w_sheathed_slide(const WObservation *o)
{
    return !o->drawn&&o->alive&&o->grounded&&o->resources_ready&&spirit_available(o,10)&&
        (o->state==1||o->state==2)&&o->base.native_action<=5&&
        o->base.held==(ML_R|ML_X|ML_A)&&o->base.magnitude>=50;
}
#endif
WDecision w_wilds_enter(WState *s,const WObservation *o,unsigned move)
{
    if(move<W_CHIJIN||move>W_FOCUS||!o->alive||!o->grounded||!o->resources_ready)return (WDecision){0};
    if(!o->drawn){
        unsigned draw=move==W_CHIJIN&&w_red_draw(o);
#if ML_WILDS_MOVES
        draw|=move==W_SLIDE&&w_sheathed_slide(o);
#endif
        if(s->move||!draw)return (WDecision){0};
    }
    if(move==W_CHIJIN&&o->rank!=3)return (WDecision){0};
    if(move==W_RELEASE&&(s->move!=W_LAND||!s->wilds_helm_red||
       o->rank<1+s->helm_cost_pending))return (WDecision){0};
    if(move==W_SLIDE&&!spirit_available(o,10))return (WDecision){0};
    /* A held sprint button belongs to the uphill draw, not its thrust chord. */
    unsigned uphill_sprint=move==W_CHIJIN&&!o->drawn&&
        w_red_sprint(o)&&(o->base.held&ML_R);
    WDecision d=enter(s,o,move,uphill_sprint?ML_R:0);
    if(move==W_RELEASE)d.rank_delta-=1;
    if(move==W_SLIDE&&o->rank!=3)d.gauge_delta-=10;
    return d;
}

#if ML_WILDS_MOVES
WDecision w_art_follow(WState *s,const WObservation *o,unsigned keys)
{
    if(s->move||!o->alive||!o->grounded||!o->resources_ready)return (WDecision){0};
    if(keys==ML_X)return w_wilds_enter(s,o,W_CHIJIN);
    if(!o->drawn)return (WDecision){0};
    if(keys==(ML_R|ML_Y))return w_wilds_enter(s,o,W_FOCUS);
    unsigned move=chord(keys);
    if(!move||(move==W_SHEATHE&&o->valor))return (WDecision){0};
    return enter(s,o,move,keys);
}
#endif

static WDecision combine(WDecision earned,WDecision next)
{
    next.rank_delta+=earned.rank_delta;
    next.valor_gain+=earned.valor_gain;
    next.red_recovery|=earned.red_recovery;
    next.gauge_clear|=earned.gauge_clear;next.gauge_fill|=earned.gauge_fill;
    if(earned.regen_seconds>next.regen_seconds)next.regen_seconds=earned.regen_seconds;
    return next;
}
static WDecision ground_motion(WState *s,unsigned source,unsigned previous,float frame,unsigned kind)
{
    const WClip *clip=w_clip(source);
    WDecision d={.kind=kind,.move=s->move,.source=source,.heading=s->heading,
                 .motion_contract=1,.speed=1};
    if(!clip){d.kind=W_ABORT;return d;}
    const WMotionLink *link=w_motion_link(source,previous,W_BANK_W03_00,frame);
    if(link){d.start=link->start;d.blend=link->blend;d.speed=link->speed;}
    s->source=source;s->slot=d.slot=clip->slot;s->bound=0;s->entry_start=d.start;
    if(s->queued)s->queued_source=source;
    return d;
}
/* Preserve the helper's LinkMotion as a real preparation phase. The FSM
 * helper's unconditional edge installs that phase on the destination action;
 * it is not a request to skip straight to the destination's main clip. */
static WDecision ground_enter(WState *s,const WObservation *o,unsigned node,unsigned mask)
{
    unsigned previous=s->move?s->source:w_source(o->base.motion),prep=0,main=0;
    if(!previous&&(o->state==1||o->state==2)&&o->base.native_action<=5)previous=1;
    const WGroundNode *n=w_ground_node(node);
    if(!n)return (WDecision){.kind=W_ABORT};
    if(n->kind==2){
        /* World311 sets LinkMotionPhase1 even when a missed counter costs red. */
        prep=node==76&&previous==311?(unsigned)n->phase1:n->value;
        int target=w_ground_select(node,0);if(target<0)return (WDecision){.kind=W_ABORT};
        node=(unsigned)target;n=w_ground_node(node);
    }
    unsigned special=node==28?W_FORESIGHT:node==45?W_THRUST:node==61?W_SHEATHE:
        node==64?W_SMALL:node==65?W_BIG:0;
    if(special){
        WDecision d=enter(s,o,special,mask);
        if(prep){
            s->main_source=s->source;
            WDecision phase=ground_motion(s,prep,previous,o->base.animation_frame,W_ENTER);
            return combine(d,phase);
        }
        return d;
    }
    switch(node){
    case 1:
        main=104;
        if(!prep&&(previous==1||previous==104))prep=132;
        if(!prep&&previous==107)prep=126;
        break;
    case 2:main=108;break;case 3:main=107;if(!prep)prep=119;break;
    case 4:main=105;break;case 5:main=109;break;case 6:main=110;break;
    case 7:main=103;break;case 8:main=128;break;case 9:main=127;break;
    case 10:main=102;break;case 11:main=101;break;case 12:main=106;break;
    case 23:main=112;break;case 38:main=107;break;
    default:return (WDecision){.kind=W_ABORT};
    }
    WDecision earned=w_leave(s);s->move=W_GROUND;s->ground_node=node;s->rank=o->rank;
    s->heading=o->base.heading;s->consumed|=mask;
    s->turn_seen=0;s->turn_sample=0;s->turn_duration=s->turn_fraction=0;
    s->counter_success=s->hit_success=s->rank_paid=0;
    s->main_source=prep?main:0;
    WDecision d=ground_motion(s,prep?prep:main,previous,o->base.animation_frame,W_ENTER);
    unsigned cost=node==1||node==12?15:node==2||node==6?20:node==5?25:0;
    s->spirit_paid=cost && spirit_available(o,cost);
    if(s->spirit_paid){
        /* Spirit-I accepts an empty bar; later source edges test their cost
         * before entry. Spending belongs to action init, not phase changes. */
#if ML_WILDS_MOVES
        if(o->rank!=3)
#endif
            d.gauge_delta=-(int)cost;
    }
    return combine(earned,d);
}
#if ML_WORLD_INPUT_VM
#include "world_command_driver.inc"
#if ML_WILDS_MOVES
WDecision w_art_spirit(WState *s,const WObservation *o,unsigned moving_two)
{
    if(s->move||!o->alive||!o->drawn||!o->grounded||!o->resources_ready||o->valor)
        return (WDecision){0};
    if(o->base.magnitude>=50){
        if(moving_two&&spirit_available(o,15)){
            wilds_spirit_direction(s,&o->base);
            return wilds_moving_spirit_two(s,o);
        }
        WDecision d=w_wilds_enter(s,o,W_SLIDE);
        if(d.kind!=W_KEEP)return d;
        return wilds_directional_weak_spirit(s,o);
    }
    return wilds_native(s,o,spirit_available(o,15)?16:17);
}
#endif
#endif
void w_input_finish(WState *s,const WObservation *o,WDecision d)
{
#if ML_WORLD_INPUT_VM
    const WEvent *event=0;unsigned source=0;
    if(d.kind==W_ENTER||d.kind==W_MOTION){
        w_controls_bind(&s->controls);s->input_motion=0x500+d.slot;
        source=d.source;event=w_event(source,d.motion_contract?d.start:0);
    }else if(d.kind==W_IDLE||d.kind==W_ROLL||d.kind==W_ABORT){
        w_controls_bind(&s->controls);s->input_motion=0;
        if(d.kind==W_ABORT)s->controls.force_clear=1;
        if(d.kind==W_IDLE)event=w_event(1,0);
    }else{
        source=s->move?s->source:w_guild_source(o->state,o->base.native_action,o->base.motion);
        event=w_event(source,o->base.animation_frame);
    }
    uint32_t permission[8];world_input_permission(source,event,permission);
    w_controls_finish(&s->controls,permission);
#else
    (void)s;(void)o;(void)d;
#endif
}
WDecision w_step(WState *s,const WObservation *o)
{
    WDecision d={0};const MLObservation *b=&o->base;
    if(s->actor!=b->actor||s->epoch!=b->epoch){
        /* An area load can replace the native action without submitting a
         * request through our action hook. Settle this hunter's exit before
         * discarding its old epoch. Never bill a newly allocated hunter for
         * the previous actor's pending move. Held transition keys stay spent. */
        if(s->actor==b->actor&&s->actor)d=w_leave(s);
        w_reset(s);s->actor=b->actor;s->epoch=b->epoch;
        s->previous_held=s->consumed=b->held;w_keys_tick(&s->keys,b);return d;
    }
#if ML_WILDS_MOVES && ML_WORLD_INPUT_VM
    if(!o->drawn){
        s->wilds_direction_pending=0;
        s->wilds_bridge_held=b->held;
    }
#endif
    unsigned sheathing=s->move==W_SHEATHE_END;
#if ML_WILDS_MOVES
    sheathing|=(s->source==917&&b->animation_frame>=W_ROUND_SHEATHE_FRAME)||(s->source==904&&b->animation_frame>=W_RELEASE_SHEATHE_FRAME);
#endif
    if(!o->alive||(!o->drawn&&!sheathing)||!o->resources_ready){if(!s->move){s->controls=(WControls){0};s->foresight_press.motion=0;s->wilds_charge_link=0;s->wilds_charge_open=0;s->wilds_chain_source=0;s->spirit_crimson_pending=0;return d;}d=combine(d,w_leave(s));d.kind=W_ABORT;return d;}
    w_keys_tick(&s->keys,b);
    if(s->move&&!w_private(o->state,b->native_action)){d=combine(d,w_leave(s));d.kind=W_ABORT;return d;}
    unsigned new_sample=s->sample!=b->sample_sequence;
    if(new_sample){s->previous_held=b->held;s->sample=b->sample_sequence;s->consumed&=b->held;}
    unsigned held=b->held&~s->consumed;
    unsigned source=s->move?s->source:w_source(b->motion);
    float frame=b->animation_frame;
    if(!(frame>=0&&frame<100000)){if(!s->move)return d;d=combine(d,w_leave(s));d.kind=W_ABORT;return d;}
    if(s->move){
        if(b->native_action!=w_action(s->move)){d=combine(d,w_leave(s));d.kind=W_ABORT;return d;}
        if(b->motion!=0x500+s->slot)return d; /* Never execute against outgoing clock. */
        s->bound=1;
        if(o->counter)s->counter_success=1;
        if(o->contact&&!s->hit_success){
            s->hit_success=1;
            if(s->valor_foresight&&s->source==918)d.regen_seconds=15;
            if(s->move==W_SMALL)d.regen_seconds=s->counter_success?30:15;
            if(s->move==W_FORESIGHT&&s->source!=918&&s->counter_success){
                d.gauge_fill=1;
#if ML_WILDS_MOVES
                if(o->valor_style&&!o->valor)d.valor_gain=W_VALOR_COUNTER_GAIN;
#endif
            }
        }
        if(s->move==W_CHARGE&&(s->source==917||s->source==W_CRIMSON_SPIN)&&s->hit_success&&!s->rank_paid){
            s->rank_paid=1;spirit_round_reward(&d,o->rank);
        }
        if(s->move==W_BIG&&frame>=37&&!s->rank_paid){
#if ML_WILDS_MOVES
            if(s->counter_success){
                s->rank_paid=1;
                big_counter_rank_reward(o,&d);
            }
#else
            s->rank_paid=1;if(!s->counter_success&&s->rank)d.rank_delta=-1;
#endif
        }
#if ML_WILDS_MOVES
        if(s->move==W_BIG&&s->rank==3&&o->rank==3&&s->counter_success&&s->hit_success&&!s->wilds_iai_recovered){
            s->wilds_iai_recovered=1;d.red_recovery|=W_RED_IAI;
        }
#endif
        if(s->move==W_LAND&&s->helm_cost_pending&&new_sample&&
           b->simulation_delta>0&&b->simulation_delta<30){
            s->helm_cost_seconds-=b->simulation_delta/60.0f;
            if(s->helm_cost_seconds<=0){s->helm_cost_pending=0;if(s->rank)d.rank_delta=-1;}
        }
    }
    const WEvent *e=w_event(source,frame);
    if(s->move==W_GROUND&&s->ground_node==5&&s->hit_success&&!s->rank_paid&&e&&(e->trigger&0x40000000u)){
        s->rank_paid=1;spirit_round_reward(&d,o->rank);
    }
#if ML_WORLD_INPUT_VM
    return combine(d,world_command_step(s,o,source,e));
#endif
    if(s->move==W_GROUND&&s->main_source&&
       (b->motion_finished||frame>=w_clip(source)->length-1)){
        unsigned main=s->main_source;s->main_source=0;
        return combine(d,ground_motion(s,main,source,frame,W_MOTION));
    }
    if(new_sample&&b->simulation_delta>0&&b->simulation_delta<30)
        s->solo_age+=b->simulation_delta/60.0f;
    unsigned selected=chord(b->held),mask=selected==W_SHEATHE?ML_R|ML_B:selected==W_FORESIGHT?ML_R|ML_A:ML_R|ML_X;
    /* A held modifier may join a newly pressed partner. Consume the whole
     * chord on entry, but do not require both keys to be released together. */
    if(!(held&mask))selected=0;
    if(!s->move&&(o->state==1||o->state==2)&&b->native_action<=5&&o->grounded&&selected==W_THRUST)
        return combine(d,enter(s,o,W_THRUST,mask));
    if(!s->move&&o->state!=16){s->queued=0;return d;}
    if(s->queued&&s->queued_source!=source)s->queued=0;
    if(s->move==W_SHEATHE){
        /* Native start -> loop changes action at phase1 without rebinding306. */
        if(new_sample&&e&&e->phase==1&&b->simulation_delta>0&&b->simulation_delta<30)
            s->hold_seconds+=b->simulation_delta/60.0f;
        unsigned choice=(held&ML_R)?W_BIG:(held&ML_X)?W_SMALL:0;
        if(choice)offer(s,e,source,choice,choice==W_BIG?ML_R:ML_X,
                        choice==W_BIG?WI_R:WI_X,b->heading);
        unsigned stage=e?(e->input[s->queued_word]>>s->queued_shift)&3:0;
        if(s->queued&&stage==3)return combine(d,enter(s,o,s->queued,s->queued_mask));
        if(s->hold_seconds>=3.5f)return combine(d,enter(s,o,W_SHEATHE_END,0));
        return d;
    }
    if(s->move==W_THRUST&&s->hit_success&&o->rank>=1)return combine(d,enter(s,o,W_RISE,0));
    if(s->move==W_RISE){if(b->motion_finished||frame>=w_clip(source)->length-1)return combine(d,enter(s,o,W_AIR,0));return d;}
    if(s->move==W_AIR){
        if(o->grounded&&frame>0)return combine(d,enter(s,o,W_LAND,0));
        unsigned stage=e?(e->input[0]>>30)&3:0;
        if((held&(ML_X|ML_A|ML_R))&&stage){s->queued=W_DOWN;s->queued_source=source;}
        /* The source air-attack execute interval opens at42. Complete the
         * successful Helm automatically there, retaining the earlier turn
         * interval and optional buffered attack without requiring a key. */
        if(stage==3)return combine(d,enter(s,o,W_DOWN,s->queued==W_DOWN?ML_X|ML_A|ML_R:0));
        if(b->motion_finished||frame>=w_clip(source)->length-1)
            return combine(d,enter(s,o,W_DOWN,0));
        return d;
    }
    if(s->move==W_DOWN){if(o->grounded&&frame>0)return combine(d,enter(s,o,W_LAND,0));return d;}
    if(o->grounded&&selected&&e){
        if(source==109&&selected!=W_SHEATHE)selected=0;
        if(source==174&&selected!=W_SHEATHE&&selected!=W_FORESIGHT)selected=0;
        if(source==140&&selected==W_FORESIGHT)selected=0;
        if(source>=308&&source<=311&&selected!=W_THRUST)selected=0;
        if(selected)offer(s,e,source,selected,mask,move_input(selected),b->heading);
    }
    /* A source-owned buffered gesture survives release until its execute stage;
     * a motion change or interrupt discards it. No invented 12-frame timeout. */
    if(s->move&&!selected&&!chord(b->held)&&e){
        unsigned follow=(source==140||source==307);
        if((held&ML_B)&&!(held&ML_R))
            offer(s,e,source,0x200,ML_B,WI_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        else if((held&(ML_X|ML_A))==(ML_X|ML_A)&&(follow||source==170)){
            unsigned fade=ml_choose_fade(b->heading,b->input_heading,b->magnitude);
            WInput input=fade==ML_FADE_LEFT?WI_FADE_LEFT:fade==ML_FADE_RIGHT?WI_FADE_RIGHT:WI_FADE;
            unsigned choice=source==307?0x400+(fade==ML_FADE_LEFT?81:fade==ML_FADE_RIGHT?80:79):
                0x100+ml_move(fade)->native_action;
            if(!offer(s,e,source,choice,ML_X|ML_A,input,b->heading))
                offer(s,e,source,source==307?0x44f:0x100+ml_move(ML_FADE_BACK)->native_action,ML_X|ML_A,WI_FADE,b->heading);
        }else if(follow&&(held&ML_R)&&
                 ((source==140&&s->counter_success&&s->hit_success)||source==307))
            /* 307 -> 363 -> 104 at42, observed in World421810. */
            offer(s,e,source,source==140?0x11e:0x452,ML_R,WI_R,b->heading);
        else if(follow&&(held&ML_X))
            offer(s,e,source,source==307?0x44d:0x100,ML_X,WI_X,b->heading);
        else if(follow&&(held&ML_A))
            offer(s,e,source,source==307?0x44e:0x104,ML_A,WI_A,b->heading);
    }
    if(s->move==W_GROUND&&!selected&&!(held&(ML_B|ML_Y))){
        /* The generated graph owns Spirit context through face-button
         * insertions. Node23 resumes II; node38 resumes III. */
        uint64_t properties=0;
        if(o->base.gauge>=15)properties|=(uint64_t)1<<16;
        if(o->base.gauge>=20)properties|=(uint64_t)1<<17;
        if(o->base.gauge>=25)properties|=(uint64_t)1<<15;
        if(o->rank)properties|=(uint64_t)1<<10;
        WInput input=WI_R;unsigned input_mask=0;
        if(held==ML_R){properties|=1;input=WI_R;input_mask=ML_R;}
        else if(held==ML_X){properties|=(uint64_t)1<<3;input=WI_X;input_mask=ML_X;}
        else if(held==ML_A){properties|=(uint64_t)1<<7;input=WI_A;input_mask=ML_A;}
        else if((held&(ML_X|ML_A))==(ML_X|ML_A)){
            unsigned fade=ml_choose_fade(b->heading,b->input_heading,b->magnitude);
            unsigned bit=fade==ML_FADE_LEFT?5:fade==ML_FADE_RIGHT?6:4;
            properties|=(uint64_t)1<<bit;
            input=fade==ML_FADE_LEFT?WI_FADE_LEFT:fade==ML_FADE_RIGHT?WI_FADE_RIGHT:WI_FADE;
            input_mask=ML_X|ML_A;
        }
        int next=w_ground_select(s->ground_node,properties);
        if(next>=0&&input_mask)offer(s,e,source,0x400+(unsigned)next,input_mask,input,b->heading);
    }
    /* World thrust <-> rising slash loop. Native GU's rising slash lacks
     * the circle return. Use the source permission intervals for both ends;
     * leave X+A to native fade and let a late R supersede a queued solo key. */
    unsigned loop=!s->move&&((source==105&&b->native_action==4)||
                            (source==107&&(b->native_action==25||b->native_action==7)));
    /* Source FSM nodes3/4 both link R to action1001 (Spirit-I). GU can
     * discard an early R before it ever submits a native request, so own
     * this buffer at the raw-input stage just like the alternating links.
     * A later chord replaces it through offer(); a released tap survives. */
    unsigned native_attack=!s->move&&source>=101&&source<=110&&b->native_action<154;
    unsigned basic_r=native_attack&&source!=104&&source!=106&&source!=108;
    if(basic_r&&o->grounded&&!selected&&held==ML_R&&
       (input_stage(e,WI_R)||r_window_ahead(source,frame))){
        s->queued=0x300;s->queued_mask=ML_R;s->queued_source=source;
        s->queued_word=WI_R.word;s->queued_shift=WI_R.shift;s->queued_heading=b->heading;
    }
    /* World FSM Spirit-I -> KijinStab, Spirit-II -> KijinUpper accepts
     * either face button. GU22 is SpiritStab; native25's previous-action
     * context retains Spirit-II -> upper -> Spirit-III through the adapter. */
    if(native_attack&&!selected&&!(held&(ML_R|ML_B|ML_Y))&&
       (held&(ML_X|ML_A))!=(ML_X|ML_A)&&(source==104||source==106)){
        if(held&ML_X)offer(s,e,source,0x100+(source==104?22:25),ML_X,WI_X,b->heading);
        else if(held&ML_A)offer(s,e,source,0x100+(source==104?22:25),ML_A,WI_A,b->heading);
    }
    if(loop&&!selected&&!(held&(ML_R|ML_B|ML_Y))&&
       (held&(ML_X|ML_A))!=(ML_X|ML_A)){
        /* GU action7 is the draw-upper: prep116 then107 at frame20.
         * Native combo upper25 binds107 directly, preserving its attack row. */
        if(source==105&&(held&ML_X))offer(s,e,source,0x119,ML_X,WI_X,b->heading);
        if(source==107&&(held&ML_A))offer(s,e,source,0x104,ML_A,WI_A,b->heading);
    }
    if(loop&&(held&(ML_X|ML_A))==(ML_X|ML_A)&&s->queued>=0x100)s->queued=0;
    if(s->queued&&e){
        unsigned stage=(e->input[s->queued_word]>>s->queued_shift)&3;
        if(!stage){
            WInput input={s->queued_word,s->queued_shift};
            if(s->move||!input_window_ahead(source,frame,input))s->queued=0;
        }
        else if(stage==3&&((s->queued==0x300)?(s->keys.ready&ML_R):
                         (!loop||s->queued<0x100||(s->keys.ready&s->queued_mask)))){
            if(s->queued<0x100)return combine(d,enter(s,o,s->queued,s->queued_mask));
            if(s->queued>=0x400)return combine(d,ground_enter(s,o,s->queued-0x400,s->queued_mask));
            unsigned choice=s->queued,mask=s->queued_mask,heading=s->queued_heading;
            if(loop&&(choice==0x119||choice==0x104))heading=b->heading;
            d=combine(d,w_leave(s));s->consumed|=mask;d.heading=heading;
            d.kind=choice==0x300?W_NATIVE_R:choice==0x200?W_ROLL:W_NATIVE;d.native_action=choice-0x100;
            return d;
        }
    }
    if(s->move&&(b->motion_finished||frame>=w_clip(source)->length-1)){d=combine(d,w_leave(s));d.kind=W_IDLE;}
    return d;
}
