/* Optional Guild-only prototype. The stock build omits this controller. */
enum { PAD_BOOST=0x2, BOOST_COST=100, BOOST_CAPACITY=3*BOOST_COST, BOOST_HIT=12, BOOST_WOUND=30,
       BOOST_DURATION=25*60, BOOST_SHEATHE_WINDOW=60, RADIANCE_INPUT=ML_X|ML_A, RADIANCE_STAMINA=0x13e0 };
enum { RADIANCE_OFF,RADIANCE_HOLD,RADIANCE_RELEASE,RADIANCE_FLASH,RADIANCE_CHASE,RADIANCE_FOLLOW };
enum { DIV_INPUT_NONE, DIV_INPUT_SPIRIT_HELD, DIV_INPUT_ROUND, DIV_INPUT_SHEATHE, DIV_INPUT_ROLL, DIV_INPUT_PRIMARY, DIV_INPUT_SECONDARY, DIV_INPUT_FADE, DIV_INPUT_THRUST, DIV_INPUT_FORESIGHT, DIV_INPUT_CHARGE };
static struct {
    void *actor;
    unsigned area,sample,charge,sheathe,enhanced_iai;
    unsigned radiance,boost_toggle_pending,radiance_hit,radiance_sample;
    float remaining,ready_frames;
    unsigned musou_sheathe_r,flash_r_buffered;
    float walk_speed;
    unsigned walk_intent;
    float flash_travel;
    unsigned double_cut,double_credited,double_step;
    float double_travel_frame;
    unsigned resheathe_pending_key,resheathe_handoff;
    unsigned dividing_input,dividing_input_heading,dividing_magnitude,dividing_fade_action;
    unsigned dividing_sample;
    unsigned dividing_step_pending,dividing_step_heading,dividing_step_sample;
    float dividing_hold_ticks,dividing_travel_frame;
    unsigned flash_step_release;
    unsigned double_resheathe_step;
    unsigned quest;
    unsigned radiance_entry_sample,radiance_entry_counter;
    unsigned dividing_exit_sample;
    unsigned counter_sheathe;
    float passive_frames;
    unsigned helm_power,release_power;
} ascendance;

/* Pursuit follows the particular monster whose native reaction accepted
 * this flash. Keep its registry ID and status alongside the contact. */
static struct {
    void *target,*status;
    unsigned uid,region,confirmed,buffered,near,rewarded,sample;
    float age,dx,dz,speed;
    unsigned independent,brake_cancel,travel_sample;
    float wall_stall_frames;
    float first_hit_hold,first_hit_frame;
    unsigned first_hit_sample,first_hit_pose_pending;
} radiance_pursuit;
enum { PURSUIT_FIRST_HIT_HOLD = 30 };
static void *ascendance_velocity_actor;
enum { MUSOU_NONE,MUSOU_FLURRY,MUSOU_DASH,MUSOU_CUT,MUSOU_ENTRY,MUSOU_SEAT };
enum { MUSOU_LEGACY_CYCLES=5,MUSOU_MAX_HITS=8,MUSOU_SUSTAIN_FRAMES=9 };
static const struct {unsigned char clip,group;} musou_chain[]={
    {MUSOU_FORWARD,1},{MUSOU_REVERSE,1},{MUSOU_FORWARD,1},{MUSOU_SPIN,1},
    {MUSOU_REVERSE,2},{MUSOU_FORWARD,2},{MUSOU_REVERSE,2},{MUSOU_SPIN,2},
    {MUSOU_FORWARD,3},{MUSOU_REVERSE,3},{MUSOU_FORWARD,3},{MUSOU_REVERSE,3},{MUSOU_SPIN,3},
    {MUSOU_REVERSE,4},{MUSOU_FORWARD,4},{MUSOU_REVERSE,4},{MUSOU_FORWARD,4},{MUSOU_SPIN,4},
};
#define MUSOU_STROKE_COUNT (sizeof(musou_chain)/sizeof(*musou_chain))
#define MUSOU_STROKE_RATE 1.35f
/* Long Sword Boost 1.1.0 scales the whole Musou sequence by 1.15 × 1.30.
 * The delayed burst already inherits this from its recorded blade hit. */
static const float musou_damage_multiplier=1.15f*1.30f;
/* The reference scales the authored 4.8 m by 2.5. The return uses the
 * actual displacement after floor and wall collision. */
#define MUSOU_DASH_UNITS 1200
#define RADIANCE_WALK_UNITS_PER_TICK (650.f/60.f)
typedef struct {
    void *target,*status;
    unsigned uid,id,region,damage,motion_value;
    float offset[3];
} MusouBurstTarget;
static struct {
    unsigned phase,cycles,hits,sustain_sample,last_attack,hit_seen;
    unsigned dash_heading,return_heading,stroke,opening_path,second_path,group_two_side,late_group;
    float start_x,start_z,return_dx,return_dz,travel_frame,sustain_frames,spin_x,spin_z;
    float wall_root_frame;
    unsigned spin_sample,seat_input_sample,seat_key;
    float seat_input_buffer;
    unsigned burst_level,burst_sent,burst_count;
    MusouBurstTarget burst_targets[10];
} musou;
#if ML_WILDS_MOVES
enum { MUSOU_RED_BUFFER_FRAME=37, MUSOU_RED_CANCEL_FRAME=69 };
static struct {
    unsigned hit,buffered;
} red_musou;

#endif
static void *world_monster(unsigned uid);
#if ML_WORLD_PRESENTATION
static void world_helm_impact(void *,const float *);
#endif
extern void ml_ascendance_gauge(unsigned);

static unsigned ascendance_in_town(void)
{
    const void *control=(const void *)(uptr)word((const void *)main_base,0x188ca48);
    return control&&byte(control,0x4c)!=255;
}

static unsigned ascendance_quest_data(void)
{
    const void *quest=(const void *)(uptr)word((const void *)main_base,0x188ad90);
    return quest?word(quest,0xa4):0;
}

static void ascendance_rebind(void *a,unsigned quest,unsigned area,unsigned sample,unsigned carry)
{
    unsigned charge=carry?ascendance.charge:0;
    float remaining=carry?ascendance.remaining:0;
    float passive=carry?ascendance.passive_frames:0;
    ascendance=(__typeof__(ascendance)){
        .actor=a,.quest=quest,.area=area,.sample=sample,.charge=charge,.remaining=remaining,.passive_frames=passive};
    musou=(__typeof__(musou)){0};
    radiance_pursuit=(__typeof__(radiance_pursuit)){0};
#if ML_WILDS_MOVES
    red_musou=(__typeof__(red_musou)){0};
#endif
}

static void ascendance_quest_poll(void)
{
    /* Town input still updates when the quest hunter no longer does. */
    if(ascendance.actor&&ascendance_in_town()){
        ascendance_rebind(0,0,0,0,0);
        ml_ascendance_gauge(0);
    }
}

static unsigned ascendance_owner(void *a)
{
    return a&&ascendance.actor==a&&ascendance.area&&ascendance.area!=255&&ascendance.area==byte(a,AREA)&&
        (int16_t)u16(a,HP)>0&&hunter_style(a)==0;
}

static float ascendance_attack_rate(void *a,unsigned source)
{
    if(!ascendance_owner(a)||ascendance.remaining<=0)return 1.f;
    if(source==906||source==907)return 1.2f;
    if(source==109||source==917||source==W_CRIMSON_SPIN)return 1.1f;
    return 1.f;
}

static void ascendance_apply_attack_rate(void *a,float rate)
{
    /* The nominal speed returns after native hitstop. Preserve the lowered
     * clock during contact and only replace the ordinary playback rate. */
    set_real(a,0x14b0,rate);
    if(real(a,0x50c)>=.999f)set_real(a,0x50c,rate);
}

#if ML_WILDS_MOVES
static unsigned ascendance_red_spin(void *a)
{
    return ascendance_owner(a)&&world.move==W_CHARGE&&world.source==W_CRIMSON_SPIN&&
        world_bound&&world_bound_actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_CHARGE)&&u16(a,MOTION)==0x500+world.slot;
}
#endif

static unsigned ascendance_owned_blade(const void *attack)
{
    return attack&&world_attack_owned&&(uptr)attack==(uptr)world_attack_actor+0x3020&&
        u16(attack,0x34)==world_attack_id;
}

static void ascendance_musou_mark(void *target,void *status,unsigned region,
                                  const float *point,unsigned damage)
{
    if(!musou.opening_path||musou.phase<MUSOU_FLURRY||musou.phase>MUSOU_CUT)return;
    unsigned uid=byte(status,0x1b5),id=u16(status,0x1b6);
    if(world_monster(uid)!=target)return;
    for(unsigned n=0;n<musou.burst_count;++n){
        const MusouBurstTarget *existing=musou.burst_targets+n;
        if(existing->target==target&&existing->status==status&&existing->id==id)return;
    }
    if(musou.burst_count>=sizeof(musou.burst_targets)/sizeof(*musou.burst_targets))return;
    MusouBurstTarget *mark=musou.burst_targets+musou.burst_count++;
    unsigned mv=musou.phase==MUSOU_FLURRY?(musou_spin_slot(world.slot)?6:12):14;
    *mark=(MusouBurstTarget){.target=target,.status=status,.uid=uid,.id=id,.region=region,
        .damage=damage,.motion_value=mv};
    for(unsigned k=0;k<3;++k)mark->offset[k]=point[k]-real(target,0x40+4*k);
    log_event("ascendance_musou_mark",(u32)(uptr)target,musou.phase,region,damage);
}

static void ascendance_musou_burst(void *a,float frame)
{
    if(frame<58||musou.burst_sent||!musou.opening_path)return;
    musou.burst_sent=1;
    static const unsigned mv_by_level[]={0,35,65,100,150};
    unsigned level=musou.burst_level?musou.burst_level:1;
    unsigned mv=mv_by_level[level-1],hits=0;
    if(mv){
        for(unsigned i=0;i<musou.burst_count;++i){
            const MusouBurstTarget *mark=musou.burst_targets+i;
            void *target=mark->target,*status=mark->status;
            if(world_monster(mark->uid)!=target||byte(target,AREA)!=byte(a,AREA)||
               word(target,0x1428)!=(u32)(uptr)status||u16(status,0x1b6)!=mark->id||
               (int)word(status,0x370)<=0)continue;
            unsigned damage=(mark->damage*mv+mark->motion_value/2)/mark->motion_value;
            int applied=((int (*)(void *,unsigned,int,unsigned))(main_base+0x9d6f8))
                (target,mark->region,(int)damage,0);
            if(applied<=0)continue;
            ++hits;
#if ML_WORLD_PRESENTATION
            float point[3];
            for(unsigned k=0;k<3;++k)point[k]=real(target,0x40+4*k)+mark->offset[k];
            world_helm_impact(a,point);
#endif
            log_event("ascendance_musou_burst_hit",(u32)(uptr)target,(u32)frame,damage,(u32)applied);
        }
    }
    log_event("ascendance_musou_burst",(u32)(uptr)a,(u32)frame,level,hits);
}

static unsigned ascendance_double_blade(void *a)
{
    return ascendance.double_cut&&ascendance_owner(a)&&world.move==W_SMALL&&world_bound&&
        world_bound_actor==(u32)(uptr)a&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_SMALL)&&
        u16(a,MOTION)==0x500+world.slot&&double_draw_slot(world.slot);
}

static void ascendance_hit(void *a,const void *attack,void *target,unsigned wound,unsigned damage)
{
    if(!ascendance_owner(a))return;
#if ML_WILDS_MOVES
    if(damage&&!red_musou.hit&&ascendance.remaining>0&&ascendance_red_spin(a)&&
       ascendance_owned_blade(attack)){
        red_musou.hit=1;
        log_event("ascendance_musou_red_hit",(u32)(uptr)a,(u32)(uptr)target,(u32)real(a,FRAME),damage);
    }
#endif
    if(damage&&ascendance_double_blade(a)&&
       ascendance.double_credited!=(world_hit==65?2u:1u)&&ascendance_owned_blade(attack)){
        ascendance.double_credited=world_hit==65?2:1;
        unsigned before=word(a,0x2780);
        world_original_gauge_gain(a,10,(u32)-1,0);
        log_event("ascendance_double_hit",ascendance.double_credited,before,word(a,0x2780),damage);
    }
    if(damage&&ascendance.radiance==RADIANCE_FLASH&&world.move==W_BIG&&
       !ascendance.radiance_hit&&ascendance_owned_blade(attack)){
        ascendance.radiance_hit=1;
        unsigned before=word(a,0x2780);
        ((void (*)(void *,int,int))(main_base+0x2a309c))(a,50,-1);
        log_event("ascendance_flash_hit",(u32)(uptr)a,before,word(a,0x2780),damage);
    }
    if(damage&&ascendance.radiance==RADIANCE_FOLLOW&&target==radiance_pursuit.target&&
       world.move==W_SMALL&&(!radiance_pursuit.independent||world.slot==PURSUIT_FIRST_SLOT)&&
       !radiance_pursuit.rewarded&&ascendance_owned_blade(attack)){
        radiance_pursuit.rewarded=1;
        if(radiance_pursuit.independent){
            radiance_pursuit.first_hit_hold=PURSUIT_FIRST_HIT_HOLD;
            radiance_pursuit.first_hit_frame=real(a,FRAME);
            radiance_pursuit.first_hit_sample=sample_sequence;
            radiance_pursuit.first_hit_pose_pending=1;
        }
        unsigned before=word(a,0x2780);
        ((void (*)(void *,int,int))(main_base+0x2a309c))(a,50,-1);
        log_event("ascendance_follow_hit",(u32)(uptr)a,before,word(a,0x2780),damage);
    }
    if(damage&&musou.phase&&ascendance_owned_blade(attack)){
        if(musou.phase==MUSOU_FLURRY){
            unsigned id=u16(attack,0x34);
            if(!musou.hit_seen||musou.last_attack!=id){
                musou.hit_seen=1;
                musou.last_attack=id;
                if(musou.hits<MUSOU_MAX_HITS)++musou.hits;
            }
        }
        log_event("ascendance_musou_damage",(u32)(uptr)target,musou.phase,musou.hits,damage);
    }
    if(damage&&world.move==W_BIG&&!musou.phase)
        log_event("ascendance_iai_damage",(u32)(uptr)a,world.source,damage,ascendance.enhanced_iai);
    if(wound&&ascendance.remaining>0){
        float before=ascendance.remaining;
        ascendance.remaining=BOOST_DURATION;
        log_event("ascendance_wound_refill",(u32)(uptr)a,(u32)(uptr)target,(u32)before,BOOST_DURATION);
    }
    if((!wound&&!damage)||ascendance.charge==BOOST_CAPACITY)return;
    unsigned next=ascendance.charge+(wound?BOOST_WOUND:BOOST_HIT);
    ascendance.charge=next>BOOST_CAPACITY?BOOST_CAPACITY:next;
    log_event("ascendance_charge",(u32)(uptr)a,ascendance.charge,wound,damage);
}

static float ascendance_attack_scale(void *attack)
{
    void *a=ascendance.actor;
    if(!a||(uptr)attack!=(uptr)a+0x3020||!ascendance_owner(a))return 1.0f;
    if(musou.phase&&ascendance_owned_blade(attack)){
        if(musou.phase==MUSOU_FLURRY){
            if(!musou_clip(u16(a,MOTION)))return .56f*musou_damage_multiplier;
            if(musou_spin_slot(world.slot))return .275f*musou_damage_multiplier;
            return (world.slot==musou_strokes[MUSOU_SPIN].slot?.55f:.43f)*musou_damage_multiplier;
        }
        if(musou.opening_path)return 1.05f*musou_damage_multiplier;
        float bonus=musou.hits>=2?musou.hits*(musou.phase==MUSOU_DASH?.10f:.16f):0;
        return (1.05f+bonus)*musou_damage_multiplier;
    }
    unsigned boost=ascendance.remaining>0;
    if(ascendance.enhanced_iai&&world.move==W_BIG&&world_attack_owned&&
       world_attack_actor==(u32)(uptr)a&&u16(attack,0x34)==world_attack_id)
        return boost?1.68f:1.6f;
    if(world_attack_owned&&(world.move==W_RISE||world.move==W_AIR||world.move==W_DOWN||world.move==W_LAND)&&
       ascendance.helm_power)return ascendance.helm_power*.01f;
    if(world_attack_owned&&world.move==W_RELEASE&&ascendance.release_power)return ascendance.release_power*.01f;
    unsigned round=u16(a,STATE)==16&&u16(a,ACTION)==30&&u16(a,MOTION)==0x500+127;
    if(boost&&(round||(world_attack_owned&&(world.source==906||world.source==907||world.source==109||
       world.source==917||world.source==W_CRIMSON_SPIN))))return 1.25f;
    return boost?1.05f:1.0f;
}

static void ascendance_transition(void *a,const WDecision *d)
{
    if(!ascendance_owner(a))return;
    if(d->kind==W_ENTER&&d->move==W_THRUST)ascendance.helm_power=ascendance.remaining>0?170:120;
    if(d->kind==W_ENTER&&d->move==W_RELEASE){
        ascendance.release_power=ascendance.remaining>0?170:120;
        log_event("ascendance_author_release",(u32)(uptr)a,ascendance.release_power,(u32)ascendance.remaining,0);
    }
#if ML_WILDS_MOVES
    red_musou=(__typeof__(red_musou)){0};
#endif
    ascendance.resheathe_handoff=ascendance.double_cut&&d->kind==W_ENTER&&d->move==W_SHEATHE;
    if(!ascendance.resheathe_handoff)
        ascendance.resheathe_pending_key=0;
    if(d->kind==W_MOTION)return;
    ascendance.double_resheathe_step=ascendance.resheathe_handoff&&ascendance.double_step&&
        u16(a,MOTION)==0x500+DOUBLE_DRAW_RETURN;
    if(!(d->kind==W_ENTER&&d->move==W_SMALL&&double_draw_slot(d->slot)))ascendance.double_cut=0;
    ascendance.walk_speed=0;ascendance.walk_intent=0;
    if(!(d->kind==W_ENTER&&(((musou.phase==MUSOU_FLURRY||musou.phase==MUSOU_ENTRY)&&d->move==W_SMALL)||
                           (musou.phase==MUSOU_DASH&&d->move==W_BIG)||
                           ((musou.phase==MUSOU_CUT||musou.phase==MUSOU_SEAT)&&d->move==W_SMALL))))
        musou=(__typeof__(musou)){0};
    if(ascendance.radiance==RADIANCE_RELEASE&&d->kind==W_ENTER&&d->move==W_BIG)
        ascendance.radiance=RADIANCE_FLASH;
    else if(!(d->kind==W_ENTER&&
       ((ascendance.radiance==RADIANCE_HOLD&&d->move==W_SHEATHE)||
        (ascendance.radiance==RADIANCE_CHASE&&d->move==W_THRUST)||
        (ascendance.radiance==RADIANCE_FOLLOW&&d->move==W_SMALL)))){
        ascendance.radiance=RADIANCE_OFF;
        radiance_pursuit.target=0;
    }
    unsigned enhanced_hold=d->kind==W_ENTER&&ascendance.sheathe&&!ascendance.radiance&&
        ascendance.ready_frames<BOOST_SHEATHE_WINDOW&&(ascendance.remaining>0||ascendance.counter_sheathe);
    unsigned enhanced=enhanced_hold&&d->move==W_BIG;
    unsigned small=enhanced_hold&&d->move==W_SMALL&&d->source==307&&!d->motion_contract;
    ascendance.enhanced_iai=enhanced||small;
    ascendance.counter_sheathe=d->kind==W_ENTER&&d->move==W_SHEATHE&&d->counter_resheathe;
    ascendance.sheathe=d->kind==W_ENTER&&d->move==W_SHEATHE&&!ascendance.radiance&&
        (ascendance.remaining>0||ascendance.counter_sheathe);
    if(ascendance.counter_sheathe)log_event("ascendance_counter_sheathe",(u32)(uptr)a,0,0,0);
    ascendance.ready_frames=0;
    if(enhanced)log_event("ascendance_iai",(u32)(uptr)a,d->source,160,0);
}

static unsigned ascendance_sheathe_resource(void *a)
{
    const void *res=(const void *)(uptr)word(a,0xf18);
    const void *bank=res?(const void *)(uptr)word(res,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<=ENHANCED_SHEATHE_SLOT)return 0;
    const void *clip=(const void *)(uptr)word(bank,8+4*ENHANCED_SHEATHE_SLOT);
    return clip&&word(clip,8)==w_clip(306)->length;
}

static void ascendance_sheathe_enter(void *a,WDecision *d)
{
    if(d->kind!=W_ENTER||d->move!=W_SHEATHE||d->slot==32||!ascendance.sheathe||!ascendance_sheathe_resource(a))return;
    world.slot=d->slot=ENHANCED_SHEATHE_SLOT;
    log_event("ascendance_sheathe_pose",(u32)(uptr)a,RADIANCE_HOLD_SLOT,world.slot,0);
}

/* A pose rebind carries the normal command and hold clocks. Declining it on
 * a frame that already chose a command keeps R, X and B live at the boundary. */
static void ascendance_sheathe_pose_tick(void *a,const WObservation *o)
{
    if(!ascendance_owner(a)||ascendance.radiance||!world_bound||
       world_bound_actor!=(u32)(uptr)a||!w_iai_ready(&world,o)||
       u16(a,MOTION)!=0x500+world.slot)return;
    unsigned previous=world.slot;
    unsigned rest=previous==32;
    if(rest&&real(a,FRAME)<176)return;
    unsigned hold=previous==MUSOU_HOLD_SLOT,relax=previous==MUSOU_RELAX_SLOT;
    if((hold&&ascendance.sheathe)||(relax&&real(a,FRAME)<116))return;
    if(!rest&&!hold&&!relax&&previous!=RADIANCE_HOLD_SLOT&&previous!=ENHANCED_SHEATHE_SLOT)return;
    unsigned target=ascendance.sheathe?ENHANCED_SHEATHE_SLOT:RADIANCE_HOLD_SLOT;
    unsigned blend=ENHANCED_SHEATHE_BLEND;
    if(rest&&ascendance.sheathe){
        const void *res=(const void *)(uptr)word(a,0xf18);
        const void *bank=res?(const void *)(uptr)word(res,0x64):0;
        if(bank&&u16(bank,6)>MUSOU_HOLD_SLOT){
            const void *h=(const void *)(uptr)word(bank,8+4*MUSOU_HOLD_SLOT);
            const void *r=(const void *)(uptr)word(bank,8+4*MUSOU_RELAX_SLOT);
            if(h&&r&&word(h,8)==225&&word(r,8)==119){target=MUSOU_HOLD_SLOT;blend=0;}
        }
    }else if(hold){target=MUSOU_RELAX_SLOT;blend=6;}
    else if(relax){target=RADIANCE_HOLD_SLOT;blend=3;}
    if(target==previous||(ascendance.sheathe&&!ascendance_sheathe_resource(a)))return;
    float frame=rest||hold||relax?80:real(a,FRAME);
    original_layer(a,0,5000+target,0x81,blend,frame,1);
    if(u16(a,MOTION)!=0x500+target)return;
    world.slot=target;
    if(rest)world.hold_seconds=0;
#if ML_WORLD_INPUT_VM
    world.input_motion=0x500+target;
#endif
    log_event("ascendance_sheathe_pose",(u32)(uptr)a,previous,target,(u32)frame);
}
static unsigned ascendance_radiance_ready(void *a)
{
    return ascendance_owner(a)&&!ascendance.radiance&&
        !musou.phase&&!ascendance.double_cut&&hunter_style(a)==0&&
        ascendance.charge>=BOOST_COST&&
        !(word(a,AIR)&0x400)&&(int16_t)u16(a,HP)>0;
}

static unsigned ascendance_radiance_available(void *a)
{
    return ascendance_radiance_ready(a)&&(byte(a,DRAWN)&1);
}

static void ascendance_spend_reserve(void)
{
    ascendance.charge-=BOOST_COST;
    ascendance.remaining=BOOST_DURATION;
    ascendance.passive_frames=0;
}

static unsigned ascendance_independent_flash(void)
{
    return world.move==W_BIG&&world.slot==RADIANCE_FLASH_SLOT&&
        (ascendance.radiance==RADIANCE_RELEASE||ascendance.radiance==RADIANCE_FLASH);
}

static struct {
    void *actor;
    unsigned area,sample,source,resheathe,roll;
    float origin,drop,age,duration,cut_frame;
} radiance_step;

static unsigned ascendance_chase_travel(void)
{
    return ascendance.radiance==RADIANCE_CHASE&&radiance_pursuit.independent&&world.move==W_THRUST&&
        (world.slot==PURSUIT_RUN_SLOT||world.slot==PURSUIT_BRAKE_SLOT);
}

static unsigned ascendance_spirit_thrust(void)
{
    return world.move==W_THRUST&&world.source==170&&world.slot==225;
}

static unsigned ascendance_wilds_step_cut(void)
{
#if ML_WILDS_MOVES
    if(world.move==W_SLIDE)
        return world.source==903||world.source==910||world.source==911||world.source==912||
               (world.source>=W_SPIRIT_TWO_FORWARD&&world.source<=W_SPIRIT_TWO_BACK);
#endif
    return world.move==W_CHIJIN&&
        (world.source==W_CRIMSON_MOVING||world.source==W_CRIMSON_RETREAT||world.source==906||world.source==907);
}

static unsigned ascendance_grounded_iai(void)
{
    if(ascendance.radiance||musou.phase)return 0;
    return (world.move==W_BIG&&world.source>=308&&world.source<=311)||
        (world.move==W_SMALL&&world.source==307&&!ascendance.double_cut);
}

static unsigned ascendance_grounded_foresight(void)
{
    return world.move==W_FORESIGHT&&world.source==140;
}

static unsigned ascendance_resheathe_slot(unsigned slot)
{
    return slot==ENHANCED_SHEATHE_SLOT||slot==RADIANCE_HOLD_SLOT;
}

static unsigned ascendance_ground_phase(void *a)
{
    return ascendance_owner(a)&&world_bound&&world_bound_actor==(u32)(uptr)a&&
        u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move)&&u16(a,MOTION)==0x500+world.slot&&
        ((ascendance.radiance==RADIANCE_HOLD&&world.move==W_SHEATHE)||ascendance_independent_flash()||
          ascendance_chase_travel()||ascendance_spirit_thrust()||
          ascendance_grounded_iai()||ascendance_grounded_foresight()||ascendance_wilds_step_cut()||
          (ascendance.double_resheathe_step&&world.move==W_SHEATHE&&ascendance_resheathe_slot(world.slot))||
          (world.move==W_SMALL&&(world.slot==DIVIDING_STEP_SLOT||
           (ascendance.double_cut&&ascendance.double_step&&double_draw_motion(0x500+world.slot)))));
}

static void ascendance_step_start(void *a,float origin,float drop)
{
    if(!ascendance_ground_phase(a)||(drop>-20&&drop<20)||
       (radiance_step.actor==a&&radiance_step.area==byte(a,AREA)))return;
    float duration=drop<0?1:8,cut_frame=0,frame=real(a,FRAME);
    if(ascendance_spirit_thrust()||ascendance_grounded_iai()||ascendance_grounded_foresight()){
        cut_frame=w_next_hit_frame(world.source,frame);
        if(cut_frame<0)return;
    }
#if ML_WILDS_MOVES
    if(ascendance_wilds_step_cut()){
        /* Let the native ground solve finish before a strike becomes active. */
        for(unsigned i=0;i<sizeof(wilds_strikes)/sizeof(*wilds_strikes);++i){
            const WildsStrike *strike=wilds_strikes+i;
            if(strike->source!=world.source||frame>=strike->end)continue;
            if(frame>=strike->begin){
                /* The retreat can start crossing an edge during its first
                 * sweep. Finish the short descent before the closing cut. */
                if(world.source!=912||strike->row!=156)return;
                duration=6;
                continue;
            }
            if(!cut_frame||strike->begin<cut_frame)cut_frame=strike->begin;
        }
    }
#endif
    if(cut_frame){
        float speed=real(a,0x50c);
        if(speed<1)speed=1;
        float remaining=(cut_frame-frame)/speed;
        /* Frame 58 can arrive a few thousandths late at the two-frame boundary. */
        if(remaining<2&&(world.source!=912||remaining<1.99f))return;
        if(remaining<duration)duration=remaining;
    }
    radiance_step=(__typeof__(radiance_step)){
        .actor=a,.area=byte(a,AREA),.sample=sample_sequence,.origin=origin,.drop=drop,
        .duration=duration,.cut_frame=cut_frame,.source=world.source,
        .resheathe=drop>0&&ascendance.double_resheathe_step&&world.move==W_SHEATHE&&ascendance_resheathe_slot(world.slot)
    };
}

/* A follow-up can bind after the floor solver has already run on its old pose. */
static unsigned ascendance_resheathe_step_follow(void *a)
{
    unsigned area=byte(a,AREA);
    if(!radiance_step.resheathe||radiance_step.actor!=a||radiance_step.area!=area||
       radiance_step.age>=radiance_step.duration||!ascendance_owner(a)||world.actor!=(u32)(uptr)a||
       world.epoch!=area||u16(a,STATE)!=16)return 0;
    if(ascendance.radiance||musou.phase)return 0;
    unsigned iai=world.move==W_SMALL&&world.source==307&&!ascendance.double_cut;
    unsigned big=world.move==W_BIG&&world.source>=308&&world.source<=311;
    unsigned double_draw=world.move==W_SMALL&&ascendance.double_cut&&double_draw_slot(world.slot);
    if(!iai&&!big&&!double_draw)return 0;
    unsigned motion=u16(a,MOTION);
    if(motion!=0x500+world.slot&&!ascendance_resheathe_slot(motion-0x500))return 0;
    float support=real(a,0x10f0)-(radiance_step.origin-radiance_step.drop);
    return support>=-24&&support<=24;
}

static void ascendance_resheathe_roll_begin(void *a)
{
    if(radiance_step.resheathe&&radiance_step.actor==a&&radiance_step.area==byte(a,AREA)&&
       u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_SHEATHE)&&
       ascendance_resheathe_slot(u16(a,MOTION)-0x500))
        radiance_step.roll=1;
}

static unsigned ascendance_resheathe_roll_active(void *a)
{
    if(!radiance_step.roll||radiance_step.actor!=a||radiance_step.area!=byte(a,AREA)||
       radiance_step.age>=radiance_step.duration)return 0;
    unsigned state=u16(a,STATE),action=u16(a,ACTION);
    if(!w_native_roll(state,action)&&
       !(state==16&&action==w_action(W_SHEATHE)&&ascendance_resheathe_slot(u16(a,MOTION)-0x500)))
        return 0;
    float support=real(a,0x10f0)-(radiance_step.origin-radiance_step.drop);
    return support>=-24&&support<=24;
}

static void ascendance_step_floor(void *a)
{
    if(radiance_step.actor!=a)return;
    if(radiance_step.area!=byte(a,AREA)||(!ascendance_ground_phase(a)&&!ascendance_resheathe_step_follow(a)&&!ascendance_resheathe_roll_active(a))||(word(a,AIR)&0x400)){
        radiance_step.actor=0;return;
    }
    if(radiance_step.sample!=sample_sequence){
        float frames=real(a,0x1c);
        radiance_step.sample=sample_sequence;
        if(frames>0&&frames<=4)radiance_step.age+=frames;
    }
    if(radiance_step.age>=radiance_step.duration||
       (radiance_step.cut_frame&&real(a,FRAME)>=radiance_step.cut_frame)){
        radiance_step.actor=0;return;
    }
    float t=radiance_step.age/radiance_step.duration;
    /* A short window needs an even descent to settle before the next cut. */
    float progress=radiance_step.duration<8?t:t*t;
    float floor=real(a,0x10f0);
    float y=radiance_step.origin-radiance_step.drop*progress;
    if(radiance_step.drop<0){
        float support_delta=floor-(radiance_step.origin-radiance_step.drop);
        if(radiance_step.source!=world.source||support_delta < -12||support_delta>12){
            radiance_step.actor=0;
            return;
        }
        /* Take the larger part immediately to clear the stair lip.
         * Give the remaining lift back over the next simulation frame. */
        y=floor+radiance_step.drop*.40f*(1-t);
    }else if(y<=floor||y<=real(a,0x44))return;
    set_real(a,0x44,y);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}

static unsigned ascendance_flash_resource(void *a)
{
    const void *res=(const void *)(uptr)word(a,0xf18);
    const void *bank=res?(const void *)(uptr)word(res,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<=RADIANCE_FLASH_SLOT)return 0;
    const void *clip=(const void *)(uptr)word(bank,8+4*RADIANCE_FLASH_SLOT);
    return clip&&word(clip,8)==RADIANCE_FLASH_FRAMES;
}

/* Playback speed for the phases the reference drives with its own action-speed
 * curve. The reference writes ActionMotionSpeed every update; the port only
 * wrote it on event frames, so the flash, the enhanced entry and the flurry
 * strokes all played at a constant 1.0. */
static float ascendance_ease(float t)
{
    if(t<0)t=0;
    if(t>1)t=1;
    return t*t*t*(10+t*(-15+6*t));
}

static float ascendance_flash_rate(float frame)
{
    if(frame<29)return 1.15f;
    if(frame<31)return 1.15f+(.26f-1.15f)*ascendance_ease((frame-29)/2);
    if(frame<=35)return .26f;
    if(frame<38)return .26f+(1.6f-.26f)*ascendance_ease((frame-35)/3);
    return 1.6f;
}

/* Linear lookup in a per-frame rate table; the tables are one entry per frame. */
static float ascendance_rate_at(const float *table,unsigned count,float frame)
{
    if(!(frame>=0))return table[0];
    unsigned index=(unsigned)frame;
    if(index+1>=count)return table[count-1];
    float t=frame-index;
    return table[index]+(table[index+1]-table[index])*t;
}

/* A negative rate leaves the motion speed alone; zero holds the current pose. */
static float ascendance_walk_rate(void);
static float ascendance_frame_speed(void *a)
{
    unsigned motion=u16(a,MOTION);
    float frame=real(a,FRAME);
    if(motion==0x500+PURSUIT_FIRST_SLOT)
        return radiance_pursuit.first_hit_hold>0?0:
            ascendance_rate_at(pursuit_first_rates,PURSUIT_FIRST_FRAMES,frame);
    if(motion==0x500+PURSUIT_SECOND_SLOT)
        return ascendance_rate_at(pursuit_second_rates,PURSUIT_SECOND_FRAMES,frame);
    if(ascendance_independent_flash())return ascendance_flash_rate(frame);
    if(ascendance.radiance==RADIANCE_HOLD&&motion==0x500+RADIANCE_WALK_SLOT)return ascendance_walk_rate();
    if(musou.phase==MUSOU_FLURRY&&!musou_spin_slot(world.slot)){
        /* Every flurry cut is baked as 18 frames (15 authored + 3 tail); 1.2
         * plays it in the reference's 15-tick cut interval. */
        const MusouStroke *clip=musou_clip(u16(a,MOTION));
        if(clip&&clip->frames==18)return 1.2f;
    }
    return -1;
}

static void ascendance_flash_floor(void *a)
{
    if(!ascendance_independent_flash()||!ascendance_owner(a)||!world_bound||
       world_bound_actor!=(u32)(uptr)a||u16(a,STATE)!=16||u16(a,ACTION)!=w_action(W_BIG)||
       u16(a,MOTION)!=0x500+RADIANCE_FLASH_SLOT||(word(a,AIR)&0x400))return;
    float f=real(a,FRAME);
    if(!(f>=0))return;
    /* LongSwordBoost 1.1.0, ls_radiance_v38 clip 1, forward root in GU
     * units. The pose and travel share this frame clock through the slow tail. */
    static const float root[]={
        0.0000f,2.5976f,4.8783f,6.8973f,8.7096f,10.1737f,
        11.4331f,12.5901f,13.7470f,15.0892f,16.6162f,18.2897f,
        20.0151f,21.6414f,22.9037f,24.2377f,25.5152f,26.6338f,
        27.5177f,28.1769f,28.8361f,29.4832f,30.0698f,30.5891f,
        31.0564f,31.4646f,31.7510f,32.0265f,32.2929f,32.4606f,
        32.5851f,32.6693f,32.6976f,32.7260f,32.7543f,32.7827f
    };
    unsigned last=sizeof(root)/sizeof(*root)-1;
    if(f>last)f=last;
    unsigned i=(unsigned)f;
    float travel=root[i];
    if(i<last)travel+=(root[i+1]-travel)*(f-i);
    float step=travel-ascendance.flash_travel;
    ascendance.flash_travel=travel;
    if(step<0||step>32)step=0;
    set_real(a,0x40,real(a,0x1000)+step*world_sin(world.heading));
    set_real(a,0x48,real(a,0x1008)+step*world_sin(world.heading+16384));
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}
static unsigned ascendance_flash_attack(const void *attack)
{
    return ascendance.radiance==RADIANCE_FLASH&&world.move==W_BIG&&ascendance_owned_blade(attack);
}

static void ascendance_flash_target(void *a,void *attack,void *target,void *status,
                                    unsigned region,const void *point)
{
    if(!ascendance_owner(a)||!ascendance_flash_attack(attack)||!status||
       radiance_pursuit.confirmed||u16(status,0x3ae))return;
    unsigned uid=byte(status,0x1b5);
    if(world_monster(uid)!=target)return;
    radiance_pursuit=(__typeof__(radiance_pursuit)){
        .target=target,.status=status,.uid=uid,.region=region,
        .dx=real(point,0)-real(target,0x40),.dz=real(point,8)-real(target,0x48)
    };
    log_event("ascendance_flash_target",(u32)(uptr)a,(u32)(uptr)target,region,uid);
}

static unsigned ascendance_reaction_before(void *target)
{
    if(ascendance.radiance!=RADIANCE_FLASH||target!=radiance_pursuit.target||
       radiance_pursuit.confirmed||radiance_pursuit.age>14||
       word(target,0x1428)!=(uptr)radiance_pursuit.status)return 0;
    const void *status=radiance_pursuit.status;
    unsigned pending=u16(status,0x3ae);
    return pending>=3&&pending<=5&&byte(status,0x3b2)==radiance_pursuit.region?pending:0;
}

static void ascendance_reaction_after(void *target,unsigned pending,unsigned before)
{
    unsigned now=u16(target,0x73e0),group=now&255;
    if(pending&&before!=now&&(group==10||group==11)){
        radiance_pursuit.confirmed=1;
        log_event("ascendance_stagger",(u32)(uptr)target,pending,before,now);
    }
}

static unsigned ascendance_pursuit_point(void *a,float *dx,float *dz)
{
    void *target=radiance_pursuit.target;
    if(!target||world_monster(radiance_pursuit.uid)!=target||byte(target,AREA)!=byte(a,AREA)||
       word(target,0x1428)!=(uptr)radiance_pursuit.status||
       (int)word(radiance_pursuit.status,0x370)<=0)return 0;
    float dy=real(target,0x44)-real(a,0x44);
    *dx=real(target,0x40)+radiance_pursuit.dx-real(a,0x40);
    *dz=real(target,0x48)+radiance_pursuit.dz-real(a,0x48);
    return dy>=-65&&dy<=65&&(*dx)*(*dx)+(*dz)*(*dz)<=460.f*460.f;
}

static unsigned ascendance_face(float dx,float dz,unsigned fallback)
{
    float x=dx<0?-dx:dx,z=dz<0?-dz:dz;
    if(x+z<1)return fallback;
    unsigned low=0,high=16384;
    for(unsigned i=0;i<13;++i){
        unsigned yaw=(low+high)/2;
        if(x*world_sin(yaw+16384)>z*world_sin(yaw))low=yaw;
        else high=yaw;
    }
    unsigned yaw=dz<0?32768-high:high;
    return (dx<0?0-yaw:yaw)&65535;
}

static unsigned ascendance_double_resource(void *a,unsigned slot)
{
    const void *res=(const void *)(uptr)word(a,0xf18);
    const void *bank=res?(const void *)(uptr)word(res,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<=slot)return 0;
    const void *clip=(const void *)(uptr)word(bank,8+4*slot);
    return clip&&word(clip,8)==DOUBLE_DRAW_END+3+double_draw_offset(slot);
}

static unsigned ascendance_pursuit_resources(void *a)
{
    const void *res=(const void *)(uptr)word(a,0xf18);
    const void *bank=res?(const void *)(uptr)word(res,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<=PURSUIT_SECOND_SLOT)return 0;
    const unsigned clips[][2]={{PURSUIT_RUN_SLOT,PURSUIT_RUN_FRAMES},{PURSUIT_BRAKE_SLOT,PURSUIT_BRAKE_FRAMES},
        {PURSUIT_FIRST_SLOT,PURSUIT_FIRST_FRAMES},{PURSUIT_SECOND_SLOT,PURSUIT_SECOND_FRAMES}};
    for(unsigned i=0;i<sizeof(clips)/sizeof(*clips);++i){
        const void *clip=(const void *)(uptr)word(bank,8+4*clips[i][0]);
        if(!clip||word(clip,8)!=clips[i][1])return 0;
    }
    return 1;
}

#include "double_draw_step.h"
/* Original direct resheathe timing for the 24-tick GU action budget.
 * Integrate its eased weight over F8..16, then use the fixed 1.4 tail. */
static float double_resheathe_time(float f)
{
    float area=f;
    if(f>=16)area=12;
    else if(f>8){
        float u=(f-8)/8;
        area=8+8*(u-u*u*u+.5f*u*u*u*u);
    }
    return (5*f+4*area)/7;
}
static float double_resheathe_speed(float frame,float ticks)
{
    if(frame>=16)return 1.4f;
    if(!(ticks>0))return 0;
    float t=double_resheathe_time(frame)+ticks;
    float next;
    if(t<=double_resheathe_time(8))next=t*7/9;
    else if(t>=double_resheathe_time(16))next=(7*t-48)/5;
    else{
        float lo=8,hi=16;
        for(unsigned i=0;i<21;++i){
            float mid=(lo+hi)*.5f;
            if(double_resheathe_time(mid)<t)lo=mid;else hi=mid;
        }
        next=(hi+lo)*.5f;
    }
    return (next-frame)/ticks;
}
static void double_draw_position(float frame,float *x,float *z)
{
    const float (*root)[2]=ascendance.double_step?double_draw_step_root:double_draw_standing_root;
    unsigned end=ascendance.double_step?64:39;
    if(world.slot==DIVIDING_STEP_SLOT){root=dividing_step_root;end=DIVIDING_STEP_FRAMES-1;}
    if(world.slot==DOUBLE_DRAW_RETURN){root=double_draw_resheathe_root;end=24;}
    if(frame>end)frame=end;
    unsigned n=(unsigned)frame,next=n<end?n+1:n;
    float t=frame-n;
    const float *a=root[n],*b=root[next];
    *x=a[0]+(b[0]-a[0])*t;*z=a[1]+(b[1]-a[1])*t;
}

static float ascendance_pursuit_floor(void *a,unsigned before_wall)
{
    unsigned dividing=world.move==W_SMALL&&world.slot==DIVIDING_STEP_SLOT;
    unsigned step_double=world.move==W_SMALL&&ascendance.double_cut&&ascendance.double_step&&
        double_draw_motion(0x500+world.slot);
    unsigned chase=ascendance_chase_travel();
    unsigned wall_root=dividing||step_double||chase;
    if(wall_root!=before_wall)return 0;
    if((!dividing&&!radiance_pursuit.independent&&!ascendance.double_cut)||!ascendance_owner(a)||!world_bound||
       world_bound_actor!=(u32)(uptr)a||u16(a,STATE)!=16||
       u16(a,ACTION)!=w_action(world.move)||u16(a,MOTION)!=0x500+world.slot||
       (!dividing&&!radiance_pursuit_pose(u16(a,MOTION))&&!double_draw_motion(u16(a,MOTION)))||(word(a,AIR)&0x400))return 0;
    if(chase&&radiance_pursuit.travel_sample==sample_sequence)return 0;
    float step=0,side=0;
    if(dividing||ascendance.double_cut){
        if(dividing||double_draw_motion(u16(a,MOTION))){
            float *last=dividing?&ascendance.dividing_travel_frame:&ascendance.double_travel_frame;
            float frame=real(a,FRAME);
            /* Submit authored step travel once, before the wall resolver.
             * A second check on the same frame already includes contact push. */
            if(wall_root&&(!(frame>=0)||frame==*last))return 0;
            if(frame>=*last){
                float previous_x,previous_z,x,z;
                double_draw_position(*last<0?0:*last,&previous_x,&previous_z);
                double_draw_position(frame,&x,&z);
                side=x-previous_x;step=z-previous_z;
                *last=frame;
            }
        }
    }else if(radiance_pursuit.travel_sample!=sample_sequence){
        radiance_pursuit.travel_sample=sample_sequence;
        float frames=real(a,0x1c);
        if(frames>0&&frames<=4)step=frames*radiance_pursuit.speed;
    }
    float sine=world_sin(world.heading),cosine=world_sin(world.heading+16384);
    set_real(a,0x40,real(a,0x1000)+step*sine+side*cosine);
    set_real(a,0x48,real(a,0x1008)+step*cosine-side*sine);
    put(a,HEADING,world.heading);put(a,TARGET,world.heading);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
    return chase?step:0;
}

static WDecision ascendance_pursuit_motion(unsigned slot,float frame,float blend)
{
    world.slot=slot;
    return (WDecision){.kind=W_MOTION,.move=world.move,.source=world.source,.slot=slot,.heading=world.heading,
        .motion_contract=1,.start=frame,.blend=blend,.speed=1};
}

static WDecision ascendance_pursuit_run(void *a,WObservation *o,unsigned valid,float dx,float dz)
{
    float frame=o->base.animation_frame;
    if(!o->grounded){
        WDecision d=w_leave(&world);d.kind=W_ABORT;return d;
    }
    if(o->base.motion!=0x500+world.slot)return (WDecision){0};
    float frames=o->base.simulation_delta,dist=att_sqrt(dx*dx+dz*dz);
    if(!(frames>0&&frames<=4))frames=0;
    unsigned timed=radiance_pursuit.age>=(radiance_pursuit.buffered?88:62);
    if(!radiance_pursuit.brake_cancel&&radiance_pursuit.wall_stall_frames>=6){
        radiance_pursuit.brake_cancel=1;
        log_event("ascendance_chase_wall",(u32)(uptr)a,(u32)frame,(u32)radiance_pursuit.age,0);
    }
    if(!valid||timed)radiance_pursuit.brake_cancel=1;
    if(valid&&!radiance_pursuit.brake_cancel){
        unsigned heading=ascendance_face(dx,dz,world.heading);
        if(o->base.magnitude>=50){
            int steer=ml_heading_delta(o->base.input_heading,heading)/3;
            if(steer>4096)steer=4096;
            if(steer< -4096)steer= -4096;
            heading=(heading+steer)&65535u;
        }
        int turn=ml_heading_delta(heading,world.heading),limit=(int)(1880*frames);
        if(turn>limit)turn=limit;
        if(turn< -limit)turn= -limit;
        world.heading=(world.heading+turn)&65535;
        o->base.heading=world.heading;
        put(a,HEADING,world.heading);put(a,TARGET,world.heading);
    }
    unsigned braking=world.slot==PURSUIT_BRAKE_SLOT;
    if(!braking&&(radiance_pursuit.brake_cancel||dist<=(radiance_pursuit.buffered?209:185))){
        log_event("ascendance_chase_brake",(u32)(uptr)a,(u32)dist,
            (u32)(radiance_pursuit.speed*100),radiance_pursuit.brake_cancel);
        return radiance_pursuit.brake_cancel?ascendance_pursuit_motion(PURSUIT_BRAKE_SLOT,0,5):ascendance_pursuit_motion(PURSUIT_BRAKE_SLOT,PURSUIT_ARRIVAL_START,8);
    }
    if(braking){
        /* 6.5 m/s (10.83 units per frame) over 78 cm, the authored brake clip's travel. */
        radiance_pursuit.speed-=.752f*frames;
        if(radiance_pursuit.speed<0)radiance_pursuit.speed=0;
        if(pursuit_brake_elapsed(frame)>=12&&radiance_pursuit.speed<=.1f){
            if(radiance_pursuit.brake_cancel){
                WDecision d=w_leave(&world);d.kind=W_IDLE;
                log_event("ascendance_chase_end",(u32)(uptr)a,valid,(u32)frame,(u32)radiance_pursuit.age);
                return d;
            }
            if(valid&&radiance_pursuit.buffered&&dist<=205){
                WDecision d=w_radiance_enter(&world,o,W_SMALL);
                if(d.kind==W_ENTER){
                    ascendance.radiance=RADIANCE_FOLLOW;
                    if(o->has_focus_heading)world.heading=d.heading=o->focus_camera_heading;
                    world.slot=d.slot=PURSUIT_FIRST_SLOT;
                    d.motion_contract=1;d.blend=5;d.speed=1;
                    log_event("ascendance_follow",(u32)(uptr)a,(u32)(uptr)radiance_pursuit.target,d.heading,0);
                }
                return d;
            }
            if(dist>200)return ascendance_pursuit_motion(PURSUIT_RUN_SLOT,0,5);
        }
    }else{
        /* Authored gait: the pursuit run loop's root motion is 6.5 m/s, and the
         * reference ramps to chase_speed 7.2 m/s beyond 3 m of target distance.
         * Speed is GU units per frame, so 6.5 m/s = 10.83 and 7.2 m/s = 12. */
        float top=dist>=300?12.f:dist<=185?10.83f:10.83f+(dist-185)*.010174f;
        radiance_pursuit.speed+=.75f*frames;
        if(radiance_pursuit.speed>top)radiance_pursuit.speed=top;
        if(frame>=PURSUIT_LOOP_START+PURSUIT_LOOP_FRAMES||o->base.motion_finished)
            return ascendance_pursuit_motion(PURSUIT_RUN_SLOT,PURSUIT_LOOP_START,0);
    }
    return (WDecision){0};
}

static float ascendance_pursuit_second_start(float hit_frame)
{
    /* The baked frames map first F10..18 to source F28..40 and second
     * F0..5 to source F34..40. First F14 is the earliest shared pose. */
    if(!(hit_frame>14.f))return 0;
    float start=(hit_frame-14.f)*1.25f;
    /* Keep the second cut's complete F5..13 damage window available. */
    return start<4.75f?start:4.75f;
}

static WDecision ascendance_pursuit_cuts(void *a,WObservation *o)
{
    if(o->state!=16||o->base.native_action!=w_action(W_SMALL)||o->base.motion!=0x500+world.slot)
        return (WDecision){0};
    unsigned first=world.slot==PURSUIT_FIRST_SLOT;
    float frame=o->base.animation_frame;
    if(first&&radiance_pursuit.rewarded){
        if(radiance_pursuit.first_hit_sample!=o->base.sample_sequence){
            radiance_pursuit.first_hit_sample=o->base.sample_sequence;
            float delta=o->base.simulation_delta;
            if(delta>0&&delta<30)radiance_pursuit.first_hit_hold-=delta;
        }
        if(radiance_pursuit.first_hit_hold>0){
            if(radiance_pursuit.first_hit_pose_pending){
                /* The native clock may already have advanced. Bind only
                 * this layer, at the recorded contact pose and zero speed. */
                radiance_pursuit.first_hit_pose_pending=0;
                original_layer(a,0,5000+PURSUIT_FIRST_SLOT,0,0,
                               radiance_pursuit.first_hit_frame,0);
            }
            o->base.held=0;o->contact=0;
            return (WDecision){0};
        }
        radiance_pursuit.first_hit_hold=0;
        float start=ascendance_pursuit_second_start(radiance_pursuit.first_hit_frame);
        log_event("ascendance_follow_second",(u32)(uptr)a,(u32)(uptr)radiance_pursuit.target,(u32)frame,0);
        log_event("ascendance_follow_join",(u32)(uptr)a,(u32)(radiance_pursuit.first_hit_frame*1000.f),(u32)(start*1000.f),0);
        pursuit_cut_bridge_begin(a);
        return ascendance_pursuit_motion(PURSUIT_SECOND_SLOT,start,5);
    }
    unsigned end=first?PURSUIT_FIRST_FRAMES-3:PURSUIT_SECOND_FRAMES-3;
    /* Radiance guards neutral movement until an unhit follow finishes. */
    if(first&&o->base.held)
        end=PURSUIT_FIRST_BRANCH;
    if(frame>=end||o->base.motion_finished){
        if(!first&&o->grounded)pursuit_settle_begin(a);
        WDecision d=w_leave(&world);d.kind=W_IDLE;
        log_event("ascendance_follow_end",(u32)(uptr)a,first,radiance_pursuit.rewarded,(u32)frame);
        return d;
    }
    o->base.held=0;o->contact=0;
    return (WDecision){0};
}
enum { FLASH_MUSOU_BRANCH=29, FLASH_MUSOU_BUFFER=27 };
static WDecision ascendance_musou_start(void *a,const WObservation *o);

static unsigned ascendance_flash_link(void *a,const WObservation *o)
{
    return ascendance_owner(a)&&ascendance.remaining>0&&ascendance_independent_flash()&&
        ascendance.radiance==RADIANCE_FLASH&&
        world_bound&&world_bound_actor==(u32)(uptr)a&&o->grounded&&o->alive&&o->drawn&&
        o->resources_ready&&o->state==16&&o->base.native_action==w_action(W_BIG)&&
        o->base.motion==0x500+RADIANCE_FLASH_SLOT&&!o->base.motion_finished&&
        o->base.animation_frame<RADIANCE_FLASH_FRAMES-3;
}

static unsigned ascendance_flash_chase_target(void *a,float *dx,float *dz)
{
    return radiance_pursuit.confirmed&&world.counter_success&&ascendance_pursuit_point(a,dx,dz);
}

static WDecision ascendance_pursuit_command(void *a,WObservation *o,unsigned edge)
{
    float delta=o->base.simulation_delta,dx=0,dz=0,frame=o->base.animation_frame;
    if(radiance_pursuit.target&&radiance_pursuit.sample!=o->base.sample_sequence){
        radiance_pursuit.sample=o->base.sample_sequence;
        if(delta>0&&delta<30)radiance_pursuit.age+=delta;
    }
    if(ascendance.radiance==RADIANCE_FLASH&&world.move==W_BIG&&o->grounded&&
       o->state==16&&o->base.native_action==w_action(W_BIG)&&o->base.motion==0x500+world.slot&&
       ascendance_flash_chase_target(a,&dx,&dz)){
        /* A confirmed reaction authorizes the chase once. Subsequent updates
         * track that target even after its native stagger animation ends. */
        WDecision d=w_radiance_pursuit_enter(&world,o);
        if(d.kind==W_ENTER){
            ascendance.radiance=RADIANCE_CHASE;
            radiance_pursuit.independent=ascendance_pursuit_resources(a);
            if(radiance_pursuit.independent){
                world.slot=d.slot=PURSUIT_RUN_SLOT;
                d.motion_contract=1;d.blend=7;d.speed=1;
            }
            radiance_pursuit.age=0;
            /* Start every chase from rest so a cancelled one cannot leak speed. */
            radiance_pursuit.speed=0;
            radiance_pursuit.buffered=((edge&PAD_X)&&o->base.held==ML_X)||((edge&PAD_R)&&o->base.held==ML_R);
            world.heading=d.heading=ascendance_face(dx,dz,o->base.heading);
            log_event("ascendance_chase",(u32)(uptr)a,(u32)(uptr)radiance_pursuit.target,d.heading,0);
            log_event("ascendance_chase_dispatch",(u32)(uptr)a,(u32)(frame*1000.f),d.rank_delta,d.gauge_fill);
        }
        return d;
    }
    if(ascendance_independent_flash()&&o->state==16&&o->base.native_action==w_action(W_BIG)&&
       o->base.motion==0x500+RADIANCE_FLASH_SLOT){
        if(ascendance_flash_link(a,o)){
            if(o->base.held&~ML_R)ascendance.flash_r_buffered=0;
            else if((edge&PAD_R)&&o->base.held==ML_R&&frame>=FLASH_MUSOU_BRANCH-FLASH_MUSOU_BUFFER)
                ascendance.flash_r_buffered=1;
            if(ascendance.flash_r_buffered&&frame>=FLASH_MUSOU_BRANCH&&
               (!world.counter_success||world.rank_paid)){
                WDecision d=ascendance_musou_start(a,o);
                if(d.kind==W_ENTER)
                    log_event("ascendance_musou_flash",(u32)(uptr)a,(u32)frame,ascendance.radiance_hit,d.slot);
                return d;
            }
        }
        if(frame>=RADIANCE_FLASH_FRAMES||o->base.motion_finished){
            WDecision d=w_leave(&world);
            d.kind=W_IDLE;
            log_event("ascendance_flash_end",(u32)(uptr)a,(u32)frame,word(a,0x40),word(a,0x48));
            return d;
        }
        o->base.held=0;
        o->base.magnitude=0;
    }
    if(ascendance.radiance!=RADIANCE_CHASE||world.move!=W_THRUST)return (WDecision){0};
    if(!o->alive||!o->resources_ready||!o->drawn||o->state!=16||
       o->base.native_action!=w_action(W_THRUST))return (WDecision){0};
    if(((edge&PAD_X)&&o->base.held==ML_X)||((edge&PAD_R)&&o->base.held==ML_R))radiance_pursuit.buffered=1;
    unsigned valid=o->grounded&&ascendance_pursuit_point(a,&dx,&dz);
    if(radiance_pursuit.independent){
        WDecision d=ascendance_pursuit_run(a,o,valid,dx,dz);
        o->base.held=0;o->base.magnitude=0;
        return d;
    }
    if(o->base.motion==0x500+world.slot&&valid){
        unsigned heading=ascendance_face(dx,dz,world.heading);
        put(a,HEADING,heading);put(a,TARGET,heading);
        o->base.heading=world.heading=heading;
        float distance=dx*dx+dz*dz;
        if(frame>=10&&radiance_pursuit.buffered&&distance<=240.f*240.f){
            WDecision d=w_radiance_enter(&world,o,W_SMALL);
            if(d.kind==W_ENTER){
                ascendance.radiance=RADIANCE_FOLLOW;
                log_event("ascendance_follow",(u32)(uptr)a,(u32)(uptr)radiance_pursuit.target,heading,0);
            }
            return d;
        }
        radiance_pursuit.near=frame>=10&&distance<=145.f*145.f;
        /* Ground collision retains ownership of the actual displacement. */
        float speed=radiance_pursuit.near?0:3.8f;
        set_real(a,0x1010,speed*world_sin(heading));
        set_real(a,0x1018,speed*world_sin(heading+16384));
        ascendance_velocity_actor=a;
    }
    if(!valid||frame>=76||radiance_pursuit.age>=88||(radiance_pursuit.near&&radiance_pursuit.age>=54)){
        WDecision d=w_leave(&world);d.kind=W_IDLE;
        log_event("ascendance_chase_end",(u32)(uptr)a,valid,(u32)frame,(u32)radiance_pursuit.age);
        return d;
    }
    o->base.held=0;
    return (WDecision){0};
}

static WDecision ascendance_musou_motion(const WObservation *o,unsigned phase,unsigned move)
{
    unsigned same=world.move==move;
    WDecision d=w_radiance_enter(&world,o,move);
    if(d.kind!=W_ENTER)return d;
    musou.phase=phase;
    musou.wall_root_frame=-1;
    if(same)d.kind=W_MOTION;
    d.motion_contract=1;d.blend=5;d.start=0;d.speed=1;
    if(phase==MUSOU_FLURRY&&musou.stroke==0){
        ++musou.cycles;
        musou.hit_seen=0;
    }
    if(phase==MUSOU_FLURRY&&musou.stroke){
        unsigned index=musou.stroke-1;
        unsigned extra=index>=MUSOU_STROKE_COUNT;
        unsigned slot=extra?0:musou_strokes[musou_chain[index].clip].slot;
        if(musou.opening_path){
            if(index==0)slot=musou_strokes[musou.opening_path].slot;
            if(index==1){
                unsigned first=musou.opening_path-MUSOU_PATH_FIRST;
                /* Four of eight groups include a horizontal stroke.
                 * The original draws from all groups 35% of the time. */
                unsigned draw=(o->base.sample_sequence*2654435761u)>>8;
                unsigned horizontal=draw%40<7;
                musou.second_path=2*(first&1)+((draw/40)&1)+(horizontal?4:0);
                static const unsigned char second_slots[]={57,58,59,60,64,65,66,67};
                slot=second_slots[musou.second_path];
            }
            if(index==2){
                static const unsigned char third_slots[]={62,59,63,57,68,69,68,69};
                slot=third_slots[musou.second_path];
            }
            if(index==3){
                static const unsigned char spin_slots[]={71,71,70,70,71,70,71,70};
                slot=spin_slots[musou.second_path];
            }
            if(index>=4&&index<=7){
                /* Legal level-two triples are A-H-B and B-J-A. */
                if(index==4)musou.group_two_side=(o->base.sample_sequence>>2)&1;
                static const unsigned char slots[2][4]={{46,48,68,71},{47,49,69,70}};
                slot=slots[musou.group_two_side][index-4];
            }
            if(index>=8){
                unsigned step=extra?(index-MUSOU_STROKE_COUNT)%4:(index-8)%5;
                unsigned follows_rapid=extra&&index>MUSOU_STROKE_COUNT;
                if(step==0){
                    unsigned draw=(o->base.sample_sequence*2654435761u)>>8;
                    unsigned pool=draw%60;
                    /* A group starting with A ends with B and vice versa.
                     * Across a spinless join, keep the previous group's side. */
                    unsigned side=follows_rapid?musou.late_group/3:((draw/60)&1);
                    musou.late_group=3*side+(pool<46?0:pool<53?1:2);
                }
                /* ACDB, ACHB, AHDB, BDCA, BDJA, BJCA. Only levels 3/4 use column 5. */
                static const unsigned char slots[6][5]={
                    {46,44,58,59,71},{46,44,64,68,71},{46,48,42,59,71},
                    {47,45,60,57,70},{47,45,67,69,70},{47,49,43,57,70}
                };
                slot=slots[musou.late_group][step];
                if(follows_rapid&&step==0)slot=musou.late_group<3?63:62;
            }
        }
        world.slot=d.slot=slot;
        musou.cycles=extra?5:musou_chain[index].group;
        musou.hit_seen=0;
        musou.travel_frame=0;d.speed=MUSOU_STROKE_RATE;
        if(musou_path_slot(d.slot)){d.speed=1;d.blend=0;}
        if(musou_spin_slot(d.slot)){d.speed=.66f;d.blend=0;musou.spin_x=0;musou.spin_z=0;}
        log_event("ascendance_musou_stroke",(u32)(uptr)ascendance.actor,musou.cycles,index+1,d.slot);
    }
    log_event("ascendance_musou",(u32)(uptr)ascendance.actor,phase,musou.cycles,musou.hits);
    return d;
}

static WDecision ascendance_musou_dash(void *a,const WObservation *o)
{
    unsigned yaw=o->base.magnitude>=50?o->base.input_heading:o->base.heading;
    unsigned outgoing=0;
    if(musou.opening_path){
        /* Select the authored join by the stroke that just ended. */
        switch(world.slot){
        case 46:case 57:case 63:case 69:outgoing=33;break; /* A */
        case 47:case 59:case 62:case 68:outgoing=34;break; /* B */
        case 43:case 44:case 53:case 55:case 60:outgoing=35;break; /* C */
        case 42:case 45:case 54:case 56:case 58:outgoing=36;break; /* D */
        case 70:case 71:outgoing=37;break;
        case 48:case 64:case 66:outgoing=38;break; /* H */
        case 49:case 65:case 67:outgoing=39;break; /* J */
        }
    }
    WDecision d=ascendance_musou_motion(o,MUSOU_DASH,W_BIG);
    if(d.kind!=W_ENTER)return d;
    musou.start_x=real(a,0x40);musou.start_z=real(a,0x48);
    musou.dash_heading=yaw;musou.travel_frame=12;
    if(outgoing){
        const void *resource=(const void *)(uptr)word(a,0xf18);
        const void *bank=resource?(const void *)(uptr)word(resource,0x64):0;
        const void *desc=bank&&outgoing<u16(bank,6)?(const void *)(uptr)word(bank,8+4*outgoing):0;
        if(desc&&word(desc,8)==35){
            world.slot=d.slot=outgoing;d.blend=0;d.speed=1;musou.travel_frame=0;
        }
    }
    world.heading=d.heading=yaw;
    log_event("ascendance_dash_out",(u32)(uptr)a,word(a,0x40),word(a,0x48),yaw);
    return d;
}

static WDecision ascendance_musou_return(void *a,const WObservation *o)
{
    unsigned original_out=world.slot>=33&&world.slot<=39;
    WDecision d=ascendance_musou_motion(o,MUSOU_CUT,W_SMALL);
    if(d.kind!=W_ENTER)return d;
    float x=real(a,0x40),z=real(a,0x48);
    musou.return_heading=ascendance_face(musou.start_x-x,musou.start_z-z,
        (musou.dash_heading+32768)&65535);
    d.blend=9;musou.travel_frame=5;
    world.heading=d.heading=musou.return_heading;
    if(original_out){
        const void *resource=(const void *)(uptr)word(a,0xf18);
        const void *bank=resource?(const void *)(uptr)word(resource,0x64):0;
        const void *desc=bank&&40<u16(bank,6)?(const void *)(uptr)word(bank,8+4*40):0;
        if(desc&&word(desc,8)==35){
            world.slot=d.slot=40;d.blend=6;d.speed=1;
            musou.travel_frame=0;
            musou.return_dx=musou.start_x-x;musou.return_dz=musou.start_z-z;
            world.heading=d.heading=musou.dash_heading;
        }
    }
    log_event("ascendance_dash_return",(u32)(uptr)a,word(a,0x40),word(a,0x48),d.heading);
    return d;
}

static unsigned ascendance_dash_floor(void *a,unsigned before_wall)
{
    unsigned finishing=musou.phase==MUSOU_DASH||musou.phase==MUSOU_CUT||musou.phase==MUSOU_SEAT;
    unsigned spinning=musou.phase==MUSOU_FLURRY&&musou_spin_slot(world.slot);
    unsigned wall_root=finishing||spinning;
    if(wall_root!=before_wall||!musou.phase||!ascendance_owner(a)||
       !world_bound||world_bound_actor!=(u32)(uptr)a||u16(a,STATE)!=16||
       u16(a,ACTION)!=w_action(world.move)||u16(a,MOTION)!=0x500+world.slot||
       (word(a,AIR)&0x400))return 0;
    /* The borrowed Iai clips already submit root travel. Replace that
     * proposal for these two actions only, before native ground resolution.
     * GU stores the settled position from the previous update at 0x1000. */
    enum { POS_X=0x40,POS_Z=0x48,PREV_X=0x1000,PREV_Z=0x1008 };
    float x=real(a,PREV_X),z=real(a,PREV_Z);
    float frame=real(a,FRAME);
    /* The pair solver can re-enter the wall check on the same animation frame.
     * Leave its modified position intact after submitting the root step. */
    if(wall_root){
        if(!(frame>=0)||musou.wall_root_frame==frame)return 0;
        musou.wall_root_frame=frame;
    }
    if(musou.phase==MUSOU_SEAT){
        float t=frame<28?frame/28:1;t=t*t*(3-2*t);
        int delta=(int16_t)(musou.dash_heading-musou.return_heading);
        world.heading=(musou.return_heading+(int)(delta*t))&65535;
        put(a,HEADING,world.heading);put(a,TARGET,world.heading);
        set_real(a,POS_X,x);set_real(a,POS_Z,z);return 1;
    }
    if(spinning){
        if(frame>25)frame=25;
        unsigned n=(unsigned)frame;
        float along=musou_spin_travel[n];
        if(n<25)along+=(musou_spin_travel[n+1]-along)*(frame-n);
        float advance=along-musou.travel_frame;
        musou.travel_frame=along;
        set_real(a,POS_X,x+advance*musou.spin_x);
        set_real(a,POS_Z,z+advance*musou.spin_z);
        return 1;
    }
    if(musou.phase==MUSOU_ENTRY||(musou.phase==MUSOU_FLURRY&&musou_path_slot(world.slot))){
        set_real(a,POS_X,x);set_real(a,POS_Z,z);return 1;
    }
    if(musou.phase==MUSOU_FLURRY){
        const MusouStroke *clip=musou_clip(0x500+world.slot);
        if(!clip||!(frame>=0))return 0;
        unsigned offset=0;
        for(const MusouStroke *it=musou_strokes;it<clip;++it)offset+=it->frames;
        unsigned current=(unsigned)frame,previous=(unsigned)musou.travel_frame;
        if(current>=clip->frames)current=clip->frames-1;
        musou.travel_frame=(float)current;
        float dx=0,dz=0;
        if(current>previous&&current-previous<=4){
            dx=musou_root_steps[offset+current][0]-musou_root_steps[offset+previous][0];
            dz=musou_root_steps[offset+current][1]-musou_root_steps[offset+previous][1];
        }
        float sine=world_sin(world.heading),cosine=world_sin(world.heading+16384);
        set_real(a,POS_X,x+dx*cosine+dz*sine);
        set_real(a,POS_Z,z+dz*cosine-dx*sine);
        return 1;
    }
    unsigned returning=musou.phase==MUSOU_CUT&&world.slot==40;
    if(returning||(musou.phase==MUSOU_DASH&&world.slot>=33&&world.slot<=39)){
        if(!(frame>=0))return 0;
        if(frame>32)frame=32;
        unsigned n=(unsigned)frame;
        float progress=musou_out_progress[n];
        if(n<32)progress+=(musou_out_progress[n+1]-progress)*(frame-n);
        float fraction=progress-musou.travel_frame;
        float step=MUSOU_DASH_UNITS*fraction;
        musou.travel_frame=progress;
        if(returning){
            float t=frame<14?frame/14:1;
            t=t*t*(3-2*t);
            int delta=(int16_t)(musou.return_heading-musou.dash_heading);
            world.heading=(musou.dash_heading+(int)(delta*t))&65535;
            put(a,HEADING,world.heading);put(a,TARGET,world.heading);
            set_real(a,POS_X,x+musou.return_dx*fraction);
            set_real(a,POS_Z,z+musou.return_dz*fraction);
            return 1;
        }
        set_real(a,POS_X,x+step*world_sin(musou.dash_heading));
        set_real(a,POS_Z,z+step*world_sin(musou.dash_heading+16384));
        return 1;
    }
    float end=musou.phase==MUSOU_DASH?36:35;
    if(frame>end)frame=end;
    float advance=frame-musou.travel_frame;
    float step=0;
    if(advance>0){
        musou.travel_frame=frame;
        if(advance<=4)step=8*advance;
    }
    unsigned yaw=musou.dash_heading;
    float remaining=(float)MUSOU_DASH_UNITS;
    if(musou.phase==MUSOU_CUT){
        float dx=musou.start_x-x,dz=musou.start_z-z;
        yaw=ascendance_face(dx,dz,musou.return_heading);
        remaining=dx*world_sin(yaw)+dz*world_sin(yaw+16384);
    }else{
        remaining-=(x-musou.start_x)*world_sin(yaw)+(z-musou.start_z)*world_sin(yaw+16384);
    }
    if(remaining<step)step=remaining>0?remaining:0;
    set_real(a,POS_X,x+step*world_sin(yaw));
    set_real(a,POS_Z,z+step*world_sin(yaw+16384));
    return 1;
}

static unsigned ascendance_musou_resources(void *a)
{
    const void *resource=(const void *)(uptr)word(a,0xf04+5*4);
    const void *bank=resource?(const void *)(uptr)word(resource,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c)return 0;
    for(unsigned i=0;i<sizeof(musou_strokes)/sizeof(*musou_strokes);++i){
        const MusouStroke *clip=musou_strokes+i;
        if(u16(bank,6)<=clip->slot)return 0;
        const void *desc=(const void *)(uptr)word(bank,8+4*clip->slot);
        if(!desc||word(desc,8)!=clip->frames)return 0;
    }
    return 1;
}
static WDecision ascendance_musou_start(void *a,const WObservation *o)
{

    musou=(__typeof__(musou)){0};
    ascendance.radiance=RADIANCE_OFF;
    radiance_pursuit.target=0;
    musou.stroke=ascendance_musou_resources(a);
    if(musou.stroke){
        unsigned slot=(o->base.sample_sequence&1)?MUSOU_ENTRY_LEFT:MUSOU_ENTRY_RIGHT;
        const void *res=(const void *)(uptr)word(a,0xf18);
        const void *bank=res?(const void *)(uptr)word(res,0x64):0;
        const void *desc=bank&&slot<u16(bank,6)?(const void *)(uptr)word(bank,8+4*slot):0;
        if(desc&&word(desc,8)==MUSOU_ENTRY_SAMPLES){
            WDecision d=ascendance_musou_motion(o,MUSOU_ENTRY,W_SMALL);
            if(d.kind==W_ENTER||d.kind==W_MOTION){
                world.slot=d.slot=slot;d.blend=3;
                musou.opening_path=MUSOU_PATH_FIRST+2*(slot-MUSOU_ENTRY_LEFT)+((o->base.sample_sequence>>1)&1);
                log_event("ascendance_musou_entry",(u32)(uptr)a,slot,MUSOU_ENTRY_END,0);
            }
            return d;
        }
    }
    return ascendance_musou_motion(o,MUSOU_FLURRY,W_SMALL);
}

#if ML_WILDS_MOVES
static WDecision ascendance_red_musou_command(void *a,const WObservation *o,unsigned edge)
{
    if(!ascendance_red_spin(a)||ascendance.remaining<=0||o->rank!=3||!o->alive||
       !o->drawn||!o->grounded||!o->resources_ready)return (WDecision){0};
    float frame=o->base.animation_frame;
    const WildsClip *clip=wilds_clip(W_CRIMSON_SPIN);
    if(!clip||!(frame>=0&&frame<clip->recovery)||o->base.motion_finished)return (WDecision){0};
    if(o->base.held&~ML_R)red_musou.buffered=0;
    else if((edge&PAD_R)&&o->base.held==ML_R&&frame>=MUSOU_RED_BUFFER_FRAME)
        red_musou.buffered=1;
    if(!red_musou.hit||!red_musou.buffered||frame<MUSOU_RED_CANCEL_FRAME||!world.rank_paid)
        return (WDecision){0};
    WDecision d=ascendance_musou_start(a,o);
    if(d.kind==W_ENTER){
        if(!musou_entry_slot(d.slot))d.blend=7;
        log_event("ascendance_musou_red",(u32)(uptr)a,(u32)frame,o->base.held,d.slot);
    }
    return d;
}
#endif
static WDecision ascendance_double_start(void *a,const WObservation *o,unsigned enhanced)
{
    /* The source accepts a camera-relative direction strictly above 0.2. */
    unsigned step=o->base.magnitude*5>128;
    unsigned slot=DOUBLE_DRAW_BASE+enhanced+2*step;
    if(!ascendance_double_resource(a,slot))return (WDecision){0};
    WDecision d=w_radiance_enter(&world,o,W_SMALL);
    if(d.kind==W_ENTER){
        ascendance.double_cut=1;ascendance.double_credited=0;
        ascendance.double_travel_frame=step?-1:0;ascendance.double_step=step;
        if(step)world.heading=d.heading=o->base.magnitude>=50?o->base.input_heading:stick_world_heading(a);
        world.slot=d.slot=slot;
        d.motion_contract=1;d.start=0;d.speed=1;d.blend=3;
        log_event("ascendance_double_start",(u32)(uptr)a,enhanced,step,d.heading);
    }
    return d;
}

static WDecision ascendance_musou_command(void *a,WObservation *o,unsigned raw,unsigned edge)
{
    if(!o->alive||!o->drawn||!o->resources_ready||o->state!=16||
       o->base.native_action!=w_action(world.move))return (WDecision){0};
    unsigned bound=o->base.motion==0x500+world.slot;
    float frame=o->base.animation_frame;
    if(!musou.phase){
        if(ascendance.remaining<=0||ascendance.radiance!=RADIANCE_FOLLOW||!radiance_pursuit.rewarded||
           world.move!=W_SMALL||!bound||!o->grounded||
           (radiance_pursuit.independent?(world.slot!=PURSUIT_SECOND_SLOT||frame<20):(frame<37||frame>=70))||
           o->base.held!=ML_R)return (WDecision){0};
        unsigned from_second=radiance_pursuit.independent;
        WDecision d=ascendance_musou_start(a,o);
        if(from_second&&(d.kind==W_ENTER||d.kind==W_MOTION)&&musou_entry_slot(d.slot))
            pursuit_musou_bridge_begin(a);
        return d;
    }
    if(!o->grounded){
        WDecision d=w_leave(&world);d.kind=W_ABORT;
        return d;
    }
    if(musou.phase==MUSOU_SEAT&&bound){
        ascendance_musou_burst(a,frame);
        /* Original insertion marker is 58. Its input grace is .12 seconds. */
        const float grace=.12f*60;
        if(musou.seat_input_sample!=o->base.sample_sequence){
            musou.seat_input_sample=o->base.sample_sequence;
            if(o->base.simulation_delta>0)musou.seat_input_buffer-=o->base.simulation_delta;
            if(musou.seat_input_buffer<0)musou.seat_input_buffer=0;
        }
        unsigned keys=o->base.held;
        /* Dodge requires a new press at or after insertion; it never uses
         * the attack buffer. A prior held dodge is consumed across the handoff. */
        if(frame>=58&&keys==ML_B&&(edge&0x4000)&&!(raw&PAD_BOOST)){
            WDecision d=w_leave(&world);d.kind=W_ROLL;d.sheathed_roll=1;
            d.heading=o->base.magnitude>=50?o->base.input_heading:o->base.heading;
            log_event("ascendance_musou_seat_b",(u32)(uptr)a,(u32)frame,d.heading,0);
            log_event("ascendance_musou_end",(u32)(uptr)a,musou.cycles,musou.hits,0);
            musou=(__typeof__(musou)){0};
            return d;
        }
        unsigned fresh_x=keys==ML_X&&(edge&PAD_X);
        unsigned fresh_r=keys==ML_R&&(edge&PAD_R);
        unsigned fresh_a=keys==ML_A&&(edge&0x2000);
        if((keys&&keys!=ML_X&&keys!=ML_R&&keys!=ML_A)||(raw&PAD_BOOST)){
            musou.seat_input_buffer=0;
        }else if((fresh_x||fresh_r||fresh_a)&&frame>=58-grace){
            musou.seat_key=keys;
            musou.seat_input_buffer=grace;
        }
        if(frame>=58&&musou.seat_input_buffer>0){
            unsigned spirit=musou.seat_key==ML_R;
            unsigned secondary=musou.seat_key==ML_A;
            WDecision d=secondary?ascendance_double_start(a,o,0):w_radiance_enter(&world,o,spirit?W_BIG:W_SMALL);
            if(d.kind==W_ENTER){
                if(!secondary){d.motion_contract=1;d.start=0;d.speed=1;d.blend=spirit?6:7;}
                if(spirit){
                    /* The insertion has completed, just as for a fresh stance Iai. */
                    ascendance.sheathe=ascendance.remaining>0;
                    ascendance.ready_frames=0;
                }
                log_event(secondary?"ascendance_musou_seat_a":spirit?"ascendance_musou_seat_r":"ascendance_musou_seat_x",(u32)(uptr)a,(u32)frame,d.slot,0);
                log_event("ascendance_musou_end",(u32)(uptr)a,musou.cycles,musou.hits,0);
                musou=(__typeof__(musou)){0};
            }
            if(d.kind!=W_KEEP)return d;
            musou.seat_input_buffer=0;
        }
    }
    if(musou.phase==MUSOU_ENTRY){
        if(bound&&(frame>=MUSOU_ENTRY_END||o->base.motion_finished))
            return ascendance_musou_motion(o,MUSOU_FLURRY,W_SMALL);
    }else if(musou.phase==MUSOU_FLURRY){
        if(bound&&musou_spin_slot(world.slot)&&frame>=11&&musou.cycles<5&&musou.burst_level<=musou.cycles)
            musou.burst_level=musou.cycles+1;
        if(musou_spin_slot(world.slot)&&musou.spin_sample!=o->base.sample_sequence){
            musou.spin_sample=o->base.sample_sequence;
            float dt=o->base.simulation_delta;
            if(dt>6)dt=6;
            if(dt>0){
                /* Approximate the original 55 ms exponential input filter. */
                float t=dt/(60*.055f);
                float ease=1-1/(1+t*(1+t*(.5f+t*(1.f/6+t/24))));
                float strength=o->base.magnitude>=50?w_scale_integer(o->base.magnitude,1.f/128.f):0;
                unsigned yaw=o->base.input_heading;
                musou.spin_x+=(strength*world_sin(yaw)-musou.spin_x)*ease;
                musou.spin_z+=(strength*world_sin(yaw+16384)-musou.spin_z)*ease;
            }
        }
        if(o->base.held&ML_R){
            musou.sustain_frames=MUSOU_SUSTAIN_FRAMES;
        }else if(musou.sustain_sample!=o->base.sample_sequence&&o->base.simulation_delta>0){
            musou.sustain_frames-=o->base.simulation_delta;
            if(musou.sustain_frames<0)musou.sustain_frames=0;
        }
        musou.sustain_sample=o->base.sample_sequence;
        const MusouStroke *clip=musou_clip(0x500+world.slot);
        float end=clip?(float)(clip->frames-3):38;
        if(bound&&(frame>=end||(clip&&o->base.motion_finished))){
            unsigned limit=MUSOU_STROKE_COUNT+(musou.opening_path?12:0);
            unsigned capped=clip?musou.stroke>=limit:musou.cycles>=MUSOU_LEGACY_CYCLES;
            if(ascendance.remaining<=0||musou.sustain_frames<=0||capped)return ascendance_musou_dash(a,o);
            if(clip){
                ++musou.stroke;
                if((musou.stroke==3||musou.stroke==7||musou.stroke==12||musou.stroke==17)&&!musou.opening_path)++musou.stroke;
            }
            return ascendance_musou_motion(o,MUSOU_FLURRY,W_SMALL);
        }
    }else if(musou.phase==MUSOU_DASH&&bound&&(frame>=(world.slot>=33&&world.slot<=39?32:43)||o->base.motion_finished))
        return ascendance_musou_return(a,o);
    else if(bound&&((musou.phase==MUSOU_CUT&&(frame>=(world.slot==40?32:39)||o->base.motion_finished))||
                    (musou.phase==MUSOU_SEAT&&(frame>=59||o->base.motion_finished)))){
        unsigned seated=musou.phase==MUSOU_SEAT;
        if(!seated&&world.slot==40){
            const void *res=(const void *)(uptr)word(a,0xf18);
            const void *bank=res?(const void *)(uptr)word(res,0x64):0;
            const void *desc=bank&&41<u16(bank,6)?(const void *)(uptr)word(bank,8+4*41):0;
            if(desc&&word(desc,8)==62){
                WDecision seat=ascendance_musou_motion(o,MUSOU_SEAT,W_SMALL);
                if(seat.kind==W_MOTION||seat.kind==W_ENTER){
                    world.slot=seat.slot=41;seat.blend=0;
                    world.heading=seat.heading=musou.return_heading;
                    log_event("ascendance_dash_home",(u32)(uptr)a,word(a,0x40),word(a,0x48),seat.heading);
                }
                return seat;
            }
        }
        WDecision d=w_radiance_enter(&world,o,W_SHEATHE);
        if(d.kind==W_ENTER){
            ascendance.musou_sheathe_r=o->base.held&ML_R;
            if(seated)world.consumed|=o->base.held&ML_B;
            d.motion_contract=1;d.blend=seated?6:9;d.start=seated?80:0;d.speed=1;
            if(seated){
                const void *res=(const void *)(uptr)word(a,0xf18);
                const void *bank=res?(const void *)(uptr)word(res,0x64):0;
                const void *desc=bank&&32<u16(bank,6)?(const void *)(uptr)word(bank,8+4*32):0;
                if(desc&&word(desc,8)==179){world.slot=d.slot=32;d.blend=0;}
            }
            world.heading=d.heading=seated?musou.dash_heading:musou.return_heading;
            if(!seated)log_event("ascendance_dash_home",(u32)(uptr)a,word(a,0x40),word(a,0x48),d.heading);
            log_event("ascendance_musou_end",(u32)(uptr)ascendance.actor,musou.cycles,musou.hits,0);
        }
        return d;
    }
    /* The reused Iai actions supply animation and collision. Do not
     * consume ordinary Iai follow-ups or award their spirit regeneration. */
    o->base.held=0;o->contact=0;
    return (WDecision){0};
}

static unsigned ascendance_walk_available(void *a)
{
    const void *resource=(const void *)(uptr)word(a,0xf04+5*4);
    const void *bank=resource?(const void *)(uptr)word(resource,0x64):0;
    return bank&&word(bank,0)==0x00544d4c&&u16(bank,6)>RADIANCE_WALK_SLOT&&
        word(bank,8+4*RADIANCE_WALK_SLOT);
}

static float ascendance_walk_rate(void)
{
    float rate=1.35f*ascendance.walk_speed/RADIANCE_WALK_UNITS_PER_TICK;
    return rate>.45f?rate:.45f;
}

static void ascendance_walk_bind(void *a,WObservation *o,unsigned slot,float start,float blend)
{
    float rate=slot==RADIANCE_WALK_SLOT?ascendance_walk_rate():1;
    unsigned previous=world.slot;
    original_layer(a,0,5000+slot,blend>0?0x81:0,blend,start,rate);
    if(u16(a,MOTION)!=0x500+slot)return;
    world.slot=slot;
    o->base.motion=u16(a,MOTION);
    o->base.animation_frame=real(a,FRAME);
    o->base.motion_finished=0;
    if(previous!=slot)log_event("ascendance_walk", (u32)(uptr)a,slot,world.heading,(u32)(ascendance.walk_speed*100));
}

static void ascendance_walk_tick(void *a,WObservation *o,float frames)
{
    unsigned walking=world.slot==RADIANCE_WALK_SLOT;
    unsigned magnitude=o->base.magnitude;
    ascendance.walk_intent=magnitude>=(ascendance.walk_intent?43:50);
    float target=0;
    if(ascendance.walk_intent){
        int delta=ml_heading_delta(o->base.input_heading,world.heading);
        int limit=(int)(1390.f*frames);
        if(delta>limit)delta=limit;
        if(delta< -limit)delta= -limit;
        world.heading=(world.heading+delta)&65535;
        put(a,HEADING,world.heading);
        put(a,TARGET,world.heading);
        o->base.heading=world.heading;

        int gap=ml_heading_delta(o->base.input_heading,world.heading);
        unsigned turn=gap<0?-gap:gap;
        /* Ease from a shuffle to a full stride as the hips face the new direction. */
        float aligned=(16384.f-turn)/10923.f;
        if(aligned<0)aligned=0;
        if(aligned>1)aligned=1;
        aligned=aligned*aligned*(3-2*aligned);
        target=RADIANCE_WALK_UNITS_PER_TICK*w_scale_integer(magnitude,1.f/128.f)*(.24f+.76f*aligned);
    }
    float speed=ascendance.walk_speed, change=.7523f*frames;
    if(target>speed+change)speed+=change;
    else if(target<speed-change)speed-=change;
    else speed=target;
    ascendance.walk_speed=speed;
    if(!walking&&speed>0)
        ascendance_walk_bind(a,o,RADIANCE_WALK_SLOT,RADIANCE_HOLD_FRAME,8);
    else if(walking&&speed<=0)
        ascendance_walk_bind(a,o,RADIANCE_HOLD_SLOT,RADIANCE_HOLD_FRAME,8);
    else if(walking&&o->base.animation_frame>=RADIANCE_HOLD_FRAME+RADIANCE_STRIDE_FRAMES){
        float phase=o->base.animation_frame-RADIANCE_STRIDE_FRAMES;
        ascendance_walk_bind(a,o,RADIANCE_WALK_SLOT,phase,0);
    }
}

static void ascendance_walk_floor(void *a)
{
    if(ascendance.radiance!=RADIANCE_HOLD||!ascendance_owner(a)||
       !world_bound||world_bound_actor!=(u32)(uptr)a||world.move!=W_SHEATHE||
       u16(a,STATE)!=16||u16(a,ACTION)!=w_action(W_SHEATHE)||
       u16(a,MOTION)!=0x500+world.slot||(word(a,AIR)&0x400))return;
    if(world.slot!=RADIANCE_WALK_SLOT&&
       (world.slot!=RADIANCE_HOLD_SLOT||real(a,FRAME)<RADIANCE_HOLD_FRAME))return;
    float frames=real(a,0x1c);
    if(!(frames>0&&frames<=4))return;
    /* Drop the old pose's root contribution when the walk blend finishes. */
    float step=world.slot==RADIANCE_WALK_SLOT?frames*ascendance.walk_speed:0;
    set_real(a,0x40,real(a,0x1000)+step*world_sin(world.heading));
    set_real(a,0x48,real(a,0x1008)+step*world_sin(world.heading+16384));
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}

#if ML_WILDS_MOVES
static struct {
    unsigned actor,epoch,last_sample,applied,brake_sample,loop_seen;
    float vx,vz,brake_age;
} release_run_join;

enum { RELEASE_RUN_NONE, RELEASE_RUN_START, RELEASE_RUN_JOIN,
       RELEASE_RUN_STEADY, RELEASE_RUN_BRAKE, RELEASE_RUN_SETTLE };

static unsigned release_run_stage(void *a)
{
    if(release_run_join.actor!=(u32)(uptr)a)return RELEASE_RUN_NONE;
    if(release_run_join.epoch!=byte(a,AREA)||u16(a,STATE)!=2||
       (word(a,AIR)&0x400)){
        release_run_join.actor=0;return RELEASE_RUN_NONE;
    }
    unsigned action=u16(a,ACTION),motion=u16(a,MOTION),stick=u16(a,0x1410);
    unsigned moving=stick>=50&&stick<=128;
    if(action==4&&motion==42&&moving)return RELEASE_RUN_START;
    if(action==5&&motion==4){
        if(real(a,FRAME)<13.f&&moving&&release_run_join.last_sample)
            return RELEASE_RUN_JOIN;
        release_run_join.loop_seen=1;
        return RELEASE_RUN_STEADY;
    }
    if(action==3&&motion==30&&!moving&&release_run_join.loop_seen){
        if(real(a,FRAME)<.05f&&sample_sequence-release_run_join.last_sample<=4&&
           (!release_run_join.brake_sample||release_run_join.brake_sample==sample_sequence))
            return RELEASE_RUN_BRAKE;
        if(release_run_join.brake_sample&&release_run_join.brake_age<11.f)
            return RELEASE_RUN_SETTLE;
    }
    release_run_join.actor=0;
    return RELEASE_RUN_NONE;
}
static void release_run_before_wall(void *a)
{
    float frame=real(a,FRAME),dt=real(a,0x1c);
    if(!(frame>=8.f&&frame<13.f&&dt>0&&dt<=2)||release_run_join.applied==sample_sequence)return;
    release_run_join.applied=sample_sequence;
    unsigned yaw=u16(a,HEADING);
    float sx=world_sin(yaw),sz=world_sin(yaw+16384);
    float carry=(release_run_join.vx*sx+release_run_join.vz*sz)*dt;
    if(!(carry>0&&carry<12.f))return;
    float x=real(a,0x1000),z=real(a,0x1008);
    float dx=real(a,0x40)-x,dz=real(a,0x48)-z;
    if(frame>8.05f&&dx*sx+dz*sz<=0)return;
    /* The loop bind contributes no root delta. Carry the last accepted
     * start step into the new clip and rejoin the native stride by F13. */
    float t=(frame-8.f)/5.f;
    t=t*t*(3.f-2.f*t);
    set_real(a,0x40,x+(1.f-t)*carry*sx+t*dx);
    set_real(a,0x48,z+(1.f-t)*carry*sz+t*dz);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}
static void release_brake_before_wall(void *a)
{
    release_run_join.brake_sample=sample_sequence;
    release_run_join.applied=sample_sequence;
    float dt=real(a,0x1c);
    if(!(dt>0&&dt<=2))return;
    float x=real(a,0x1000),z=real(a,0x1008);
    float dx=real(a,0x40)-x,dz=real(a,0x48)-z;
    float length=att_sqrt(dx*dx+dz*dz);
    if(!(length>.01f))return;
    /* Bound the first brake request by the last accepted running pace.
     * Project that pace onto the native brake direction before wall contact. */
    float limit=(release_run_join.vx*dx+release_run_join.vz*dz)*dt/length;
    if(limit<0)limit=0;
    if(length<=limit)return;
    float weight=limit/length;
    set_real(a,0x40,x+weight*dx);
    set_real(a,0x48,z+weight*dz);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}
static void release_brake_settle(void *a)
{
    float dt=real(a,0x1c);
    if(!(dt>0&&dt<=2)||release_run_join.applied==sample_sequence)return;
    release_run_join.applied=sample_sequence;
    release_run_join.brake_age+=dt;
    float age=release_run_join.brake_age;
    if(!(age>4.f&&age<11.f))return;
    /* Redistribute the movement around the final frozen F0 sample.
     * At unit updates these offsets sum to zero. Native frames still advance normally. */
    static const float offsets[]={0,.10f,1.45f,.25f,-.75f,-.75f,-.30f,0};
    unsigned index=(unsigned)(age-4.f);
    float fraction=age-4.f-index;
    float adjustment=(offsets[index]+(offsets[index+1]-offsets[index])*fraction)*dt;
    float x=real(a,0x1000),z=real(a,0x1008);
    float dx=real(a,0x40)-x,dz=real(a,0x48)-z;
    float length=att_sqrt(dx*dx+dz*dz),direction_length=length;
    if(length<.01f){
        if(adjustment<=0)return;
        dx=release_run_join.vx;dz=release_run_join.vz;
        direction_length=att_sqrt(dx*dx+dz*dz);
        if(direction_length<.01f)return;
    }
    float next=length+adjustment;
    if(next<0)next=0;
    set_real(a,0x40,x+dx*next/direction_length);
    set_real(a,0x48,z+dz*next/direction_length);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}
static void release_run_after_wall(void *a)
{
    float dt=real(a,0x1c);
    if(!(dt>0&&dt<=2)||release_run_join.last_sample==sample_sequence)return;
    release_run_join.last_sample=sample_sequence;
    release_run_join.vx=(real(a,0x40)-real(a,0x1000))/dt;
    release_run_join.vz=(real(a,0x48)-real(a,0x1008))/dt;
}
#endif

#if ML_WILDS_MOVES
static struct {
    void *actor;
    unsigned area,entry_sample,last_sample;
    float age,carry_x,carry_z;
} slide_third;

static float slide_third_carry_fraction(float age)
{
    static const float fractions[]={0,.32f,.62f,.84f,1};
    if(age>=4)return 1;
    unsigned index=(unsigned)age;
    return fractions[index]+(age-index)*(fractions[index+1]-fractions[index]);
}

static void slide_third_before_wall(void *a)
{
    if(slide_third.actor!=a)return;
    if(slide_third.area!=byte(a,AREA)||(word(a,AIR)&0x400)){
        slide_third.actor=0;
        return;
    }
    unsigned motion=u16(a,MOTION);
    if(!slide_third.last_sample&&sample_sequence-slide_third.entry_sample<=4&&
       (motion==0x555||motion==0x556))return;
    if(u16(a,STATE)!=16||u16(a,ACTION)!=26||motion!=0x56c||real(a,FRAME)>.05f){
        slide_third.actor=0;
        return;
    }
    float dt=real(a,0x1c);
    if(!(dt>0&&dt<=2)||slide_third.last_sample==sample_sequence)return;
    float x=real(a,0x40),z=real(a,0x48),dx,dz;
    if(!slide_third.last_sample){
        float vx=x-real(a,0x1000),vz=z-real(a,0x1008);
        if(vx*vx+vz*vz<4){
            slide_third.actor=0;
            return;
        }
        /* The bind update submits the last sliding step at full size.
         * Ease part of it into the following frozen startup samples. */
        slide_third.carry_x=.43f*vx;
        slide_third.carry_z=.43f*vz;
        dx=-slide_third.carry_x;
        dz=-slide_third.carry_z;
    }else{
        float end=slide_third.age+dt;
        float delta=slide_third_carry_fraction(end)-slide_third_carry_fraction(slide_third.age);
        dx=delta*slide_third.carry_x;
        dz=delta*slide_third.carry_z;
        slide_third.age=end;
        if(end>=4)slide_third.actor=0;
    }
    slide_third.last_sample=sample_sequence;
    set_real(a,0x40,x+dx);
    set_real(a,0x48,z+dz);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
    log_event("slide_third_carry",sample_sequence,word(&slide_third.age,0),word(&dx,0),word(&dz,0));
}
#endif

#if ML_WILDS_MOVES
static struct {
    void *actor;
    unsigned area,sample,active,last_sample;
    float vx,vz;
} round_walk;

static unsigned round_walk_before_wall(void *a)
{
    if(round_walk.actor!=a||!round_walk.active)return 0;
    unsigned stick=u16(a,0x1410);
    if(round_walk.area!=byte(a,AREA)||u16(a,STATE)!=2||u16(a,ACTION)!=1||
       u16(a,MOTION)!=3||real(a,FRAME)>.05f||stick<50||stick>128||(word(a,AIR)&0x400)){
        round_walk.actor=0;
        return 0;
    }
    float dt=real(a,0x1c);
    if(!(dt>0&&dt<=2)||round_walk.last_sample==sample_sequence)return 0;
    float x=real(a,0x1000),z=real(a,0x1008);
    float dx=real(a,0x40)-x,dz=real(a,0x48)-z;
    if(real(a,0x4f4)<=.01f&&real(a,0x4fc)>.05f&&dx*dx+dz*dz<.01f){
        /* The frozen walk clock starts advancing one update after the
         * stock blend runs out. Carry the last accepted walking velocity
         * through that update before native wall and floor resolution. */
        if(round_walk.vx*round_walk.vx+round_walk.vz*round_walk.vz>=1){
            set_real(a,0x40,x+dt*round_walk.vx);
            set_real(a,0x48,z+dt*round_walk.vz);
            ((void (*)(void *))(main_base+0x53a9fc))(a);
            log_event("round_walk_carry",sample_sequence,word(&round_walk.vx,0),word(&round_walk.vz,0),0);
        }
        round_walk.actor=0;
        return 0;
    }
    return 1;
}

static void round_walk_after_wall(void *a)
{
    float dt=real(a,0x1c);
    round_walk.last_sample=sample_sequence;
    round_walk.vx=(real(a,0x40)-real(a,0x1000))/dt;
    round_walk.vz=(real(a,0x48)-real(a,0x1008))/dt;
}
#endif

/* Submit controlled root travel before the native wall and floor solvers. */
static NativeCall ascendance_original_wall;
static u32 ascendance_wall(void *a,u32 b,u32 c,u32 d)
{
    if(ascendance_dash_floor(a,1))
        ((void (*)(void *))(main_base+0x53a9fc))(a);
    float chase_step=ascendance_pursuit_floor(a,1);
    ascendance_walk_floor(a);
    ascendance_flash_floor(a);
#if ML_WILDS_MOVES
    slide_third_before_wall(a);
    unsigned round_walking=round_walk_before_wall(a);
    unsigned release_stage=release_run_stage(a);
    if(release_stage==RELEASE_RUN_JOIN)release_run_before_wall(a);
    if(release_stage==RELEASE_RUN_BRAKE)release_brake_before_wall(a);
    if(release_stage==RELEASE_RUN_SETTLE)release_brake_settle(a);
#endif
    float x=real(a,0x40),z=real(a,0x48);
    u32 result=ascendance_original_wall(a,b,c,d);
#if ML_WILDS_MOVES
    if(round_walking)round_walk_after_wall(a);
    if(release_stage==RELEASE_RUN_START||release_stage==RELEASE_RUN_STEADY)
        release_run_after_wall(a);
#endif
    if(chase_step>0&&world.slot==PURSUIT_RUN_SLOT){
        /* Only count progress lost in the wall solver, before body contact. */
        float advance=(x-real(a,0x40))*world_sin(world.heading)+
            (z-real(a,0x48))*world_sin(world.heading+16384);
        if(radiance_pursuit.speed>=2&&advance>=.75f*chase_step)
            radiance_pursuit.wall_stall_frames+=real(a,0x1c);
        else radiance_pursuit.wall_stall_frames=0;
    }
    return result;
}

static WDecision ascendance_radiance_begin(void *a,const WObservation *o)
{
    WDecision d=w_radiance_enter(&world,o,W_SHEATHE);
    if(d.kind==W_ENTER){
        if(o->state==16||o->state==0x4000){
            d.motion_contract=1;d.blend=8;d.speed=1;
        }
        if(ascendance.remaining<=0){
            log_event("ascendance_start",(u32)(uptr)a,25,0,1);
        }
        ascendance_spend_reserve();
        ascendance.radiance=RADIANCE_HOLD;
        ascendance.radiance_entry_sample=o->base.sample_sequence;
        ascendance.radiance_entry_counter=0;
        ascendance.boost_toggle_pending=0;
        ascendance.flash_step_release=0;
        ascendance.radiance_hit=0;
        ascendance.flash_r_buffered=0;
        radiance_pursuit=(__typeof__(radiance_pursuit)){0};
        log_event("ascendance_radiance",(u32)(uptr)a,RADIANCE_INPUT,
            (o->state<<16)|o->base.native_action,(u32)(o->base.animation_frame*1000));
    }
    return d;
}

static WDecision ascendance_flash_enter(void *a,const WObservation *o,unsigned stepping)
{
    WDecision d=w_radiance_enter(&world,o,W_BIG);
    if(d.kind!=W_ENTER)return d;
    if(stepping){d.motion_contract=1;d.blend=9;d.start=0;d.speed=1;}
    if(ascendance_flash_resource(a)){
        world.slot=d.slot=RADIANCE_FLASH_SLOT;
        world.iai_focus_locked=1;
        d.motion_contract=1;d.blend=stepping?8:6;d.start=0;d.speed=1;
        ascendance.flash_travel=0;
        log_event("ascendance_flash_step",(u32)(uptr)a,word(a,0x40),word(a,0x48),world.heading);
    }
    world.counter_success=ascendance.radiance_entry_counter;
    ascendance.radiance_entry_counter=0;
    ascendance.radiance=RADIANCE_RELEASE;
    return d;
}

static WDecision ascendance_radiance_command(void *a,WObservation *o,unsigned raw,unsigned edge,unsigned start)
{
    if(ascendance.musou_sheathe_r){
        if(world.move==W_SHEATHE&&(o->base.held&ML_R))o->base.held&=~ML_R;
        else ascendance.musou_sheathe_r=0;
    }
    if(ascendance_owner(a)&&ascendance.double_cut&&world.move==W_SMALL){
        if(o->state!=16||o->base.native_action!=w_action(W_SMALL)||o->base.motion!=0x500+world.slot)
            return (WDecision){0};
        if(!o->grounded||!o->alive||!o->resources_ready){
            WDecision d=w_leave(&world);d.kind=W_ABORT;return d;
        }
        unsigned keys=o->base.held;
        o->contact=0;
        o->base.held=0;
        if(world.slot==DOUBLE_DRAW_RETURN){
            /* Only a fresh single-key attack can replace a pending request.
             * Releasing the button keeps it until the stance accepts input. */
            if((keys&&keys!=ML_A&&keys!=ML_X&&keys!=ML_R)||(raw&PAD_BOOST))
                ascendance.resheathe_pending_key=0;
            else if((keys==ML_A&&(edge&0x2000))||(keys==ML_X&&(edge&PAD_X))||(keys==ML_R&&(edge&PAD_R)))
                ascendance.resheathe_pending_key=keys;
            /* The requested hold binds on the following update. The outgoing
             * layer still evaluates once, including its final insertion pose. */
            float evaluated=o->base.animation_frame;
            float delta=o->base.simulation_delta*real(a,0x50c);
            if(delta>0)evaluated+=delta;
            if(evaluated>=24.f-.0001f||o->base.motion_finished){
                WDecision d=w_radiance_enter(&world,o,W_SHEATHE);
                if(d.kind==W_ENTER){
                    d.motion_contract=1;d.start=80;d.blend=7;d.speed=1;
                    log_event("ascendance_double_seated",(u32)(uptr)a,(u32)(evaluated+.0001f),0,0);
                }
                return d;
            }
            return (WDecision){0};
        }
        if(o->base.animation_frame>=DOUBLE_DRAW_END+double_draw_offset(world.slot)||o->base.motion_finished){
            log_event("ascendance_double_home",world.slot,(u32)o->base.animation_frame,0,0);
            ascendance.double_travel_frame=ascendance.double_step?-1:0;
            WDecision d=ascendance_pursuit_motion(DOUBLE_DRAW_RETURN,0,3);
            d.speed=7.f/9.f;
            return d;
        }
        return (WDecision){0};
    }
    /* A press sampled on the displayed final return pose reaches the
     * command driver as the hold binds. Offer it once through the hold gate. */
    if(ascendance.resheathe_handoff&&w_iai_ready(&world,o)){
        ascendance.resheathe_handoff=0;
        /* Entry-period dodge presses expire on release. Only a new, single
         * dodge on the handoff sample belongs to the native hold. */
        if((o->base.held&ML_B)&&(!(edge&0x4000)||o->base.held!=ML_B||(raw&PAD_BOOST)))
            world.consumed|=ML_B;
        if(!(raw&PAD_BOOST)&&o->base.animation_frame<81&&
           ((o->base.held==ML_X&&(edge&PAD_X))||(o->base.held==ML_R&&(edge&PAD_R))))
            ascendance.resheathe_pending_key=o->base.held;
    }
    if(ascendance.resheathe_pending_key&&
       (world.move!=W_SHEATHE||(o->base.held&~ascendance.resheathe_pending_key)||(raw&PAD_BOOST)||
        !o->alive||!o->grounded||!o->resources_ready||
        (o->base.motion==0x500+world.slot&&o->base.animation_frame>92)))
        ascendance.resheathe_pending_key=0;
    unsigned fresh_a=(edge&0x2000)&&o->base.held==ML_A;
    if(ascendance_owner(a)&&!ascendance.radiance&&!musou.phase&&!(raw&PAD_BOOST)&&
       (ascendance.resheathe_pending_key||fresh_a)&&w_iai_ready(&world,o)){
        unsigned key=fresh_a?ML_A:ascendance.resheathe_pending_key;
        ascendance.resheathe_pending_key=0;
        if(key==ML_X||key==ML_R)return w_iai_attack(&world,o,key);
        WDecision d=ascendance_double_start(a,o,world.slot==MUSOU_RELAX_SLOT||(world.slot!=32&&ascendance.sheathe));
        if(d.kind!=W_KEEP)return d;
    }
    if(ascendance_owner(a)&&(musou.phase||ascendance.radiance==RADIANCE_FOLLOW)){
        WDecision d=ascendance_musou_command(a,o,raw,edge);
        if(d.kind!=W_KEEP||musou.phase||!radiance_pursuit.independent)return d;
        return ascendance_pursuit_cuts(a,o);
    }
    if(ascendance_owner(a)&&(ascendance.radiance==RADIANCE_FLASH||ascendance.radiance==RADIANCE_CHASE))
        return ascendance_pursuit_command(a,o,edge);
#if ML_WILDS_MOVES
    WDecision red=ascendance_red_musou_command(a,o,edge);
    if(red.kind!=W_KEEP)return red;
#endif
    unsigned keys=o->base.held;
    if(start==2){
        WDecision d=ascendance_flash_enter(a,o,0);
        if(d.kind==W_ENTER){
            unsigned reused=ascendance.remaining>0;
            if(!reused)ascendance_spend_reserve();
            ascendance.radiance_hit=ascendance.flash_r_buffered=ascendance.boost_toggle_pending=0;
            radiance_pursuit=(__typeof__(radiance_pursuit)){0};
            log_event("ascendance_charge_flash",(u32)(uptr)a,reused,ascendance.charge,(u32)ascendance.remaining);
        }
        return d;
    }
    if(start)return ascendance_radiance_begin(a,o);
    if(ascendance.radiance!=RADIANCE_HOLD||!ascendance_owner(a)||world.move!=W_SHEATHE)
        return (WDecision){0};
    if(!o->grounded||!o->alive||!o->resources_ready)return (WDecision){0};
    unsigned ready=w_iai_ready(&world,o);
    if(ready&&ascendance.radiance_sample!=o->base.sample_sequence){
        ascendance.radiance_sample=o->base.sample_sequence;
        float frames=o->base.simulation_delta;
        if(!ascendance.flash_step_release&&(keys&RADIANCE_INPUT)&&frames>0&&frames<30){
            typedef void *(__attribute__((pcs("aapcs-vfp"))) *StaminaDelta)(void *,int,float);
            ((StaminaDelta)(main_base+0x2a3e50))(a,1,-2.5f*frames);
            if(ascendance.remaining>0&&real(a,RADIANCE_STAMINA)>0&&ascendance_walk_available(a))
                ascendance_walk_tick(a,o,frames<=4?frames:4);
            else if(o->base.magnitude>=50){
                int limit=(int)(728.f*frames);
                int delta=ml_heading_delta(o->base.input_heading,world.heading);
                if(delta>limit)delta=limit;
                if(delta< -limit)delta= -limit;
                world.heading=(world.heading+delta)&65535;
                put(a,HEADING,world.heading);
                put(a,TARGET,world.heading);
                o->base.heading=world.heading;
            }
        }
    }
    float stamina=real(a,RADIANCE_STAMINA);
    if(ready&&(!(keys&RADIANCE_INPUT)||ascendance.remaining<=0||stamina<=0||
               ascendance.flash_step_release)){
        /* Keep the bound stance through the last floor update of a short drop. */
        if(radiance_step.actor==a&&radiance_step.area==byte(a,AREA)){
            ascendance.flash_step_release=1;
            world.hold_seconds=0;
            o->base.held=0;
            return (WDecision){0};
        }
        ascendance.flash_step_release=0;
        unsigned stepping=world.slot==RADIANCE_WALK_SLOT;
        o->base.heading=world.heading;
        WDecision d=ascendance_flash_enter(a,o,stepping);
        if(d.kind==W_ENTER)
            log_event("ascendance_flash",(u32)(uptr)a,keys,stamina>0?(unsigned)stamina:0,world.heading);
        return d;
    }
    world.hold_seconds=0;
    o->base.held=0;
    return (WDecision){0};
}

static void ascendance_tick(void *a,const WObservation *o,unsigned raw,unsigned edge)
{
    const MLObservation *b=&o->base;
    unsigned area=byte(a,AREA);
    unsigned quest=ascendance_quest_data();
    if(!o->alive||hunter_style(a)!=0||ascendance_in_town()){
        if(ascendance.actor==a)ascendance_rebind(0,0,0,0,0);
        ml_ascendance_gauge(0);
        return;
    }
    unsigned owned=ascendance.actor==a;
    if(!quest||!o->resources_ready||!area)area=255;
    if(!quest&&owned)quest=ascendance.quest;
    /* Loading and camp suspend the meter. A new combat area only resets move
     * state and target references, while the same quest keeps its progress. */
    if(!owned||ascendance.quest!=quest||ascendance.area!=area){
        ascendance_rebind(a,quest,area,b->sample_sequence,owned&&quest==ascendance.quest);
    }
    if(area==255){
        ml_ascendance_gauge(0);
        return;
    }
    if(ascendance.sample!=b->sample_sequence){
        ascendance.sample=b->sample_sequence;
        if(b->simulation_delta>0&&b->simulation_delta<30){
            unsigned hold=ascendance.radiance==RADIANCE_HOLD&&world.move==W_SHEATHE;
            unsigned chase=ascendance.radiance==RADIANCE_CHASE&&world.move==W_THRUST;
            unsigned paused=(hold||chase)&&o->state==16&&b->native_action==w_action(world.move);
            if(ascendance.remaining>0&&!paused){
                ascendance.remaining-=b->simulation_delta;
                if(ascendance.remaining<=0){
                    ascendance.remaining=0;
                    if(!ascendance.counter_sheathe)ascendance.sheathe=0;
                    log_event("ascendance_end",(u32)(uptr)a,0,0,0);
                }
            }
            if(ascendance.remaining<=0&&ascendance.charge<BOOST_CAPACITY){
                ascendance.passive_frames+=b->simulation_delta;
                if(ascendance.passive_frames>=60){
                    ascendance.passive_frames-=60;
                    ++ascendance.charge;
                }
            }else ascendance.passive_frames=0;
            if(ascendance.sheathe&&world.slot!=32&&w_iai_ready(&world,o)){
                /* Keep the full opening frame available to the Iai input. */
                if(ascendance.ready_frames==0)ascendance.ready_frames=.001f;
                else ascendance.ready_frames+=b->simulation_delta;
                if(ascendance.ready_frames>=BOOST_SHEATHE_WINDOW){
                    ascendance.sheathe=0;
                    log_event("ascendance_sheathe_expire",(u32)(uptr)a,0,0,0);
                }
            }
        }
    }
    unsigned idle=!world.move&&(o->state==1||o->state==2)&&b->native_action<=5;
    unsigned sheathed=w_iai_ready(&world,o);
    /* Ignition during a native ordinary sheathe pays for Boost only.
     * Accept the entry phase as well as the ready phase. */
    unsigned ordinary_sheathe=world.move==W_SHEATHE&&world.slot==RADIANCE_HOLD_SLOT&&
        !ascendance.radiance&&world_bound&&world_bound_actor==(u32)(uptr)a&&
        o->state==16&&b->native_action==w_action(W_SHEATHE)&&
        b->motion==0x500+RADIANCE_HOLD_SLOT&&o->alive&&o->drawn;
    unsigned toggle_available=o->grounded&&!b->held&&(idle||(ordinary_sheathe&&ascendance.remaining<=0));
#if ML_SWAP_R_ZR
    /* A physical R hold used for sheathed movement must not ignite or end
     * Ascendance on release, even when the stick returns at the same time. */
    if(original_quest_input&&!o->drawn&&(b->magnitude>=50||w_uphill_run(o->state,b->native_action)))
        toggle_available=0;
#endif
    /* The boost key can lead the Radiance chord by several frames. Spend a
     * reserve for standalone ignition only after an unaccompanied release. */
    if(!toggle_available){
        ascendance.boost_toggle_pending=0;
    }else if(b->simulation_delta>0){
        if(edge&PAD_BOOST)ascendance.boost_toggle_pending=1;
        if(ascendance.boost_toggle_pending&&!(raw&PAD_BOOST)){
            ascendance.boost_toggle_pending=0;
            if(ascendance.remaining>0){
                unsigned refund=(unsigned)(ascendance.remaining*BOOST_COST/BOOST_DURATION+.5f);
                unsigned free=BOOST_CAPACITY-ascendance.charge;
                if(refund>free)refund=free;
                ascendance.charge+=refund;
                ascendance.remaining=0;
                log_event("ascendance_end",(u32)(uptr)a,1,refund,ascendance.charge);
            }else if(ascendance.charge>=BOOST_COST){
                ascendance_spend_reserve();
                log_event("ascendance_start",(u32)(uptr)a,25,ordinary_sheathe,ordinary_sheathe?(u32)b->animation_frame:0);
            }
        }
    }
    unsigned active=ascendance.remaining>0;
    unsigned remaining=active?(unsigned)((ascendance.remaining*100.f+BOOST_DURATION-1)/BOOST_DURATION):0;
    unsigned enhanced=ascendance.sheathe&&sheathed;
    unsigned radiance=ascendance.radiance==RADIANCE_HOLD;
    unsigned pursue=ascendance.radiance==RADIANCE_CHASE;
    unsigned ready=active&&ascendance.radiance==RADIANCE_FOLLOW&&radiance_pursuit.rewarded;
#if ML_WILDS_MOVES
    ready|=active&&o->rank==3&&o->grounded&&red_musou.hit&&ascendance_red_spin(a);
#endif
    float dx,dz;
    ready|=ascendance_flash_link(a,o)&&o->base.animation_frame>=FLASH_MUSOU_BRANCH&&
        !ascendance_flash_chase_target(a,&dx,&dz);
    unsigned musou_hint=musou.phase?2u:ready;
    unsigned rt_prompt=!active&&ascendance.charge>=BOOST_COST&&solo_ascendance_rt(a);
    ml_ascendance_gauge(1|(active<<1)|(enhanced<<2)|(radiance<<3)|(pursue<<4)|
        (musou_hint<<5)|(rt_prompt<<7)|(remaining<<8)|(ascendance.charge<<16));
}
