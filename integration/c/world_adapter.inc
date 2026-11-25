#if ML_WILDS_MOVES
static unsigned world_wilds_pending,wilds_input_previous,wilds_input_actor,wilds_input_epoch;
static MLObservation wilds_slide_draw_input;
static unsigned wilds_focus_active;
static float wilds_roll_tail,focus_lead_time,focus_strike_time;
static unsigned wilds_roll_x,wilds_roll_input;
static unsigned wilds_roll_magnitude,wilds_roll_heading;
enum { PAD_LOCK=0x4, PAD_R=0x200, PAD_FOCUS=0x400, PAD_X=0x1000, PAD_Y=0x8000 };
static struct {
    unsigned active,owner_state,owner_action,generation,state,action,flags;
} focus_native;
enum { RED_AURA_MAX=45*60, RED_AURA_HOLD=5*60, RED_AURA_SPEND=8*60, RED_AURA_REFUND=4*60 };
static unsigned red_rounds,red_iai_used;
static float red_aura_hold;
extern void ml_wilds_charge(unsigned);
extern void ml_wilds_focus(unsigned);
#endif
/* Included in the guarded native adapter. Private actions never enter GU's
 * stock weapon switch. Native hurt/scene requests retain their destination. */
static WState world;
#if ML_WILDS_MOVES
static NativeCall world_original_pad_map;
static NativeCall world_original_target_selection;
static u32 (*world_original_target_panel)(void *);
extern void ml_wilds_lock_hint(unsigned);

#if ML_RECOMP_RUNTIME
unsigned ml_hud_right_offset(void)
{
    static unsigned unsupported;
    if(unsupported)return 0;
    register u32 r0 __asm__("r0")=0;
    register u32 r1 __asm__("r1")=0x4D485549u;
    register u32 r2 __asm__("r2")=0;
    register u32 r3 __asm__("r3")=0;
    __asm__ volatile("svc #0x29":"+r"(r0),"+r"(r1),"+r"(r2),"+r"(r3)::"cc");
    if(r0||r2){unsupported=1;return 0;}
    return r1;
}
#else
unsigned ml_hud_native_crop(void)
{
    const void *renderer=(const void *)(uptr)word((const void *)main_base,0x2126998);
    if(!renderer)return 0;
    /* The game stores the size applied to nvnWindowSetCrop here. Forge's
     * ImGui DisplaySize tracks the full swapchain texture. */
    unsigned width=word(renderer,184),height=word(renderer,188);
    if(width<640||width>3840||height<360||height>2160||width*9!=height*16)return 0;
    return (height<<16)|width;
}
#endif

static void *world_local_hunter(void)
{
    void *players=(void *)(uptr)word((void *)main_base,0x1885b58);
    return players?((void *(*)(void *))(main_base+0x276278))(players):0;
}
static void *world_panel_node(const void *layout,unsigned index)
{
    unsigned split=word(layout,166*4);
    const void *nodes=(const void *)(uptr)word(layout,(split<=index?165:62)*4);
    if(split<=index)index-=split;
    return nodes?(void *)(uptr)word(nodes,4*index):0;
}
static u32 world_target_panel(void *panel)
{
    u32 result=world_original_target_panel(panel);
    void *players=(void *)(uptr)word((void *)main_base,0x1885b58);
    if(!players||!local_supported(((void *(*)(void *))(main_base+0x276278))(players)))return result;
    const void *layout=(const void *)(uptr)word(panel,36);
    if(!layout)return result;
    void *root=world_panel_node(layout,0x4c),*node=world_panel_node(layout,0x60);
    if(root&&node&&(word(root,84)&1)){
        ((void (*)(void *,unsigned,int))(main_base+0x56e528))(panel,0x60,0);
        ml_wilds_lock_hint(1);
    }
    return result;
}
static unsigned valor_guard_pad(unsigned bits);
static u32 world_pad_map(void *device,u32 meta,u32 pad_state,u32 sample)
{
    u32 result=world_original_pad_map(device,meta,pad_state,sample);
    u32 bits=word((void *)(uptr)sample,0);
    if(valor_guard_pad(bits)){bits|=PAD_R;put((void *)(uptr)sample,0,bits);}
    return result;
}
static u32 world_target_selection(void *camera,u32 b,u32 c,u32 d)
{
    void *a=world_local_hunter();
    uptr pad=*(uptr *)(main_base+0x2126888);
    if(!pad||!local_supported(a)||(int16_t)u16(a,HP)<=0)
        return world_original_target_selection(camera,b,c,d);

    /* Only the target selector sees the remapping. Let the engine keep its
     * normal LT and R3 words for the hunter, menus, and other camera code. */
    enum { GAMEPLAY_ZL=0x200, GAMEPLAY_R3=0x8000000 };
    void *buttons=(void *)(pad+0x6b9c);
    u32 saved[4];
    for(unsigned i=0;i<4;++i){
        saved[i]=word(buttons,4*i);
        u32 mapped=(saved[i]&GAMEPLAY_R3)?GAMEPLAY_ZL:0;
        put(buttons,4*i,(saved[i]&~(GAMEPLAY_ZL|GAMEPLAY_R3))|mapped);
    }
    u32 result=world_original_target_selection(camera,b,c,d);
    for(unsigned i=0;i<4;++i)put(buttons,4*i,saved[i]);
    return result;
}
#endif
#include "world_gauge_gain.h"
static unsigned world_normal_gauge_tenths;
static NativeCall world_original_gauge_gain;
#if ML_ASCENDANCE_PROTOTYPE
static unsigned ascendance_double_blade(void *a);
#endif
static u32 world_gauge_gain(void *a,u32 amount,u32 c,u32 d)
{
    int gain=(int)amount;
#if ML_ASCENDANCE_PROTOTYPE
    if(gain>0&&ascendance_double_blade(a))return 0;
#endif
#if ML_WILDS_MOVES
    if(gain<0&&word(a,0x3330)==3&&local_supported(a)&&(int16_t)u16(a,HP)>0)return 0;
#endif
    unsigned own=local_supported(a)&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        (int16_t)u16(a,HP)>0&&!world.move&&world_normal_gauge_action(u16(a,STATE),u16(a,ACTION));
    unsigned before=own?word(a,0x2780):0;
    if(own&&before<100)gain=world_normal_gauge_gain(gain,&world_normal_gauge_tenths);
    u32 result=world_original_gauge_gain(a,(u32)gain,c,d);
    if(own){
        if(word(a,0x2780)>=100)world_normal_gauge_tenths=0;
        if((int)amount>0)log_event("world_normal_gain",(u32)(uptr)a,amount,(u32)gain,(before<<16)|word(a,0x2780));
    }
    return result;
}
static unsigned world_loop_actor,world_loop_epoch,world_loop_action,world_loop_facing;
static void world_loop_heading(void *a)
{
    if(!world_loop_actor)return;
    if(world_loop_actor!=(u32)(uptr)a||world_loop_epoch!=byte(a,AREA)||
       u16(a,STATE)!=16||u16(a,ACTION)!=world_loop_action||(int16_t)u16(a,HP)<=0||world.move){
        world_loop_actor=0;return;
    }
    put(a,HEADING,world_loop_facing);put(a,TARGET,world_loop_facing);
}
#include "world_blend.h"
static float world_entry_blend;
static void *world_idle_actor;static unsigned world_idle_motion,world_idle_epoch;
static float world_entry_start,world_entry_speed=1;
#if ML_WORLD_INPUT_VM
static unsigned world_input_trace_sample,world_input_trace_actor,world_input_trace_flags[5];
static void world_input_trace(void *a,const WObservation *o)
{
    if(world_input_trace_sample==sample_sequence&&world_input_trace_actor==(u32)(uptr)a)return;
    if(world_input_trace_actor!=(u32)(uptr)a){
        world_input_trace_actor=(u32)(uptr)a;
        for(unsigned i=0;i<5;++i)world_input_trace_flags[i]=0;
        log_event("world_input_vm",(u32)(uptr)a,1,0,0);
    }
    world_input_trace_sample=sample_sequence;
    const WCommands *c=&world.controls.commands;
    static const unsigned commands[]={25,32,39,53,60};
    unsigned source=w_source(o->base.motion);
    if((o->state==1||o->state==2)&&o->base.native_action<=5)source=1;
    const WEvent *e=w_event(source,o->base.animation_frame);
    for(unsigned i=0;i<5;++i){
        unsigned id=commands[i],word=id/32,bit=1u<<(id%32);
        unsigned flags=((c->raw[word]&bit)!=0)|(((c->observed[word]&bit)!=0)<<1)|
            (((c->buffered[word]&bit)!=0)<<2)|(((c->executable[word]&bit)!=0)<<3)|
            (((c->raw_executable[word]&bit)!=0)<<4);
        unsigned p=world_command_bits[id];unsigned permission=e?(e->input[(p-1)/32]>>((p-1)%32))&3:0;
        unsigned record=flags|(o->base.held<<8)|(permission<<16);
        if(record==world_input_trace_flags[i])continue;
        world_input_trace_flags[i]=record;
        log_event("world_input_command",(u32)(uptr)a,id|(source<<8),
            record,(u32)(o->base.animation_frame*1000));
    }
}
#endif
/* Preserve a native solo request until its raw key has matured AND source
 * TIML permits execution. The raw-key clock already ran during recovery. */
enum { WORLD_R_WAIT_RELEASE=4 }; /* Physical button slots occupy 0..3. */
static struct {unsigned active,actor,epoch,motion,generation,action,flags,button,owner_action,owner_state;} world_r_gate;
#include "world_hits.h"
static unsigned world_bound,world_bound_actor,world_bound_action,world_last_sample;
#if ML_ASCENDANCE_PROTOTYPE
static float world_bound_attack_rate=1.f;
#endif
static float world_regen,world_helm_regen,world_regen_fraction;
static float world_regen_hud_frame;
static void (*world_original_spirit_hud)(void *);
static void world_spirit_hud(void *hud)
{
    extern void ml_gauge_glow(unsigned);
    ml_gauge_glow(0);
#if ML_WILDS_MOVES
    void *hunter=world_local_hunter();
    if(local_supported(hunter)&&hunter_style(hunter)==0&&word(hunter,0x3330)==3){
        float *clock=(float *)((uptr)hunter+0x332c);
        float remaining=*clock;
        uptr *vt=*(uptr **)hunter;
        unsigned rank=((unsigned (*)(void *))vt[0x808/4])(hunter);
        typedef float (__attribute__((pcs("aapcs-vfp"))) *AuraDuration)(void *,unsigned);
        float full=((AuraDuration)vt[0x804/4])(hunter,rank);
        if(remaining>0&&full>0)*clock=full*(remaining>RED_AURA_MAX?1:remaining/RED_AURA_MAX);
        world_original_spirit_hud(hud);
        *clock=remaining;
    }else
#endif
    world_original_spirit_hud(hud);
    float remaining=world_regen>world_helm_regen?world_regen:world_helm_regen;
    if(remaining<=0)return;
    void *a=(void *)(uptr)world.actor;
    if(!local_supported(a)||byte(a,AREA)!=world.epoch||(int16_t)u16(a,HP)<=0)return;
    /* Sampled warm white peak and envelope in the World training recording. */
    float frame=world_regen_hud_frame;
    float strength=frame<20?0:frame<25?(frame-20)*0.14f:
        frame<35?0.7f+(frame-25)*0.03f:frame<55?(55-frame)*0.05f:0;
    unsigned gauge=word(a,0x2780);
    if(strength>0&&gauge<=100)ml_gauge_glow((gauge<<8)|(unsigned)(strength*255));

}
static void (*world_original_timers)(void *);
static void world_timers(void *a)
{
    unsigned own=local_supported(a)&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        (int16_t)u16(a,HP)>0&&(world_regen>0||world_helm_regen>0);
    unsigned gauge=word(a,0x2780);float decay=real(a,0x3328);
#if ML_WILDS_MOVES
    unsigned red=word(a,0x3330)==3&&local_supported(a)&&(int16_t)u16(a,HP)>0;
    float aura=red?real(a,0x332c):0;
#endif
    world_original_timers(a);
    /* Native119eef0 contains passive gauge decay and other independent timers.
     * Source skips only that decay while either regeneration effect is active. */
    if(own){put(a,0x2780,gauge);*(float *)((uptr)a+0x3328)=decay;}
#if ML_WILDS_MOVES
    if(red&&word(a,0x3330)==3&&red_aura_hold>0){
        float elapsed=aura-real(a,0x332c);
        if(elapsed>0){
            float held=elapsed<red_aura_hold?elapsed:red_aura_hold;
            *(float *)((uptr)a+0x332c)+=held;
            red_aura_hold-=held;
        }
    }
#endif
}
static int world_hit=-1;
static unsigned world_attack_id,world_attack_actor,world_attack_owned,world_contact_seen;
static unsigned world_counter_seen;
static void set_real(void *a,u32 at,float x){*(float *)((uptr)a+at)=x;}
#if ML_WILDS_MOVES
static void world_red_time(void *a,int change,unsigned source)
{
    if(word(a,0x3330)!=3)return;
    float before=real(a,0x332c),after=before+change;
    /* Native only drops the rank when its positive clock crosses zero. */
    if(after<=1){after=1;red_aura_hold=0;}
    if(after>RED_AURA_MAX)after=RED_AURA_MAX;
    set_real(a,0x332c,after);
    log_event("wilds_red_time",(u32)(uptr)a,source,(u32)before,(u32)after);
}
#endif
#if ML_WILDS_MOVES
static void world_focus_walk_pose(void *a);
#endif
#if ML_ASCENDANCE_PROTOTYPE
static unsigned ascendance_dash_floor(void *a,unsigned before_wall);
static float ascendance_pursuit_floor(void *a,unsigned before_wall);
static void ascendance_step_floor(void *a);
static unsigned ascendance_resheathe_roll_active(void *a);
#endif
#include "world_physics.inc"
#if ML_ASCENDANCE_PROTOTYPE
#include "world_ascendance.inc"
#endif
typedef float (__attribute__((pcs("aapcs-vfp"))) *AttackScale)(void *);
static AttackScale world_original_attack_scale;
static float __attribute__((pcs("aapcs-vfp"))) world_attack_scale(void *collider)
{
    float scale=world_original_attack_scale(collider);
#if ML_ASCENDANCE_PROTOTYPE
    scale*=ascendance_attack_scale(collider);
#endif
    if(world.move==W_FORESIGHT&&world_attack_owned&&world.foresight_gauge&&
       (world.counter_success||world_counter_seen)&&
       (uptr)collider==(uptr)world_attack_actor+0x3020&&u16(collider,0x34)==world_attack_id)
        scale*=1.3f; /* Source wp03_param offset1317, native physical scale. */
    return scale;
}
#if ML_WILDS_MOVES
static int (*world_original_open_hit)(void *,unsigned,unsigned,unsigned);
static int world_open_hit(void *a,unsigned slot,unsigned row,unsigned flags)
{
    unsigned replacement=row;
    if(slot==0&&u16(a,STATE)==16&&local_supported(a)){
        unsigned action=u16(a,ACTION);
        for(unsigned i=0;i<sizeof(wilds_spirit_rows)/sizeof(*wilds_spirit_rows);++i){
            const WildsSpiritRow *spirit=wilds_spirit_rows+i;
            if(row==spirit->donor&&action>=spirit->first&&action<=spirit->last){
                const void *res=(const void *)(uptr)word(a,0x3198);
                const void *hdp=res?(const void *)(uptr)word(res,0x64):0;
                if(hdp&&word(hdp,8)>spirit->row)replacement=spirit->row;
                break;
            }
        }
    }
    return world_original_open_hit(a,slot,replacement,flags);
}
#endif
static u32 (*world_original_damage)(void *);
static u32 (*world_original_hp)(void *,int,void *,u32);
static float world_armor;
static unsigned world_damage_depth;
static unsigned world_super_depth;
#if ML_WILDS_MOVES
static unsigned world_action_armor_depth;
#endif
static u32 (*world_original_flinch_resist)(void *);
static u32 world_flinch_resist(void *a)
{
    /* GU 2ee508 asks this predicate only for its native light reactions.
     * Retain its subsequent KO, status and heavy-reaction decisions. */
    if(world_super_depth && a==(void *)(uptr)world.actor)return 1;
    return world_original_flinch_resist(a);
}
#if ML_WORLD_PRESENTATION
static void world_audio_epv(void *,unsigned,const float *);
static void world_audio_blade(void *,const float *);
#if ML_WILDS_MOVES
enum WAudioImpact { W_SOUND_CRIMSON_FINISH,W_SOUND_BLADE,W_SOUND_THRUST_BLADE,W_SOUND_HELM_BLADE,
    W_SOUND_RELEASE_BLADE,W_SOUND_RELEASE_FOLLOW,W_SOUND_HELM_FOLLOW,W_SOUND_IAI_FOLLOW,
    W_SOUND_LINGYUE_CUT,W_SOUND_LINGYUE_FINISH,W_SOUND_LINGYUE_RAPID };
static unsigned world_audio_impact(unsigned,enum WAudioImpact,unsigned,const float *);
static unsigned world_helm_wind_played;
#else
static void world_audio_tail(void *,float,const float *);
#endif
#endif
#if ML_WORLD_PRESENTATION
static void world_big_counter_flash(void *);
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
static void world_radiance_contact_fx(void *,const void *);
static void world_radiance_overlay(void *,void *);
#endif
#endif
#include "world_detector.inc"
#if ML_WILDS_MOVES
static void world_iai_contact_entry(void *a);
#if ML_WORLD_INPUT_VM
static unsigned world_foresight_contact_entry(void *a);
static unsigned crimson_available(void *a);
#endif
#endif
static u32 world_hp(void *a,int damage,void *result,u32 flags)
{
    int original=damage;unsigned before=u16(a,HP);
    /* Native2F0C9C receives a signed HP delta: its callers pass -damage.
     * Keep healing untouched and retain at least one point of real damage. */
    if(world_damage_depth&&a==(void *)(uptr)world.actor&&world_armor>0&&damage<0)
        damage=damage<-1?damage/2:-1;
    u32 value=world_original_hp(a,damage,result,flags);
    if(a==(void *)(uptr)world.actor&&original<0)
        log_event("world_hp_hit",(u32)(uptr)a,(u32)original,(u32)damage,(before<<16)|u16(a,HP));
    return value;
}
static u32 world_damage(void *a)
{
    if(!local_supported(a)||world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||
       (int16_t)u16(a,HP)<=0)return world_original_damage(a);
    unsigned packet=byte(a,0x31e0),type=byte(a,0x3216);
    unsigned eligible=packet&&(int16_t)u16(a,0x321c)>=0&&world_counter_type(type);
    if(packet&&world.move)log_event("world_incoming",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type|(eligible<<16));
    unsigned super=0;
    unsigned foresight_started=0;
#if ML_WILDS_MOVES
    /* Keep Focus armor active across the two native motion rebinds. */
    unsigned action_armor=eligible&&world.move==W_FOCUS&&world.wilds_focus_break&&
        u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_FOCUS);
    /* The engine can poll HID twice in a display frame around collision. */
    unsigned monster_body=world_body_actor==(u32)(uptr)a&&world_body_epoch==world.epoch&&
        sample_sequence-world_body_sample<=4;
    world_body_actor=0;
    if(eligible&&monster_body&&world.move==W_SHEATHE)world_iai_contact_entry(a);
#if ML_WORLD_INPUT_VM
    else if(eligible&&monster_body&&(!world.move||world.move==W_CHIJIN))
        foresight_started=world_foresight_contact_entry(a);
#endif
#if ML_ASCENDANCE_PROTOTYPE
    unsigned musou_active=musou.phase!=MUSOU_NONE;
#else
    unsigned musou_active=0;
#endif
    if(eligible&&!musou_active&&(world.move==W_SMALL||world.move==W_BIG)&&u16(a,STATE)==16){
        unsigned action=u16(a,ACTION),motion=u16(a,MOTION);
        unsigned bound=world_bound&&action==w_action(world.move)&&motion==0x500+world.slot;
        unsigned startup=!world_bound&&(action==w_action(world.move)||
            (action==w_action(W_SHEATHE)&&motion==0x500+w_clip(306)->slot));
#if ML_ASCENDANCE_PROTOTYPE
        unsigned guard=startup||(bound&&world_detectors[0].remaining>0);
        if(ascendance_independent_flash())
            guard=startup||(bound&&real(a,FRAME)<RADIANCE_FLASH_COUNTER_END);
        if(dividing_slot(world.slot)||ascendance.double_cut||radiance_pursuit_pose(motion))guard=0;
        if(guard){
#else
        if(startup||(bound&&world_detectors[0].remaining>0)){
#endif
            if(monster_body)world_counter_accept(a,type);
            *(unsigned char *)((uptr)a+0x31e0)=0;
            *(uint16_t *)((uptr)a+0x3226)=0;
            log_event("wilds_iai_guard",world.actor,world.source,startup,type);
        }
    }
#endif
    if(eligible&&world.move&&world_bound&&world_bound_actor==(u32)(uptr)a&&u16(a,STATE)==16&&
       u16(a,ACTION)==w_action(world.move)&&u16(a,MOTION)==0x500+world.slot){
        float f=real(a,FRAME);const WEvent *e=w_event(world.source,f);
        /* Original PlMotionCommon::mFlag bit25 feeds player+8948 bit22.
         * Read the animation key at contact so recovery loses armor on time. */
        super=world.move!=W_GROUND&&e&&(e->common&0x02000000u);
#if ML_WILDS_MOVES
        /* Wilds run-up window on the retained World thrust animation. */
#if ML_ASCENDANCE_PROTOTYPE
        if(world.move==W_THRUST&&ascendance.radiance==RADIANCE_CHASE)super=0;
        else
#endif
        if(world.move==W_THRUST&&f<191)super=1;
        action_armor|=world.move==W_RELEASE&&f<W_RELEASE_RECOVERY_CANCEL;
#endif
        unsigned iai=(world.move==W_SMALL||world.move==W_BIG)&&e&&(e->common&0x04000000u);
#if ML_ASCENDANCE_PROTOTYPE
        if(musou_active){iai=0;super=musou.phase!=MUSOU_FLURRY&&f<30;}
        if(dividing_slot(world.slot)||ascendance.double_cut||ascendance_independent_flash()||radiance_pursuit_pose(u16(a,MOTION))){iai=0;super=0;}
#endif
        unsigned foresight=world_foresight_invulnerable(f);
        if(iai||foresight){
            if(foresight&&foresight_started){
                world_counter_accept(a,type);
                *(uint16_t *)((uptr)a+0x3226)=0;
            }
            if(foresight)log_event("wilds_foresight_evade",world.actor,world.source,(u32)f,type);
            *(unsigned char *)((uptr)a+0x31e0)=0;
        }
    }
#if ML_WILDS_MOVES
    super|=action_armor;
#endif
    unsigned armored=eligible&&world_armor>0;
    /* Native collision37694c adds attack+56 to the pending signed halfword
     *3226. Damage2f38f8 feeds it to2f42d0 (KO accumulator2898, threshold50).
     * World PLP Hyper_Armor_Stun_Damage_Rate is0. Suppress only this packet;
     * do not clear existing KO buildup, its decay clock, or other statuses. */
    unsigned incoming_stun=u16(a,0x3226),stun_before=word(a,0x2898);
    if(armored||super)*(uint16_t *)((uptr)a+0x3226)=0;
    if(super)log_event("world_super_hit",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type);
    world_super_depth+=super;
    world_damage_depth+=armored;
#if ML_WILDS_MOVES
    world_action_armor_depth+=action_armor;
#endif
    u32 result=world_original_damage(a);
#if ML_WILDS_MOVES
    world_action_armor_depth-=action_armor;
#endif
    world_damage_depth-=armored;
    world_super_depth-=super;
    if(eligible&&incoming_stun)
        log_event("world_stun",(u32)(uptr)a,incoming_stun|(armored<<16)|(super<<17),stun_before,word(a,0x2898));
    return result;
}
static void world_close_hit(void *a)
{
    if(world_attack_owned&&world_attack_actor==(u32)(uptr)a&&u16(a,0x3054)==world_attack_id)
        *(unsigned char *)((uptr)a+0x3025)=0;
    world_attack_owned=0;world_hit=-1;world_contact_seen=0;
}
static unsigned world_contact(void *a)
{
    if(!world_attack_owned||world_attack_actor!=(u32)(uptr)a||u16(a,0x3054)!=world_attack_id||
       byte(a,0x30d4)!=1||!word(a,0x30cc))return 0;
    unsigned target=word(a,0x30d0);
    void *manager=(void *)(uptr)*(u32 *)(main_base+0x1851fe0);
    if(!target||!manager)return 0;
    /* Validate ownership through the live registry before following a pointer
     * retained by last frame's collision result. */
    for(unsigned i=0;i<10;++i)if(word(manager,0x1c+4*i)==target)
        return word((void *)(uptr)target,0x1428)!=0&&byte((void *)(uptr)target,AREA)==byte(a,AREA);
    return 0;
}
#if ML_WORLD_PRESENTATION
static void world_helm_impact(void *,const float *);
static void world_helm_line(void *,const float *);
static void world_sfx_emit(void *,const float *,unsigned);
#endif
#include "world_follow.inc"
#if ML_WILDS_MOVES
#include "world_wounds.inc"
#endif
#include "world_camera.inc"
#if ML_WILDS_MOVES
static u32 (*world_original_turn)(void *);
static struct {
    unsigned actor,epoch,action,heading,target,aimed;
} focus_spirit;

static struct {
    void *actor;
    unsigned epoch,heading,moving,started,pressed,sampled,owner;
    float travel;
} valor_third;

static unsigned valor_third_pose(void *a)
{
    return u16(a,STATE)==16&&u16(a,ACTION)==117&&u16(a,MOTION)==0x5ba&&valor_active(a);
}

static float valor_step_curve(float frame,float start,float end)
{
    if(frame<=start)return 0;
    if(frame>=end)return 1;
    float t=(frame-start)/(end-start);
    return t*t*(3-2*t);
}

static void valor_third_step(void *a)
{
    if(world.actor!=(u32)(uptr)a)return;
    unsigned action=u16(a,ACTION);
    if(u16(a,STATE)!=16||!(action==116||action==117||(action>=134&&action<=141))||
       !valor_active(a)){
        valor_third.actor=0;
        return;
    }
    if(valor_third.actor!=a||valor_third.epoch!=byte(a,AREA)||
       (action!=117&&valor_third.owner!=action))
        valor_third=(__typeof__(valor_third)){.actor=a,.epoch=byte(a,AREA),.owner=action};
    if(action!=117){
        unsigned pressed=held==ML_R;
        if(pressed&&!valor_third.pressed){
            MLObservation input=observe(a,1);
            valor_third.sampled=1;valor_third.moving=input.magnitude>=50;
            valor_third.heading=input.input_heading;
        }
        valor_third.pressed=pressed;
        return;
    }
    if(u16(a,MOTION)!=0x5ba)return;
    if(!valor_third.started){
        if(!valor_third.sampled){
            MLObservation input=observe(a,1);
            valor_third.moving=input.magnitude>=50;valor_third.heading=input.input_heading;
        }
        valor_third.started=1;valor_third.owner=117;
    }
    if(!valor_third.moving)return;
    float frame=real(a,FRAME);
    float travel=56*valor_step_curve(frame,3,44)+40*valor_step_curve(frame,50,84);
    float delta=travel-valor_third.travel;
    if(delta<=0)return;
    valor_third.travel=travel;
    unsigned yaw=valor_third.heading;
    set_real(a,0x40,real(a,0x40)+world_sin(yaw)*delta);
    set_real(a,0x48,real(a,0x48)+world_sin(yaw+16384)*delta);
}

/* Entry aim may reverse on a single buffered input. Advance the actor itself
 * so the posed blade, travel direction and rendered heading stay in step. */
typedef struct { unsigned actor,epoch,source,target,yaw,sample; } WFacingTurn;
static WFacingTurn entry_turn,roll_turn,focus_thrust_turn,release_walk_turn;
static unsigned entry_turn_delay;
static unsigned release_walk_input;
static unsigned world_turn_step(void *a,WFacingTurn *turn,unsigned follow)
{
    if(turn->sample!=sample_sequence){
        turn->sample=sample_sequence;
        float frames=real(a,0x1c);
        if(frames>2)frames=2;
        if(frames>0){
            int limit=(int)(2184.53f*frames); /* 12 degrees per source frame */
            int delta=ml_heading_delta(turn->target,turn->yaw);
            if(delta>limit)delta=limit;
            else if(delta< -limit)delta= -limit;
            turn->yaw=(turn->yaw+delta)&65535u;
        }
    }
    if(!follow&&turn->yaw==turn->target)turn->actor=0;
    return turn->yaw;
}
static void world_release_walk_step(void *a)
{
    WFacingTurn *turn=&release_walk_turn;
    if(turn->actor!=(u32)(uptr)a)return;
    MLObservation input=observe(a,1);
    if(turn->epoch!=input.epoch||input.magnitude<50||(word(a,AIR)&0x400)){
        turn->actor=0;return;
    }
    if(u16(a,STATE)!=2||input.native_action!=4||input.motion!=42){
        if(turn->source||sample_sequence-turn->sample>3)turn->actor=0;
        return;
    }
    if(!turn->source){
        /* The native start selects its heading, then the turn eases toward it. */
        turn->source=input.motion;
        turn->target=input.heading;
        release_walk_input=input.input_heading;
    }else{
        turn->target=(turn->target+ml_heading_delta(input.input_heading,release_walk_input))&65535u;
        release_walk_input=input.input_heading;
    }
    unsigned yaw=world_turn_step(a,turn,0);
    put(a,HEADING,yaw);
    put(a,TARGET,yaw);
}
static unsigned world_focus_thrust_heading(void *a,unsigned *aim)
{
    if(!world_bound||world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||
       u16(a,STATE)!=16||u16(a,ACTION)!=w_action(world.move)||u16(a,MOTION)!=0x500+world.slot||
       (world.source!=170&&world.source!=905))return 0;

    WFacingTurn *turn=&focus_thrust_turn;
    unsigned owned=turn->actor==(u32)(uptr)a&&turn->epoch==byte(a,AREA)&&turn->source==world.source;
    /* Track the current camera through wind-up, then keep the strike on
     * the heading at its first active frame (170:84, 905:34). */
    if(real(a,FRAME)<(world.source==170?84.f:34.f)){
        unsigned target;
        if((ml_pad_raw_bits()&PAD_FOCUS)&&world_camera_heading(a,&target)){
            if(!owned){
                *turn=(WFacingTurn){.actor=(u32)(uptr)a,.epoch=byte(a,AREA),.source=world.source,
                    .yaw=u16(a,HEADING),.sample=sample_sequence-1};
                owned=1;
            }
            turn->target=target;
        }
        if(owned)world_turn_step(a,turn,1);
    }
    if(owned)*aim=turn->yaw;
    return owned;
}

static unsigned crimson_split_heading,crimson_travel_heading;
static unsigned world_crimson_bound(void *a)
{
    return world.move==W_CHIJIN&&world_bound&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_CHIJIN)&&u16(a,MOTION)==0x500+world.slot;
}

static unsigned world_focus_walking(void *a,unsigned *aim)
{
    return !world.move&&u16(a,STATE)==2&&u16(a,ACTION)==0&&(byte(a,DRAWN)&1)&&
        !(word(a,AIR)&0x400)&&world_focus_heading(a,aim);
}

/* A locked attack can spend many frames facing away while the camera
 * keeps turning. Start from the body's current yaw when control opens. */
static unsigned world_focus_body_step(void *a,unsigned target)
{
    static unsigned actor,epoch,sample,origin;
    if(actor!=(u32)(uptr)a||epoch!=byte(a,AREA)||sample!=sample_sequence){
        actor=(u32)(uptr)a;epoch=byte(a,AREA);sample=sample_sequence;
        origin=u16(a,HEADING);
    }
    float frames=real(a,0x1c);
    if(!(frames>0))return origin;
    if(frames>2)frames=2;
    int limit=(int)(1456.35f*frames),delta=ml_heading_delta(target,origin);
    if(delta>limit)delta=limit;
    else if(delta< -limit)delta= -limit;
    return (origin+delta)&65535u;
}
static unsigned world_focus_air_heading(void *a,unsigned *aim)
{
    unsigned active=world.move>=W_RISE&&world.move<=W_DOWN&&world_bound&&world_air_owner(a)&&
        world.actor==(u32)(uptr)a&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move)&&
        u16(a,MOTION)==0x500+world.slot&&world_focus_heading(a,aim);
    if(active)*aim=world_focus_body_step(a,*aim);
    return active;
}

static void world_focus_walk_pose(void *a)
{
    valor_third_step(a);
    unsigned aim;
    /* Root travel has already used the committed stick direction.
     * Refresh the visible pose for floor/collision and presentation. */
    if(roll_turn.actor==(u32)(uptr)a){
        if(roll_turn.epoch==byte(a,AREA)&&u16(a,STATE)==1&&u16(a,ACTION)==6){
            aim=world_turn_step(a,&roll_turn,0);
            put(a,HEADING,aim);
            ((void (*)(void *))(main_base+0x53a9fc))(a);
            put(a,HEADING,roll_turn.target);
            return;
        }
        if(sample_sequence-roll_turn.sample>2||roll_turn.epoch!=byte(a,AREA))roll_turn.actor=0;
    }
    if(world_crimson_bound(a))aim=world.heading;
    else if(world_focus_thrust_heading(a,&aim)||world_focus_air_heading(a,&aim))world.heading=aim;
    else if(focus_spirit.actor==(u32)(uptr)a&&focus_spirit.epoch==byte(a,AREA)&&
            u16(a,STATE)==16&&focus_spirit.action==u16(a,ACTION)&&
            (focus_spirit.action==26||focus_spirit.action==27||valor_third_pose(a))&&focus_spirit.aimed)
        aim=focus_spirit.heading;
    else if(!world_focus_walking(a,&aim))return;
    /* The motion clock has already integrated travel. Refresh the
     * native orientation before collision and the render pose. */
    put(a,HEADING,aim);put(a,TARGET,aim);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
}

static void world_focus_walk_motion(void *a,unsigned active,unsigned aim)
{
    unsigned motion=u16(a,MOTION);
    const WildsClip *previous=focus_walk_clip(motion);
    if(motion!=0x504&&!previous)return;

    unsigned slot=4;
    if(active){
        int angle=ml_heading_delta(u16(a,TARGET),aim);
        if(angle>24576||angle< -24576)slot=FOCUS_WALK_BACK;
        else if(angle>8192)slot=FOCUS_WALK_LEFT;
        else if(angle< -8192)slot=FOCUS_WALK_RIGHT;
    }
    if(motion==0x500+slot)return;
    void *resource=(void *)(uptr)word(a,0xf04+5*4);
    const void *bank=resource?(const void *)(uptr)word(resource,0x64):0;
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<=slot||!word(bank,8+4*slot))return;

    float phase=real(a,FRAME)/(previous?(previous->frames-1):50);
    if(!(phase>=0&&phase<1))phase=0;
    const WildsClip *next=focus_walk_clip(0x500+slot);
    float start=phase*(next?(next->frames-1):50);
    original_layer(a,0,5000+slot,0x81,5,start,1);
    log_event("wilds_focus_walk",(u32)(uptr)a,slot,motion,(u32)start);
}

static unsigned world_ready(void *a);
/* Blend the planted hunter into one lateral step while pivoting, then
 * return to the drawn idle. The native action still accepts movement and attacks. */
static void world_focus_pivot_pose(void *a,unsigned state,unsigned action)
{
    static unsigned owner,epoch,slot;
    unsigned actor=(u32)(uptr)a,motion=u16(a,MOTION);
    unsigned owns=owner==actor&&epoch==byte(a,AREA)&&motion==0x500+slot;
    if(world.move||state!=1||action!=0||!(byte(a,DRAWN)&1)){
        owner=0;return;
    }
    unsigned facing;
    int gap=world_focus_heading(a,&facing)?ml_heading_delta(focus_camera_yaw,facing):0;
    if(owns){
        if(gap>546||gap< -546)return;
        original_layer(a,0,5001,0x81,5,0,1);
        owner=0;
    }else if(motion==0x501&&(gap>8192||gap< -8192)&&world_ready(a)){
        slot=gap>0?FOCUS_WALK_LEFT:FOCUS_WALK_RIGHT;
        original_layer(a,0,5000+slot,0x81,4,0,1);
        owner=actor;epoch=byte(a,AREA);
    }
}
static u32 world_focus_turn(void *a)
{
    u32 result=world_original_turn(a);
    if(!local_supported(a))return result;
    if(world_crimson_bound(a)){
        /* Evaluate directional root motion before restoring the aimed pose. */
        unsigned heading=crimson_split_heading?crimson_travel_heading:world.heading;
        put(a,HEADING,heading);put(a,TARGET,heading);
        return result;
    }
    unsigned state=u16(a,STATE),action=u16(a,ACTION),aim;
    if(roll_turn.actor==(u32)(uptr)a&&roll_turn.epoch==byte(a,AREA)&&state==1&&action==6){
        put(a,HEADING,roll_turn.target);put(a,TARGET,roll_turn.target);
        return result;
    }
    if(world_focus_thrust_heading(a,&aim)){
        world.heading=aim;
        put(a,HEADING,aim);put(a,TARGET,aim);
        return result;
    }
    world_focus_pivot_pose(a,state,action);
    unsigned spirit=state==16&&((action>=16&&action<=19)||action==23||action==24||
        action==26||action==27||action==30||valor_third_pose(a));
    if(spirit){
        unsigned fresh=focus_spirit.actor!=(u32)(uptr)a||focus_spirit.epoch!=byte(a,AREA)||focus_spirit.action!=action;
        if(fresh){
            focus_spirit.actor=(u32)(uptr)a;focus_spirit.epoch=byte(a,AREA);focus_spirit.action=action;
            focus_spirit.aimed=world_focus_heading(a,&focus_spirit.target);
            if(focus_spirit.aimed)
                log_event("wilds_focus_spirit",(u32)(uptr)a,action,focus_spirit.target,u16(a,MOTION));
        }
        if(action==117){
            focus_spirit.aimed=world_focus_heading(a,&focus_spirit.target);
            if(!focus_spirit.aimed&&valor_third.actor==a&&valor_third.moving&&valor_third.epoch==byte(a,AREA)){
                focus_spirit.aimed=1;
                focus_spirit.target=valor_third.heading;
            }
        }else if((action==26||action==27)&&world_focus_heading(a,&aim)){
            focus_spirit.aimed=1;
            focus_spirit.target=aim;
        }
        if(focus_spirit.aimed){
            focus_spirit.heading=world_focus_body_step(a,focus_spirit.target);
            put(a,HEADING,focus_spirit.heading);put(a,TARGET,focus_spirit.heading);
        }
    }else{
        if(focus_spirit.actor==(u32)(uptr)a)focus_spirit.actor=0;
        if(world_focus_air_heading(a,&aim)){
            world.heading=aim;
            put(a,HEADING,aim);put(a,TARGET,aim);
            return result;
        }
        unsigned walking=world_focus_walking(a,&aim)&&(int16_t)u16(a,0x1410)>0;
        if(state==2&&action==0)world_focus_walk_motion(a,walking,walking?aim:0);
        if(walking){
            /* GU's native turn target has already resolved the local stick
             * against the camera. Use it for this frame's native root step. */
            put(a,HEADING,u16(a,TARGET));
        }
    }
    return result;
}
#endif
#include "world_audio.inc"
#if ML_WORLD_PRESENTATION
#include "world_simple_fx.inc"
#else
#include "world_effects.inc"
#endif
static unsigned world_ready(void *a)
{
    void *r=(void *)(uptr)word(a,0xf04+20);
    void *bank=r?(void *)(uptr)word(r,0x64):0;
    void *h=(void *)(uptr)word(a,0x3198),*s=(void *)(uptr)word(a,0x319c);
    void *hdp=h?(void *)(uptr)word(h,0x64):0,*hts=s?(void *)(uptr)word(s,0x64):0;
#if ML_WILDS_MOVES
    if(!bank||word(bank,0)!=0x00544d4c||u16(bank,6)<252||!hdp||!hts||
       word(hdp,8)<WILDS_HIT_COUNT||word(hts,8)<=WILDS_SHAPE)return 0;
    for(unsigned i=0;i<sizeof(wilds_clips)/sizeof(*wilds_clips);++i)
        if(!word(bank,8+wilds_clips[i].slot*4))return 0;
#endif
    return bank&&word(bank,0)==0x00544d4c&&u16(bank,6)>246&&word(bank,8+246*4)&&
        hdp&&word(hdp,0)==0x00504448&&word(hdp,8)>=WORLD_HIT_COUNT&&
        hts&&word(hts,0)==0x00535448&&word(hts,8)>=WORLD_SHAPE_COUNT;
}
#if ML_WILDS_MOVES && ML_WORLD_INPUT_VM
static unsigned world_focus_native_input(void *a,unsigned state,unsigned action,unsigned flags)
{
    if(world.move||world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||
       !(byte(a,DRAWN)&1)||(word(a,AIR)&0x400)||(int16_t)u16(a,HP)<=0)return 0;
    unsigned raw=ml_pad_raw_bits(),edge=raw&~wilds_input_previous;
    if(!(raw&PAD_FOCUS))return 0;
    unsigned owner_state=u16(a,STATE),owner_action=u16(a,ACTION);
    unsigned source=w_guild_source(owner_state,owner_action,u16(a,MOTION));
    unsigned valor_style=hunter_style(a)==5;
    unsigned cut=valor_style&&owner_state==16?w_valor_draw_cancel(owner_action,u16(a,MOTION)):0;
    unsigned blue=valor_style&&valor_active(a);
    unsigned allowed=blue?w_valor_chord(owner_state,owner_action,u16(a,MOTION),real(a,FRAME),ML_R|ML_Y,1):
        cut?real(a,FRAME)>=cut:source==1||(source&&real(a,FRAME)>=24);
    if(!allowed||!world_ready(a))return 0;
    unsigned spirit=state==16&&((action>=16&&action<=19)||action==23||action==24||
        action==26||action==27||action==30);
    unsigned sheath=state==1&&action==2;
    if(held==(ML_R|ML_Y))return spirit||sheath||(blue&&state==16&&
        (action==116||action==117||action==130||(action>=134&&action<=141)));
    if(blue&&held==ML_R)return 0;
    unsigned button=held==ML_R&&spirit?PAD_R:held==ML_Y&&sheath?PAD_Y:0;
    if(!button||(!(edge&button)&&focus_lead_time<=0))return 0;
    if(!focus_native.active){
        focus_native.active=1;focus_native.owner_state=owner_state;focus_native.owner_action=owner_action;
        focus_native.generation=bind_generation;focus_native.state=state;focus_native.action=action;
        focus_native.flags=flags;
    }
    return 1;
}
static unsigned world_focus_replay(void *a,const WObservation *o,WDecision d)
{
    if(!focus_native.active)return 0;
    if(d.kind!=W_KEEP||world.move||!o->alive||!o->drawn||!o->grounded||
       u16(a,STATE)!=focus_native.owner_state||u16(a,ACTION)!=focus_native.owner_action||
       bind_generation!=focus_native.generation){
        focus_native.active=0;return 0;
    }
    if(focus_lead_time>0)return 0;
    focus_native.active=0;world_r_gate.active=0;
    unsigned aim;
    if(focus_native.state==16&&world_focus_heading(a,&aim)){
        put(a,HEADING,aim);put(a,TARGET,aim);
    }
    ++submitting;virtual_request(a,focus_native.state,focus_native.action,focus_native.flags);--submitting;
    return 1;
}
#endif
#if ML_WILDS_MOVES
static void (*world_original_rank_delta)(void *,int);
static struct {void *actor; unsigned epoch;} valor_foresight_round;
static void world_rank_delta(void *a,int delta)
{
    if(delta>0&&valor_foresight_round.actor==a&&valor_foresight_round.epoch==byte(a,AREA)&&
       u16(a,STATE)==16&&u16(a,ACTION)==30){
        log_event("valor_foresight_round_no_rank",(u32)(uptr)a,30,(unsigned)delta,0);
        return;
    }
    world_original_rank_delta(a,delta);
}
#endif
static void world_pay(void *a,WDecision d)
{
    unsigned spend=1;
#if ML_WILDS_MOVES
    if(word(a,0x3330)==3){
        spend=0;
        if(d.rank_delta>0)d.rank_delta=0;
        if((d.red_recovery&W_RED_ROUND)&&red_rounds<3){
            static const int recover[]={10*60,5*60,3*60};
            if(!red_rounds)red_aura_hold=RED_AURA_HOLD;
            world_red_time(a,recover[red_rounds++],W_RED_ROUND);
        }
        if((d.red_recovery&W_RED_IAI)&&!red_iai_used){
            red_iai_used=1;world_red_time(a,10*60,W_RED_IAI);
        }
    }
#endif
    if(d.rank_delta)((void (*)(void *,int))(main_base+0x119ed98))(a,d.rank_delta);
#if ML_WILDS_MOVES
    if(d.valor_gain&&hunter_style(a)==5&&!valor_active(a)){
        unsigned before=word(a,0x2790);
        ((void (*)(void *,int))(main_base+0x2a33a8))(a,(int)d.valor_gain);
        log_event("wilds_valor_reward",(u32)(uptr)a,before,word(a,0x2790),d.valor_gain);
    }
#endif
    /* Native gauge subtraction can refuse during GU's no-cost timer. Foresight
     * spends the whole source gauge, including its full-gauge glow timer. */
    if(d.gauge_clear&&spend){put(a,0x2780,0);set_real(a,0x3334,0);}
    if(d.gauge_delta>0||(d.gauge_delta<0&&spend)){
        int next=(int)word(a,0x2780)+d.gauge_delta;put(a,0x2780,next<0?0:next>100?100:(unsigned)next);
    }
    if(d.gauge_fill)((void (*)(void *,int,int))(main_base+0x2a309c))(a,100,-1);
    if(d.regen_seconds){
        world_regen=(float)d.regen_seconds;
        if(world.source==918)log_event("valor_foresight_regen",(u32)(uptr)a,918,d.regen_seconds,0);
    }
}
#if ML_WILDS_MOVES
static unsigned wilds_root_owned;
#include "wilds_root_data.h"
#endif
static void world_cleanup(void *a)
{
#if ML_WILDS_MOVES
    if(wilds_root_owned){
        set_real(a,0x1010,0);set_real(a,0x1018,0);wilds_root_owned=0;
    }
#endif
#if ML_ASCENDANCE_PROTOTYPE
    if(!world.move&&ascendance.actor==a){
        if(musou.phase)log_event("ascendance_musou_interrupt",(u32)(uptr)a,musou.phase,musou.cycles,musou.hits);
        musou=(__typeof__(musou)){0};
        ascendance.radiance=RADIANCE_OFF;
        ascendance.enhanced_iai=ascendance.sheathe=ascendance.musou_sheathe_r=0;
        radiance_pursuit.target=0;
    }
    if(ascendance_velocity_actor==a){
        put(a,0x1010,0);put(a,0x1018,0);
        ascendance_velocity_actor=0;
    }
#endif
    world_close_hit(a);world_counter_seen=0;world_detector_clear();
#if ML_WILDS_MOVES
    ml_wilds_charge(0);
#endif
    if(world_bound&&world_bound_actor==(u32)(uptr)a){
        set_real(a,0x50c,1);set_real(a,0x14b0,1);
        for(unsigned i=0;i<4;++i)put(a,0x1100+4*i,0);
    }
    world_bound=0;
}
static float world_motion_speed(void *a,const WEvent *e)
{
#if ML_WILDS_MOVES
    /* MOT279 settles into its release stance at frame 180. */
    if(world.source==908||world.source==915)
        return (W_CHARGE_READY_FRAME/(60.f*W_CHARGE_READY_SECONDS))*w_charge_rate(&world);
#endif
#if ML_ASCENDANCE_PROTOTYPE
    if(world.move==W_SMALL&&dividing_slot(world.slot))return 1;
    if(world.move==W_SHEATHE&&(world.slot==32||musou_stance_motion(0x500+world.slot)))return 1;
    if(musou.phase==MUSOU_CUT&&world.slot==40)return 1;
    if(musou.phase==MUSOU_DASH&&world.slot>=33&&world.slot<=39)return 1;
    if(musou.phase==MUSOU_FLURRY&&musou_spin_slot(world.slot))return .66f;
    if(u16(a,MOTION)==0x500+DOUBLE_DRAW_RETURN){
        /* GU advances the current sample with the old speed before consuming this rate. */
        float dt=real(a,0x1c);
        return double_resheathe_speed(real(a,FRAME)+real(a,0x50c)*dt,dt);
    }
    if(musou.phase==MUSOU_SEAT||musou.phase==MUSOU_ENTRY||musou_path_slot(world.slot)||double_draw_motion(u16(a,MOTION)))return 1;
    if(radiance_pursuit_pose(u16(a,MOTION)))return world.slot==PURSUIT_BRAKE_SLOT&&pursuit_brake_elapsed(real(a,FRAME))>=17?0:1;
    if(world.move==W_THRUST&&ascendance.radiance==RADIANCE_CHASE&&radiance_pursuit.near)return 0;
    if(ascendance.radiance==RADIANCE_HOLD&&world.slot==RADIANCE_WALK_SLOT)return ascendance_walk_rate();
    if(ascendance_independent_flash())return 1;
#endif
    float speed=w_float(world.source,W_FLOAT_SPEED,real(a,FRAME));
#if ML_ASCENDANCE_PROTOTYPE
    if(musou.phase==MUSOU_FLURRY)speed=musou.stroke?MUSOU_STROKE_RATE:speed*1.5f;
#endif
    /* Both Iai follow-ups open at frame 80. Retain the authored hold clock. */
    if(world.move==W_SHEATHE&&real(a,FRAME)<80){
        speed*=80.0f/48.0f;
    }
    return speed;
}
#if ML_WILDS_MOVES
static void wilds_equipment_shape(void *a);
#endif
#if ML_WILDS_MOVES
static void world_blade_hitstop(void *a,unsigned strength)
{
    /* GU HDP byte 38 becomes collider byte 0x57. Native contact scales
     * this value by effective hitzone and sharpness before pausing the hunter. */
    *(unsigned char *)((uptr)a+0x3020+0x57)=(unsigned char)strength;
}
#endif
static void world_hit_tick(void *a,const WEvent *e)
{
    unsigned helm=e->hit==82&&(world.move==W_DOWN||world.move==W_LAND);
    /* Source COL lingering flag/e4 is a1.5-simulation-frame per-target
     * cooldown (12d71b2 ->12d5370 ->12dd59c). Fresh native IDs bypass GU's
     * once-per-swing ledger; the per-target query enforces that cooldown. */
    if(world_hit==e->hit&&(!helm||world_helm_hit_sample==sample_sequence))return;
    if(helm)world_helm_hit_sample=sample_sequence;
    world_close_hit(a);world_hit=e->hit;
    if(e->hit<0||(helm&&world_helm_count>=7))return;
    const WHit *hit=0;
    for(unsigned i=0;i<sizeof(world_hits)/sizeof(*world_hits);++i)
        if(world_hits[i].source==(unsigned)e->hit){hit=world_hits+i;break;}
    if(!hit)return;
    unsigned row=hit->hit;
#if ML_WILDS_MOVES
    if(world.source==109&&word(a,0x3330)==3)row=WILDS_ROUND_RED_FORESIGHT_ROW;
#endif
    ((void (*)(void *,u32,u32,u32))(main_base+0x2b7240))(a,0,row,0);
    world_attack_id=u16(a,0x3054);world_attack_actor=(u32)(uptr)a;world_attack_owned=1;
    put(a,0x30a8,hit->shape);
#if ML_WILDS_MOVES
    if(world.move==W_THRUST||world.source==109)put(a,0x30a8,WILDS_SHAPE);
    if(!helm){
        unsigned strength=world.move==W_BIG?26:world.move==W_THRUST?12:18;
#if ML_ASCENDANCE_PROTOTYPE
        if(musou.phase==MUSOU_DASH||musou.phase==MUSOU_CUT||musou.phase==MUSOU_FLURRY)strength=0;
#endif
        world_blade_hitstop(a,strength);
    }
#endif
#if ML_WORLD_PRESENTATION
    /* Hide only the first Iai contact flash; damage and source sound remain. */
    /* v1.0.1 retains the native first-contact Iai flash. */
#endif
    set_real(a,0x307c,0);set_real(a,0x3080,10000);
    if(world.move==W_THRUST||world.move==W_DOWN||world.move==W_LAND)
        put(a,0x3050,word(a,0x3050)|0x08000000u);
    log_event("world_hit_open",(u32)(uptr)a,e->hit,row,(u32)real(a,FRAME));
}
#if ML_WILDS_MOVES
/* Finite native collision windows, using an owned attack ID per stroke. */
static uint64_t wilds_fired;
static unsigned wilds_hit_source,wilds_hit_generation,wilds_window=~0u;
_Static_assert(sizeof(wilds_strikes)/sizeof(*wilds_strikes)<=64,"Wilds strike mask capacity");
static void wilds_joint_up(void *a,unsigned joint,float *up)
{
    const float *m=world_bone(a,joint);
    if(!m)return;
    float above[3]={m[12],m[13]+1.f,m[14]};
    world_inverse(m,above,up);
}

static void wilds_blade_shape(void *a,const WildsClip *clip,float frame)
{
    void *resource=(void *)(uptr)word(a,0x319c);
    void *bank=resource?(void *)(uptr)word(resource,0x64):0;
    if(!bank||word(bank,8)<=WILDS_SHAPE)return;
    unsigned count=word(bank,16+WILDS_SHAPE*8),offset=word(bank,20+WILDS_SHAPE*8);
    if(count!=4)return;
    unsigned n=(unsigned)frame;if(n>=clip->frames)n=clip->frames-1;
    unsigned next=n+1<clip->frames?n+1:n;float t=frame-(float)n;if(t>1)t=1;
    const float *x=wilds_blade_samples[clip->offset+n],*y=wilds_blade_samples[clip->offset+next];
    unsigned moving_two=world.move==W_SLIDE&&world.source>=W_SPIRIT_TWO_FORWARD&&world.source<=W_SPIRIT_TWO_BACK;
    float up[3]={0};
    if(moving_two)wilds_joint_up(a,12,up);
    for(unsigned sphere=0;sphere<4;++sphere){
        put(bank,offset+sphere*48,12u|(12u<<16));
        unsigned crimson=world.move==W_CHIJIN;
        unsigned focus=world.move==W_FOCUS;
        float radius=world.source==905?38.f+11.f*sphere:focus?35.f+7.f*sphere:world.move==W_RELEASE?30.f+4.f*sphere:crimson?26.f:18.f;
        if(moving_two)radius=34.f+9.f*sphere;
        set_real(bank,offset+sphere*48+12,radius);
        for(unsigned axis=0;axis<3;++axis){unsigned k=sphere*3+axis;
            float point=x[k]+(y[k]-x[k])*t;
            if(crimson||focus||world.move==W_RELEASE||moving_two){
                /* Keep the inner sphere at the grip. Spread the four blade
                 * samples from 20..110 to their move-specific reach along the posed edge. */
                float grip=x[axis]+(y[axis]-x[axis])*t;
                point+=(point-grip)*(moving_two?.24f:world.source==905?1.13f:focus?.76f:world.move==W_RELEASE?.54f:57.f/90.f);
            }
            if(moving_two)point+=up[axis]*(-8.f+25.f*sphere);
            set_real(bank,offset+sphere*48+16+axis*4,point);
        }
    }
}
/* The original World thrust collider was authored on GU's left-hand bones.
 * Its Helm entry must follow an actual sword contact, using the same evaluated
 * equipment placement as the visible blade rather than an off-hand capsule. */
enum WBladeEnvelope { W_BLADE_STANDARD,W_BLADE_PURSUIT_FIRST,W_BLADE_PURSUIT_SECOND,
                      W_BLADE_RADIANCE_FLASH,W_BLADE_DIVIDING };
static void wilds_equipment_shape_for(void *a,enum WBladeEnvelope envelope)
{
    /* Preserve a low contact at the grip and lift successive spheres toward
     * the tip. The offsets are world vertical even when the weapon rolls. */
    static const struct {float base,step,radius,radius_step,lift,lift_step;} profiles[]={
        [W_BLADE_PURSUIT_FIRST]={24,57,43,10,-12,33},
        [W_BLADE_PURSUIT_SECOND]={24,60,47,10,-12,35},
        [W_BLADE_RADIANCE_FLASH]={24,53,36,8,-7,25},
        [W_BLADE_DIVIDING]={22,53,34,8,-5,25},
    };
    float pose[12];u32 joint,closed;
    if(world.source==109||world.source==917){
        void *blade=(void *)(uptr)word(a,0x23a0);
        float native[7];joint=12;
        if(!blade||word(blade,0x30)!=(u32)(uptr)a||!equipment_joint_pose(a,blade,joint,native))return;
        for(unsigned i=0;i<3;++i)pose[i]=native[i];
        for(unsigned i=0;i<4;++i)pose[4+i]=native[3+i];
    }else if(!attachment_pose(a,9,pose,&joint,&closed)||closed)return;
    void *r=(void *)(uptr)word(a,0x319c);void *bank=r?(void *)(uptr)word(r,0x64):0;
    if(!bank||word(bank,8)<=WILDS_SHAPE||word(bank,16+WILDS_SHAPE*8)!=4)return;
    unsigned offset=word(bank,20+WILDS_SHAPE*8);
    float x=pose[4],y=pose[5],z=pose[6],w=pose[7];
    float axis[3]={2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)};
    unsigned thrust=world.move==W_THRUST;
    float up[3]={0};
    if(thrust||envelope!=W_BLADE_STANDARD)wilds_joint_up(a,joint,up);
    for(unsigned i=0;i<4;++i){
        unsigned at=offset+i*48;put(bank,at,joint|(joint<<16));
        float distance=thrust?20.f+44.f*i:25.f+45.f*i;
        float radius=thrust?25.f+10.f*i:22.f;
        float lift=thrust?15.f*i:0.f;
        if(envelope!=W_BLADE_STANDARD){
            distance=profiles[envelope].base+profiles[envelope].step*i;
            radius=profiles[envelope].radius+profiles[envelope].radius_step*i;
            lift=profiles[envelope].lift+profiles[envelope].lift_step*i;
        }
        set_real(bank,at+12,radius);
        for(unsigned k=0;k<3;++k)
            set_real(bank,at+16+4*k,pose[k]+axis[k]*distance+up[k]*lift);
    }
}
static void wilds_equipment_shape(void *a){wilds_equipment_shape_for(a,W_BLADE_STANDARD);}
static void wilds_hit_tick(void *a)
{
    if(!world_bound||u16(a,MOTION)!=0x500+world.slot||u16(a,ACTION)!=w_action(world.move))return;
    const WildsClip *clip=wilds_clip(world.source);if(!clip)return;
    if(wilds_hit_source!=world.source||wilds_hit_generation!=bind_generation){
        wilds_hit_source=world.source;wilds_hit_generation=bind_generation;
        wilds_fired=0;wilds_window=~0u;
    }
    float frame=real(a,FRAME);unsigned active=~0u;
    world_finish_focus_burst(a,frame);
    wilds_blade_shape(a,clip,frame);
    if(world.source==917)wilds_equipment_shape(a);
    for(unsigned i=0;i<sizeof(wilds_strikes)/sizeof(*wilds_strikes);++i){
        const WildsStrike *strike=wilds_strikes+i;
        if(strike->source==world.source&&frame>=strike->begin&&frame<strike->end){active=i;break;}
    }
    if(active==wilds_window)return;
    if(wilds_window!=~0u)log_event("wilds_hit_close",(u32)(uptr)a,world.source,wilds_window,(u32)frame);
    world_close_hit(a);wilds_window=active;
    if(active==~0u||(wilds_fired&(UINT64_C(1)<<active)))return;
    const WildsStrike *strike=wilds_strikes+active;
    unsigned tier=world.wilds_tier;if(tier<1||tier>4)tier=1;
    unsigned row=strike->row+(strike->tiered?tier-1:0);
    if(world.source==917&&word(a,0x3330)!=3)row=WILDS_ROUND_NORMAL_ROW;
    ((void (*)(void *,u32,u32,u32))(main_base+0x2b7240))(a,0,row,0);
    world_attack_id=u16(a,0x3054);world_attack_actor=(u32)(uptr)a;world_attack_owned=1;world_hit=(int)row;
    /* Match GU thrust/short swings, ordinary Spirit cuts, and finishers. */
    unsigned mv=u16(a,0x3020+0x38);
    unsigned strength=mv>=35?26:mv>=20?20:12;
    crimson_final_strike=world.move==W_CHIJIN&&(active+1==sizeof(wilds_strikes)/sizeof(*wilds_strikes)||
        wilds_strikes[active+1].source!=world.source);
    if(world.source==904)strength=row==160?34:16;
    if(world.move==W_CHIJIN)strength+=crimson_final_strike?(world.source==907?9:6):3;
    world_blade_hitstop(a,strength);
    /* The empty donor has no contact timing. Use GU's Spirit Slash
     * parameters for the short cuts and its heavier closing cut. */
    if(world.source==906||world.source==907){
        set_real(a,0x3020+0x64,2.0f);
        set_real(a,0x3020+0x68,6.0f);
        *(unsigned char *)((uptr)a+0x3020+0x6c)=mv>=35?4:mv>=20?3:2;
    }
    put(a,0x30a8,WILDS_SHAPE);set_real(a,0x307c,0);set_real(a,0x3080,10000);
#if ML_WORLD_PRESENTATION
    /* Wilds hit rows use the empty donor; restore the native long-sword
     * contact selector. GU decides whether and where the blood appears. */
    *(unsigned char *)((uptr)a+0x3020+0x52)=0x2e;
    world_white_stroke(a,1);
#endif
    wilds_fired|=UINT64_C(1)<<active;
#if ML_ASCENDANCE_PROTOTYPE && (ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION)
    if(world.source==W_CRIMSON_SPIN)lingyue_attack_sound(a,WILDS_SOUND_HELM_CUT);
#endif
    log_event("wilds_hit_open",(u32)(uptr)a,world.source,row,(u32)frame);
}

#endif
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
static void ascendance_flash_blade(void *a)
{
    float f=real(a,FRAME);
    unsigned active=f>=RADIANCE_FLASH_HIT_BEGIN&&f<RADIANCE_FLASH_HIT_END;
    int hit=active?(int)(86+(world.rank>3?0:world.rank)):-1;
    WEvent strike={.hit=hit};
    if(active)wilds_equipment_shape_for(a,W_BLADE_RADIANCE_FLASH);
    unsigned opening=active&&world_hit!=hit;
    world_hit_tick(a,&strike);
    if(!active||!world_attack_owned)return;
    put(a,0x30a8,WILDS_SHAPE);
    if(!opening)return;
    world_blade_hitstop(a,22);
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    lingyue_attack_sound(a,WILDS_SOUND_IAI_CUT);
    lingyue_attack_sound(a,WILDS_SOUND_IAI_WHOOSH);
#endif
#if ML_WORLD_PRESENTATION
    void *weapon=(void *)(uptr)word(a,0x23a0);
    const float *blade=weapon&&word(weapon,0x30)==(u32)(uptr)a?world_bone(weapon,0):0;
    if(blade){
        float point[3];
        for(unsigned k=0;k<3;++k)point[k]=blade[12+k]+blade[8+k]*70;
        world_sfx_oriented(a,point,0x3f64,blade);
    }
#endif
    log_event("ascendance_flash_edge",(u32)(uptr)a,(u32)f,world_attack_id,hit);
}
static void ascendance_follow_blade(void *a)
{
    unsigned second=world.slot==PURSUIT_SECOND_SLOT;
    float f=real(a,FRAME);
    unsigned active=second?(f>=PURSUIT_SECOND_HIT_BEGIN&&f<PURSUIT_SECOND_HIT_END):(f>=10&&f<18);
    if(!second&&radiance_pursuit.first_hit_hold>0)active=0;
    if(ascendance.double_cut){
        float cut=f-double_draw_offset(world.slot);
        second=cut>=33;
        active=world.slot!=DOUBLE_DRAW_RETURN&&((cut>=13&&cut<19)||(second&&cut<38));
    }
    WEvent strike={.hit=active?(second?65:63):-1};
    if(active){
        enum WBladeEnvelope envelope=W_BLADE_STANDARD;
        if(!ascendance.double_cut&&radiance_pursuit.independent)
            envelope=second?W_BLADE_PURSUIT_SECOND:W_BLADE_PURSUIT_FIRST;
        wilds_equipment_shape_for(a,envelope);
    }
    unsigned opening=active&&world_hit!=strike.hit;
    world_hit_tick(a,&strike);
    if(!active||!world_attack_owned)return;
    put(a,0x30a8,WILDS_SHAPE);
    if(!opening)return;
    world_blade_hitstop(a,second?20:14);
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    if(ascendance.double_cut)
        lingyue_attack_sound(a,second?WILDS_SOUND_IAI_WHOOSH:WILDS_SOUND_IAI_CUT);
    else lingyue_attack_sound(a,second?WILDS_SOUND_HELM_CUT:wilds_swing_requests[0]);
#endif
#if ML_WORLD_PRESENTATION
    world_white_stroke(a,0);
    if(second&&!ascendance.double_cut&&radiance_pursuit.independent)
        world_radiance_second_fx(a,f);
#endif
    log_event("ascendance_follow_edge",(u32)(uptr)a,second+1,(u32)f,world_attack_id);
}
static void ascendance_musou_blade(void *a,const MusouStroke *clip)
{
    float frame=real(a,FRAME);
    unsigned group_spin=musou_spin_slot(clip->slot);
    unsigned spin=group_spin||clip->slot==musou_strokes[MUSOU_SPIN].slot;
    unsigned active=frame>=clip->hit_begin&&frame<clip->hit_end;
    int hit=active?(spin?65:63):-1;
    if(group_spin){
        /* Different logical keys open fresh native attack IDs even if this
         * update advances across the gap between the two swing windows. */
        if(frame>=16.f&&frame<21.f)hit=MUSOU_SPIN_FOLLOW_HIT;
        else if(frame>=14.f)hit=-1;
        active=hit>=0;
    }
    WEvent strike={.hit=hit};
    if(active)wilds_equipment_shape(a);
    unsigned opening=active&&world_hit!=strike.hit;
    world_hit_tick(a,&strike);
    if(!active||!world_attack_owned)return;
    put(a,0x30a8,WILDS_SHAPE);
    if(!opening)return;
    world_blade_hitstop(a,0);
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    lingyue_attack_sound(a,hit==MUSOU_SPIN_FOLLOW_HIT?WILDS_SOUND_THRUST_CUT:clip->sound);
#endif
#if ML_WORLD_PRESENTATION
    void *weapon=(void *)(uptr)word(a,0x23a0);
    const float *blade=weapon&&word(weapon,0x30)==(u32)(uptr)a?world_bone(weapon,0):0;
    if(blade){
        float point[3];
        for(unsigned k=0;k<3;++k)point[k]=blade[12+k]+blade[8+k]*70;
        world_sfx_emit(a,point,0x3f61);
    }
#endif
    log_event("ascendance_musou_swing",(u32)(uptr)a,clip->slot,musou.stroke,(u32)frame);
}
static void lingyue_musou_finish_hit(void *a,const WEvent *event)
{
    unsigned opening=event&&event->hit>=0&&world_hit!=event->hit;
    world_hit_tick(a,event);
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    if(opening&&world_attack_owned)
        lingyue_attack_sound(a,musou.phase==MUSOU_DASH?WILDS_SOUND_HELM_CUT:WILDS_SOUND_IAI_CUT);
#endif
}
#endif
static void world_bind_motion(void *a)
{
    ((void (*)(void *,u32,u32,u32))(main_base+0x282fd8))(a,0,0,0);
    *(unsigned char *)((uptr)a+5)=1;
    /* GU53a1dc adds two frames to positive blends and otherwise sets
     * internal10000 (root motion during interpolation). Flag80 avoids
     * that stock root mode; flag1 lets native280c1c advance the action
     * clock while poses blend (default GU freezes it until blend ends).
     * Keep the source duration, frame0 and unmodified event clock. */
#if ML_ASCENDANCE_PROTOTYPE
    /* Retain the boost for this action; each new bind samples it again. */
    world_bound_attack_rate=ascendance_attack_rate(a,world.source);
    if(world_bound_attack_rate>1.f)
        log_event("ascendance_attack_speed",(u32)(uptr)a,world.source,world.slot,(u32)(world_bound_attack_rate*100.f));
#endif
    original_layer(a,0,5000+world.slot,world_entry_blend>0?0x81:0,
                   world_entry_blend>2?world_entry_blend-2:0,world_entry_start,world_entry_speed);
    log_event("world_blend",(u32)(uptr)a,world.source,(u32)world_entry_blend,(u32)real(a,0x4f4));
    /* Stock common action1:2 uses vtable46c / 2a2560(a,0) to sheath.
     * Keep its packed flags intact while playing source cIaigiriWpOff312. */
    if(world.move==W_SHEATHE_END)
        ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,0);
    world_physics_begin(a,world.move);
    world_bound=1;world_bound_action=w_action(world.move);world_bound_actor=(u32)(uptr)a;
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    world_audio_begin(a);
#endif
    world_fx_begin(a);
#if ML_ASCENDANCE_PROTOTYPE
    if(!dividing_slot(world.slot)&&!ascendance.double_cut&&!musou.phase&&!radiance_pursuit_pose(u16(a,MOTION)))
#endif
    world_detector_begin(a);
}

static void world_commit(void *a,WDecision d)
{
#if ML_WILDS_MOVES
    if(world.move!=W_FOCUS||!world.wilds_focus_break){
        focus_burst.target=0;world_focus_stagger_release();
    }
#endif
    unsigned owned_abort=d.kind==W_ABORT && world_bound && world_bound_actor==(u32)(uptr)a &&
        u16(a,STATE)==16 && u16(a,ACTION)==world_bound_action;
    unsigned gauge_before=word(a,0x2780);
    world_pay(a,d);
#if ML_WILDS_MOVES
    unsigned aim;
    unsigned crimson=(d.kind==W_ENTER||d.kind==W_MOTION)&&d.move==W_CHIJIN;
    if(crimson){
        crimson_travel_heading=d.heading;
        crimson_split_heading=0;
    }
    unsigned focus_entry=d.kind==W_ENTER&&(d.move==W_FOCUS||d.move==W_CHIJIN||d.move==W_GROUND||
        d.move==W_THRUST||d.move==W_SMALL||d.move==W_BIG||d.move==W_SLIDE);
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance.radiance==RADIANCE_RELEASE||ascendance.radiance==RADIANCE_CHASE||
       ascendance.radiance==RADIANCE_FOLLOW||
       musou.phase)focus_entry=0;
#endif
    if(d.kind!=W_KEEP)focus_thrust_turn.actor=0;
#if ML_ASCENDANCE_PROTOTYPE
    unsigned focus_thrust=focus_entry&&(d.move==W_THRUST||d.move==W_FOCUS)&&
#else
    unsigned focus_thrust=d.kind==W_ENTER&&(d.move==W_THRUST||d.move==W_FOCUS)&&
#endif
        (ml_pad_raw_bits()&PAD_FOCUS)&&world_camera_heading(a,&aim);
    unsigned focus_phase=d.kind==W_MOTION&&(d.move==W_CHIJIN||d.move==W_SLIDE||d.source==902);
    /* Sideways Crimson I commits the cut in the same direction as its
     * step. The retreat keeps its forward cut, including in Focus. */
    if(focus_thrust){
        /* Do not snapshot a partially eased idle-facing angle as the target. */
        d.heading=world.heading=u16(a,HEADING);
    }else if((focus_entry||focus_phase||d.kind==W_NATIVE)&&d.crimson_direction!=W_CRIMSON_LATERAL&&
       world_focus_heading(a,&aim)){
        if(crimson)crimson_split_heading=d.crimson_direction;
        d.heading=aim;
        if(d.kind!=W_NATIVE)world.heading=aim;
        if(d.move==W_SMALL||d.move==W_BIG){
            world.iai_focus_locked=1;
            log_event("wilds_focus_iai",(u32)(uptr)a,d.source,aim,(u32)real(a,FRAME));
        }
    }
#endif
#if ML_WILDS_MOVES
    if(d.kind==W_ENTER&&d.move>=W_CHIJIN){
        log_event("wilds_direction",(u32)(uptr)a,world.heading,world.source,held);
    }
#endif
#if ML_WORLD_INPUT_VM
    if(d.kind==W_ENTER && d.move==W_GROUND && world.ground_node==1)
        log_event("world_spirit_charge",(u32)(uptr)a,gauge_before,word(a,0x2780),world.spirit_paid);
#endif
    if(d.kind==W_KEEP)return;
#if ML_ASCENDANCE_PROTOTYPE
    if(musou.phase==MUSOU_FLURRY&&musou.cycles==1&&d.kind==W_MOTION)world_armor=0;
    unsigned enhanced_small_draw=d.kind==W_ENTER&&d.move==W_SMALL&&d.source==307&&
        ascendance.sheathe&&!ascendance.radiance&&ascendance.remaining>0&&
        ascendance.ready_frames<BOOST_SHEATHE_WINDOW&&((u16(a,MOTION)==0x500+ENHANCED_SHEATHE_SLOT||u16(a,MOTION)==0x500+MUSOU_HOLD_SLOT)||u16(a,MOTION)==0x500+32);
    if(d.kind==W_ROLL)ascendance_resheathe_roll_begin(a);
    ascendance_transition(a,&d);
    ascendance_sheathe_enter(a,&d);
#endif
    unsigned native_tap=world_r_gate.active&&world_r_gate.button==WORLD_R_WAIT_RELEASE&&world_r_gate.actor==(u32)(uptr)a&&
        world_r_gate.epoch==byte(a,AREA)&&world_r_gate.generation==bind_generation&&
        world_r_gate.owner_state==u16(a,STATE)&&world_r_gate.owner_action==u16(a,ACTION)&&
        world_r_gate.motion==u16(a,MOTION)&&(d.kind==W_NATIVE||d.kind==W_NATIVE_R);
    if(native_tap){
        if(word(a,0x3330)!=3&&word(a,0x2780)<w_spirit_link_cost(world_r_gate.action)){
            world_r_gate.active=0;return;
        }
        d.kind=W_NATIVE;d.native_action=world_r_gate.action;
    }
    world_idle_actor=0;
    if(d.kind==W_IDLE && world_bound && world_bound_actor==(u32)(uptr)a){
        world_idle_actor=a;world_idle_motion=u16(a,MOTION);world_idle_epoch=byte(a,AREA);
#if ML_WILDS_MOVES
        if(world_idle_motion==0x5fa&&real(a,FRAME)>=W_RELEASE_RECOVERY_CANCEL&&
           !(byte(a,DRAWN)&1)&&u16(a,0x1410)>=50&&u16(a,0x1410)<=128){
            release_walk_turn=(WFacingTurn){.actor=(u32)(uptr)a,.epoch=byte(a,AREA),
                .yaw=u16(a,HEADING),.sample=sample_sequence};
        }
#endif
    }
    unsigned loop=d.kind==W_NATIVE&&
        ((d.native_action==25&&(u16(a,MOTION)==0x569||u16(a,MOTION)==0x570))||(d.native_action==4&&u16(a,MOTION)==0x56b));
    world_loop_actor=loop?(u32)(uptr)a:0;
    if(loop){world_loop_epoch=byte(a,AREA);world_loop_action=d.native_action;world_loop_facing=d.heading;}
    world_r_gate.active=0;
    if(d.kind!=W_ENTER||d.move<W_RISE||d.move>W_DOWN)world_physics_end(a);
    if(d.kind==W_ENTER){
#if ML_WORLD_PRESENTATION
        if(d.move==W_LAND)world_helm_landed=world_elapsed;
#endif
#if ML_WORLD_PRESENTATION && WORLD_AUDIO_GROUND
        const float *point=(const float *)((uptr)a+0x40);
        if(d.move==W_FORESIGHT&&world.foresight_gauge)
            world_audio_epv(a,25505,point);
#if !ML_WILDS_MOVES
        if(d.move==W_THRUST&&world.rank)world_audio_epv(a,25505,point);
#endif
#if ML_WILDS_MOVES
        if(d.move==W_RISE)world_helm_wind_played=0;
#else
        if(d.move==W_BIG && world.rank)world_audio_epv(a,25605,point);
        if(d.move==W_RISE)world_audio_epv(a,25515,point);
        if(d.move==W_LAND){
            if(world_helm_count)world_audio_tail(a,1,world_helm_sound_point);
        }
#endif
#endif
        if(d.move==W_RISE){
            world_helm_tier=world.rank;
            world_helm_count=world_helm_hit_sample=world_helm_emitted=0;
            world_helm_line_hit=0;
            for(unsigned i=0;i<7;++i){world_follow[i].live=0;world_follow[i].remaining=0;}
            for(unsigned i=0;i<10;++i)world_primary_cooldown[i].target=0;
            world_rise_dx=real(a,0x3040)-real(a,0x40);
            world_rise_dz=real(a,0x3048)-real(a,0x48);
        }
        if(d.move==W_BIG){
            world_follow[7]=(WFollow){0};
#if ML_ASCENDANCE_PROTOTYPE
            if(!ascendance_independent_flash())
#endif
            world_camera_begin(a);
        }
#if ML_WILDS_MOVES
        if(d.move==W_RELEASE)for(unsigned i=0;i<4;++i)release_marks[i].target=0;
        if(d.source==918){
            if(world.valor_foresight){
                unsigned before=word(a,10128);
                ((void (*)(void *,int))(main_base+0x2a3284))(a,100);
                log_event("valor_foresight_spend",(u32)(uptr)a,before,word(a,10128),100);
            }else world_red_time(a,-RED_AURA_SPEND,918);
        }
#endif
    }
    world_cleanup(a);ml_reset(&controller);
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance.actor==a)ascendance.dividing_step_pending=0;
#endif
    ++submitting;
    if(d.kind==W_ENTER||d.kind==W_MOTION){
        log_event("world_source_transition",(u32)(uptr)a,w_source(u16(a,MOTION)),d.source,(u32)(real(a,FRAME)*1000));
#if ML_WILDS_MOVES
        /* The run clip can outlive the R release by a sample. Bind the attack
         * as soon as the native attack state accepts the draw request. */
        unsigned running_crimson=d.kind==W_ENTER&&d.move==W_CHIJIN&&!(byte(a,DRAWN)&1)&&
            u16(a,STATE)==2&&(u16(a,MOTION)==4u||w_uphill_run(2,u16(a,ACTION)));
#endif
        world_entry_blend=d.motion_contract?d.blend:world_iai_blend(d.source,w_source(u16(a,MOTION)),real(a,FRAME));
#if ML_ASCENDANCE_PROTOTYPE
        /* Release the enhanced hold into the native Iai from frame zero. */
        if(!d.motion_contract&&d.kind==W_ENTER&&d.move==W_BIG&&ascendance.enhanced_iai&&
           (u16(a,MOTION)==0x500+ENHANCED_SHEATHE_SLOT||u16(a,MOTION)==0x500+MUSOU_HOLD_SLOT))
            world_entry_blend=6;
        if(!d.motion_contract&&enhanced_small_draw)world_entry_blend=7;
        /* Ease the outgoing Spirit III pose into ordinary Foresight. */
        if(!d.motion_contract&&d.kind==W_ENTER&&d.move==W_FORESIGHT&&d.source==140&&
           dividing_slot(u16(a,MOTION)-0x500)){
            world_entry_blend=6;
            if(u16(a,MOTION)==0x500+DIVIDING_STEP_SLOT){
                step_foresight_round.actor=a;step_foresight_round.epoch=byte(a,AREA);
                step_foresight_round.stage=1;step_foresight_round.prep=0;step_foresight_round.prep_frame=-1;
            }
        }
#endif
        world_entry_start=d.motion_contract?d.start:0;world_entry_speed=d.motion_contract?d.speed:1;
#if ML_WILDS_MOVES
        unsigned from=roll_turn.actor==(u32)(uptr)a&&roll_turn.epoch==byte(a,AREA)?
            roll_turn.yaw:u16(a,HEADING);
        roll_turn.actor=0;
        int gap=ml_heading_delta(d.heading,from);
        entry_turn.actor=0;
        entry_turn_delay=0;
        unsigned ease_entry=gap>2185||gap< -2185;
#if ML_ASCENDANCE_PROTOTYPE
        /* A directional double draw applies its selected world heading at entry. */
        if(double_draw_slot(d.slot)&&double_draw_offset(d.slot))ease_entry=0;
        if(d.source==917&&u16(a,MOTION)==0x500+DIVIDING_STEP_SLOT&&
           (gap<=-16384||gap>=16384))entry_turn_delay=7;
#endif
        if(ease_entry){
            entry_turn.actor=(u32)(uptr)a;entry_turn.epoch=byte(a,AREA);entry_turn.source=d.source;
            entry_turn.target=d.heading;entry_turn.yaw=from;entry_turn.sample=sample_sequence;
            world.heading=d.heading=from;
        }
        if(d.source>=W_SPIRIT_TWO_FORWARD&&d.source<=W_SPIRIT_TWO_BACK&&world_entry_blend<8)
            world_entry_blend=8;
        /* Keep the Spirit III recovery pose briefly as the round begins.
         * The existing contract advances the attack clock throughout. */
        if(!d.motion_contract&&d.kind==W_ENTER&&d.source==917&&u16(a,MOTION)==0x56c&&
           u16(a,STATE)==16&&(u16(a,ACTION)==26||u16(a,ACTION)==27))
            world_entry_blend=5;
#endif
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
        if(d.kind==W_ENTER){
#if ML_WILDS_MOVES
            if((d.move==W_CHIJIN||d.move==W_SLIDE)&&!(byte(a,DRAWN)&1)){
                ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,1);
                log_event(d.move==W_SLIDE?"wilds_slide_draw":"wilds_red_draw",
                    (u32)(uptr)a,u16(a,STATE),u16(a,ACTION),byte(a,DRAWN));
            }
#endif
            virtual_request(a,16,w_action(d.move),0);
        }
        log_event("world_enter",(u32)(uptr)a,d.move,d.source,d.slot);
#if ML_WILDS_MOVES
        if((running_crimson||(d.kind==W_ENTER&&d.move==W_FORESIGHT))&&!world_bound&&
           u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move)){
            world_bind_motion(a);
            if(running_crimson)log_event("crimson_run_bind",(u32)(uptr)a,d.slot,u16(a,MOTION),sample_sequence);
            log_event("world_bound",(u32)(uptr)a,world.move,u16(a,MOTION),word(a,0x1c));
        }
#endif
    }else if(d.kind==W_NATIVE){
#if ML_WILDS_MOVES
        if(d.skip_round_rank){
            valor_foresight_round.actor=a;
            valor_foresight_round.epoch=byte(a,AREA);
        }else if(valor_foresight_round.actor==a)valor_foresight_round.actor=0;
#endif
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
        unsigned from_motion=u16(a,MOTION);
        if(d.native_action==26&&d.dividing_step_requested&&ascendance_owner(a)&&ascendance.remaining>0){
            ascendance.dividing_step_pending=1;
            ascendance.dividing_step_heading=d.dividing_step_heading;
            ascendance.dividing_step_sample=sample_sequence;
        }
        if(d.native_action==26&&(from_motion==0x555||from_motion==0x556)){
            slide_third=(__typeof__(slide_third)){
                .actor=a,.area=byte(a,AREA),.entry_sample=sample_sequence
            };
        }
#endif
        log_event("world_native_follow",(u32)(uptr)a,d.native_action,u16(a,MOTION),(u32)real(a,FRAME));
        unsigned flags=d.native_action>=16?0x8000:0;
        if((d.native_action==16||d.native_action==17)&&(u16(a,MOTION)==0x569||u16(a,MOTION)==0x56b)){
            /* Preserve the observed native solo-R request's turn flags.
             *2ab670 consumes1000/8000 differently when adjusting heading. */
            flags=0x1000;
            log_event("world_chord_solo",(u32)(uptr)a,d.native_action,flags,0);
        }
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
        virtual_request(a,16,d.native_action,native_tap?world_r_gate.flags:flags);
        world_loop_heading(a);
    }else if(d.kind==W_NATIVE_R){
        unsigned current=u16(a,ACTION),previous=u16(a,0x250c),gauge=word(a,0x2780),target=0;
        unsigned available=gauge;
#if ML_WILDS_MOVES
        if(word(a,0x3330)==3)available=100;
#endif
        /* Native reference: I=16/17 powered/unpowered. SpiritStab22
         * resumes II24; upper25 from II18/19/23/24 resumes III27. Native
         * II costs15 and III costs20; selected native handlers own spending. */
        if(current==22)target=available>=15?24:0;
        else if(current==25&&(previous==18||previous==19||previous==23||previous==24))target=available>=20?27:0;
        else target=available>=15?16:17;
        log_event("world_chord_solo",(u32)(uptr)a,target,0x1000,gauge);
        if(target)virtual_request(a,16,target,0x1000);
    }else if(d.kind==W_ROLL){
#if ML_WILDS_MOVES
        unsigned from=u16(a,HEADING);
        int gap=ml_heading_delta(d.heading,from);
        roll_turn=(WFacingTurn){0};
        if(gap>2185||gap< -2185)
            roll_turn=(WFacingTurn){.actor=(u32)(uptr)a,.epoch=byte(a,AREA),
                .target=d.heading,.yaw=from,.sample=sample_sequence};
        if(!roll_turn.actor)put(a,HEADING,d.heading);
        put(a,TARGET,d.heading);
#else
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
#endif
        if(d.sheathed_roll)
            ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,0);
        virtual_request(a,1,6,12);
    }
    else if(d.kind==W_IDLE)((void (*)(void *,u32,u32,u32,u32,u32,u32))(main_base+0x2b6e94))(a,0,0,0,0,0,0);
    else if(owned_abort)virtual_request(a,1,0,0);
    --submitting;
}
#if ML_WILDS_MOVES
static void world_iai_contact_entry(void *a)
{
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance_owner(a)&&(ascendance.radiance==RADIANCE_HOLD||ascendance.musou_sheathe_r))return;
#endif
    ml_pad_read();
    WObservation o={.base=observe(a,0),.state=u16(a,STATE),.drawn=byte(a,DRAWN)&1,
        .alive=1,.grounded=!(word(a,AIR)&0x400),.resources_ready=world_ready(a),.valor=valor_active(a),
        .rank=((unsigned (*)(void *))(main_base+0x119ed80))(a)};
    WDecision d=w_iai_press(&world,&o);
    if(d.kind!=W_ENTER)return;
    world_commit(a,d);w_input_finish(&world,&o,d);
    log_event("wilds_iai_contact_entry",o.base.actor,d.source,(u32)o.base.animation_frame,o.base.held);
}
#if ML_WORLD_INPUT_VM
/* Body collision can come first on the same engine tick as a legal input.
 * Use the regular source window and only intercept a funded Foresight. */
static unsigned world_foresight_contact_entry(void *a)
{
#if ML_ASCENDANCE_PROTOTYPE
    if(musou.phase||ascendance.double_cut||ascendance.radiance)return 0;
#endif
    ml_pad_read();
    if(held!=(ML_R|ML_A)&&!world.foresight_press.motion)return 0;
    unsigned valor=valor_active(a);
    unsigned rank=crimson_available(a)?3:((unsigned (*)(void *))(main_base+0x119ed80))(a);
    WObservation o={.base=observe(a,1),.state=u16(a,STATE),.drawn=byte(a,DRAWN)&1,
        .alive=1,.grounded=!(word(a,AIR)&0x400),.resources_ready=world_ready(a),.valor=valor,
        .rank=rank};
    if(rank!=3&&o.base.gauge<10)return 0;
    WDecision d=w_foresight_press(&world,&o);
    if(d.kind!=W_ENTER)return 0;
    world_commit(a,d);w_input_finish(&world,&o,d);
    log_event("wilds_foresight_contact_entry",o.base.actor,d.source,(u32)o.base.animation_frame,o.base.held);
    return 1;
}
#endif
#endif
#if ML_WILDS_MOVES
/* The walking-potion controller commits X directly, bypassing request_hook.
 * Wait for GU to accept the draw so the potion still owns its cancel window. */
static struct {
    unsigned actor,epoch,saw_x,heading,magnitude;
} potion_draw;
static unsigned world_potion_draw(WObservation *o)
{
    const MLObservation *b=&o->base;
    if(world.move||o->rank!=3||!o->alive||!o->grounded||!o->resources_ready||
       (b->held&~ML_X)){
        potion_draw.actor=0;
        return 0;
    }
    if(o->state==1&&b->native_action==85&&!o->drawn){
        if(potion_draw.actor!=b->actor||potion_draw.epoch!=b->epoch)
            potion_draw=(__typeof__(potion_draw)){.actor=b->actor,.epoch=b->epoch};
        if(b->held==ML_X){
            potion_draw.saw_x=1;potion_draw.heading=b->input_heading;potion_draw.magnitude=b->magnitude;
        }
        return 0;
    }
    if(potion_draw.actor!=b->actor||potion_draw.epoch!=b->epoch)return 0;
    if((o->state==1&&b->native_action==1)||(o->state==16&&b->native_action==0)){
        if(!o->drawn)return 0;
        potion_draw.actor=0;
        if(potion_draw.saw_x){
            o->base.input_heading=potion_draw.heading;o->base.magnitude=potion_draw.magnitude;
        }
        return 1;
    }
    potion_draw.actor=0;
    return 0;
}
static void world_roll_crimson_input(const MLObservation *input)
{
    wilds_roll_x=1;
    wilds_roll_magnitude=input->magnitude;
    wilds_roll_heading=input->input_heading;
}
#endif
#if ML_WILDS_MOVES
/* A confirmed guard has its own reaction animation. Keep the player's chord
 * through hitstop and replay a solitary native Spirit request exactly once. */
static struct {
    void *actor;
    unsigned epoch,action,generation,chord,spirit,flags,early_r,replayed,released;
    float deadline;
} valor_guard;
static unsigned valor_guard_pose(void *a)
{
    return w_valor_guard_reaction(u16(a,STATE),u16(a,ACTION),u16(a,MOTION));
}
static unsigned valor_guard_same(void *a)
{
    return valor_guard.actor==a&&valor_guard.epoch==byte(a,AREA)&&
           valor_guard.generation==bind_generation&&valor_guard.action==u16(a,ACTION);
}
/* Deliver one early R through GU's own pad reader when its recovery window
 * opens. GU still selects the legal attack, checks the gauge and spends it. */
static unsigned valor_guard_pad(unsigned bits)
{
    if(!valor_guard.early_r)return 0;
    void *a=valor_guard.actor;
    void *players=(void *)(uptr)word((void *)main_base,0x1885b58);
    if(!a||!players||((void *(*)(void *))(main_base+0x276278))(players)!=a||
       world.move||world.actor!=(uptr)a||!valor_guard_same(a)||!valor_guard_pose(a)||
       (int16_t)u16(a,HP)<=0||!(byte(a,DRAWN)&1)){
        valor_guard.early_r=0;return 0;
    }
    if(bits&(0xf000u|0x800u)){valor_guard.early_r=0;return 0;}
    if(bits&PAD_R){valor_guard.released=0;return 0;}
    /* Expose the physical release before replay, even when the player
     * held the tap until the native recovery window was already open. */
    if(!valor_guard.released)return 0;
    float frame=real(a,FRAME);
    if(frame<55)return 0;
    if(!valor_guard.replayed){valor_guard.replayed=1;valor_guard.deadline=frame+8;}
    if(frame>=valor_guard.deadline){valor_guard.early_r=0;return 0;}
    return 1;
}
static unsigned valor_follow_input(unsigned keys,unsigned edge)
{
    return ((edge&PAD_X)&&keys==ML_X)||
           ((edge&(PAD_X|PAD_R))&&keys==(ML_R|ML_X))||
           ((edge&0x6200u)&&keys==(ML_R|ML_A));
}
static void valor_guard_input(void *a,unsigned keys,unsigned edge)
{
    if(!valor_guard_same(a)){
        valor_guard.actor=a;valor_guard.epoch=byte(a,AREA);valor_guard.action=u16(a,ACTION);
        valor_guard.early_r=valor_guard.replayed=valor_guard.released=0;
        valor_guard.generation=bind_generation;valor_guard.chord=valor_guard.spirit=0;
    }
    if(valor_guard.early_r&&!(keys&ML_R))valor_guard.released=1;
    if((edge&PAD_R)&&keys==ML_R&&!valor_guard.replayed&&!valor_guard.spirit&&real(a,FRAME)<55)valor_guard.early_r=1;
    if(keys==(ML_R|ML_X)&&(edge&PAD_X)&&(world.spirit_crimson_held&ML_R)&&
       (world.spirit_crimson_guard_r||world.spirit_crimson_action!=u16(a,ACTION))){
        valor_guard.chord=valor_guard.spirit=valor_guard.early_r=0;
        return;
    }
    if(valor_follow_input(keys,edge)){
        valor_guard.chord=keys;valor_guard.spirit=valor_guard.early_r=0;
    }
}
#endif
#if ML_WILDS_MOVES
static struct {
    void *actor;
    unsigned epoch,owner,keys,packet,flags,moving,sample,heading,input_heading;
    float elapsed;
} valor_link;
static struct {
    void *actor;
    unsigned epoch,state,action,sample,heading,input,magnitude;
    float elapsed;
} valor_x;
static unsigned valor_draw_cut(void *a)
{
    if(u16(a,STATE)!=16)return 0;
    unsigned cancel=w_valor_draw_cancel(u16(a,ACTION),u16(a,MOTION));
    return cancel&&hunter_style(a)==5&&!valor_active(a)?cancel:0;
}
static unsigned draw_cut_move(unsigned keys)
{
    if(keys==(ML_R|ML_X))return W_THRUST;
    return keys==(ML_R|ML_B)?W_SHEATHE:keys==(ML_R|ML_A)?W_FORESIGHT:0;
}
static unsigned valor_spirit_recovery(void *a)
{
    unsigned act=u16(a,ACTION),motion=u16(a,MOTION);
    if(u16(a,STATE)!=16||!((act>=23&&act<=24)||act==116||act==117||(act>=134&&act<=141)))return 0;
    /* Require the native strike/recovery pair. A single action can bind a
     * second clip with its own cancel frame. Carry input across that bind. */
    return w_valor_chord(16,act,motion,10000,ML_R|ML_A,0)!=0;
}
static unsigned crimson_available(void *a)
{
    return word(a,0x3330)==3||(word(a,0x3330)==4&&valor_active(a));
}
static struct {
    void *owner;
    unsigned area,last_sample;
    float x,y,z,floor,uphill_frames;
} crimson_sprint;

/* Climbing for this many samples arms the sprint draw. One step or a rubble
 bump only lasts a few samples, so the threshold keeps flat ground refused. */
#define WORLD_SPRINT_ARM 8.f

static unsigned world_rising_sprint(void *a)
{
    return crimson_sprint.owner==a&&crimson_sprint.area==byte(a,AREA)&&
        crimson_sprint.uphill_frames>=WORLD_SPRINT_ARM;
}

static void world_crimson_sprint_tick(void *a,const WObservation *o)
{
    unsigned area=byte(a,AREA),sample=o->base.sample_sequence;
    float frames=o->base.simulation_delta,floor=real(a,0x10f0),height=real(a,0x44)-floor;
    if(o->state!=2||o->base.native_action!=2||o->drawn||o->rank!=3||
       !o->alive||!o->grounded||!(height>=-1.5f&&height<=1.5f)){
        crimson_sprint.owner=0;
        return;
    }
    if(!(frames>0))return;
    float x=real(a,0x40),y=real(a,0x44),z=real(a,0x48),climb=0,travel=0,dy=0,rise=0;
    unsigned span=0;
    if(crimson_sprint.owner==a&&crimson_sprint.area==area){
        unsigned elapsed=sample-crimson_sprint.last_sample;
        if(!elapsed)return;
        float dx=x-crimson_sprint.x,dz=z-crimson_sprint.z;
        dy=floor-crimson_sprint.floor;
        rise=y-crimson_sprint.y;
        travel=dx*dx+dz*dz;
        span=elapsed;
        float max_step=32.f*(frames>1.f?frames:1.f)*(float)elapsed;
        /* Moving platforms (the Ahtal-Ka mech) climb without moving the floor
         * sample, so take the larger of the floor rise and the body rise. A
         * stationary lift still fails the travel floor below. */
        if(rise>dy)dy=rise;
        /* Dynamic ramps can use ordinary sprint. The hook can be sampled at a
         * different rate than the simulation clock, so one step may cover more
         * than one sample; the ratios below are per span, not per sample. */
        if(elapsed<=4&&frames<=3.f*(float)elapsed&&travel>=1.f&&travel<=max_step*max_step&&
           dy>0&&dy*dy>=.026f*travel&&dy*dy<=travel)
            climb=crimson_sprint.uphill_frames+frames;
    }
    crimson_sprint=(__typeof__(crimson_sprint)){
        .owner=a,.area=area,.last_sample=sample,.x=x,.y=y,.z=z,.floor=floor,
        .uphill_frames=climb>WORLD_SPRINT_ARM?WORLD_SPRINT_ARM:climb};
    /* Trace build only: arm x100 in bits 0-27, the sample span in bits 28-31,
     * squared travel x100 and the rise x1000 (+100000). Log on every change of
     * the arm level plus a 16-sample heartbeat. */
    static unsigned last_armed;
    unsigned armed=(unsigned)(crimson_sprint.uphill_frames*100.f);
    if(armed!=last_armed||(sample&15u)==0){
        last_armed=armed;
        log_event("crimson_sprint",(u32)(uptr)a,armed|((span&0xfu)<<28),(u32)(travel*100.f),
            (u32)((dy*1000.f)+100000.f));
    }
}

static unsigned world_red_draw_input(const WObservation *o)
{
    return o->base.held==ML_X||
        (o->base.held==(ML_R|ML_X)&&w_red_sprint(o));
}
static unsigned valor_link_pose(void *a)
{
    unsigned state=u16(a,STATE),act=u16(a,ACTION);
    if((state==1||state==2)&&act<=5)return 1;
    if(state==16&&act==130&&(word(a,10056)&0x400000u))return 4;
    if(valor_spirit_recovery(a))return 2;
    if(state==16&&act>=86&&!valor_guard_pose(a)&&
       w_valor_chord(state,act,u16(a,MOTION),real(a,FRAME)+18,ML_R|ML_Y,1))return 6;
    unsigned source=w_guild_source(state,act,u16(a,MOTION));
    return (source==101||source==102||source==103||source==105||source==107||
            source==127||source==128)&&real(a,FRAME)>=24?3:0;
}
static void valor_link_collect(void *a,const MLObservation *b,unsigned edge,unsigned pose)
{
    if(!pose||pose==4)return;
    unsigned owner=pose==1?0:u16(a,ACTION);
    if(valor_link.actor!=a||valor_link.epoch!=byte(a,AREA)||valor_link.owner!=owner){
        valor_link=(__typeof__(valor_link)){.actor=a,.epoch=byte(a,AREA),.owner=owner};
    }
    if(pose==5&&(edge&(0x6200u|PAD_X))&&draw_cut_move(b->held&~ML_Y)){
        valor_link.keys=b->held&~ML_Y;valor_link.packet=0;
    }
    if((pose==2||pose==3||pose==6)&&valor_follow_input(b->held,edge)){
        valor_link.keys=b->held;valor_link.packet=0;
    }
    if(pose!=2&&pose!=5&&pose!=6&&b->held==ML_R&&(edge&PAD_R)&&b->magnitude>=50&&
       (b->gauge>=10||word(a,0x3330)==3||(word(a,10056)&0x400000u))&&!valor_link.moving){
        MLObservation input=observe(a,1);
        valor_link.moving=1;valor_link.elapsed=0;
        valor_link.heading=input.heading;valor_link.input_heading=input.input_heading;
    }
}
#endif
#if ML_WILDS_MOVES
enum {
    ART_SELECTED=10068, ART_SLOT_COUNT=3, ART_REQUEST=0x40,
    ART_COMMANDS=9460, ART_COMMAND_GROUP=100, ART_COMMAND_ACTION=102,
    ART_RETURN_SAMPLES=44, ART_POST_SAMPLES=24, ART_CANCEL_SAMPLES=28,
    ART_R_CHORD_SAMPLES=12,
    ART_SLIDE_SAMPLES=ART_POST_SAMPLES+ART_R_CHORD_SAMPLES
};

static unsigned world_native_art(void *a)
{
    unsigned group=u16(a,STATE),action=u16(a,ACTION);
    if(group!=16&&group!=0x4000)return 0;
    /* Sakura, Critical Juncture and Oasis each change native action
     * number before returning control. */
    if(group==16&&(action==54||action==61||action==62))return 1;
    if(group==0x4000&&action==11)return 1;
    for(unsigned slot=0;slot<ART_SLOT_COUNT;++slot){
        const void *command=(const void *)(uptr)word(a,ART_COMMANDS+slot*4);
        if(command&&(u16(command,ART_COMMAND_GROUP)&0x7fffu)==group&&
           u16(command,ART_COMMAND_ACTION)==action)return 1;
    }
    return 0;
}

static unsigned world_art_recovery(void *a)
{
    unsigned group=u16(a,STATE),action=u16(a,ACTION),motion=u16(a,MOTION)-0x500;
    float frame=real(a,FRAME);
    if(group==16){
        if(action>=58&&action<=60&&motion==613)return frame>=145.f;
        if(action==61&&motion==614)return frame>=103.f;
        if(action==62&&motion==515)return frame>=121.f;
        if(action>=55&&action<=57&&motion==568)return frame>=125.f;
        if(action==90&&motion==663)return frame>=333.f;
    }else if(group==0x4000){
        if(action==1&&motion==268)return frame>=61.f;
        if(action==2&&motion==271)return frame>=19.f;
        if((action==3||action==4||action==6||action==10)&&motion==257)return frame>=173.f;
        if(action==11&&motion==267)return frame>=281.f;
        if(action==7&&motion==264)return frame>=101.f;
        /* The defense arts apply their effect during the initial two binds. */
        if((action==8||action==9)&&(motion==262||motion==263))return 1;
    }
    return 0;
}

static unsigned world_art_chain(void *a)
{
    if(u16(a,STATE)!=16)return 0;
    unsigned action=u16(a,ACTION);
    if((action>=51&&action<=54)||action==62)return 1;
    if(action>=58&&action<=61)return 2;
    return 0;
}

static struct {
    void *owner;
    unsigned epoch,sample,keys,route,recovery,heading,input_heading,magnitude;
    unsigned r_pressed,r_length,iai_key,art_chain;
    float r_ticks;
} art_return_input;

static unsigned world_art_input_owner(void *a)
{
    return art_return_input.owner==a&&art_return_input.epoch==byte(a,AREA);
}

static unsigned world_art_route(void *a)
{
    return (u16(a,STATE)<<16)|u16(a,ACTION);
}

static struct {
    void *owner;
    unsigned epoch,sample,route;
} art_exit;

static struct {
    void *owner;
    unsigned epoch,sample,seen,exited;
} art_slide_from;

static void world_art_slide_seed(void *a)
{
    art_slide_from=(__typeof__(art_slide_from)){a,byte(a,AREA),sample_sequence,0,0};
}

static void world_art_slide_tick(void *a,unsigned art)
{
    if(art_slide_from.owner!=a)return;
    unsigned state=u16(a,STATE),action=u16(a,ACTION);
    unsigned idle=(state==1||state==2)&&action<=5;
    unsigned expired=sample_sequence-art_slide_from.sample>ART_SLIDE_SAMPLES;
    if(art_slide_from.epoch!=byte(a,AREA)||
       (art?art_slide_from.exited||(!art_slide_from.seen&&expired):expired)||
       (!art&&!idle&&!(state==16&&action==w_action(W_SLIDE)&&!art_slide_from.seen))){
        art_slide_from.owner=0;
    }else if(art){
        if(!art_slide_from.seen){
            world.wilds_slide_exit_sample=world.wilds_slide_exit_r=0;
            log_event("hunter_art_slide_link",(u32)(uptr)a,state,action,sample_sequence-art_slide_from.sample);
        }
        art_slide_from.seen=1;
        art_slide_from.sample=sample_sequence;
    }else if(art_slide_from.seen){
        art_slide_from.exited=1;
    }
}

static unsigned world_art_post_window(void *a)
{
    unsigned state=u16(a,STATE);
    return art_exit.owner==a&&art_exit.epoch==byte(a,AREA)&&
        sample_sequence-art_exit.sample<=ART_POST_SAMPLES&&
        (state==1||state==2)&&u16(a,ACTION)<=5&&!(word(a,AIR)&0x400)&&
        (int16_t)u16(a,HP)>0;
}

static unsigned world_art_keys(void *a)
{
    unsigned keys=held;
    if((keys==ML_R&&(byte(a,DRAWN)&1)&&!valor_active(a))||
       keys==(ML_R|ML_X)||keys==(ML_R|ML_A)||keys==(ML_R|ML_B)||
       (keys==ML_X&&crimson_available(a))||
       (keys==(ML_R|ML_Y)&&(ml_pad_raw_bits()&PAD_FOCUS)))return keys;
    return 0;
}

static unsigned world_art_draw_handoff(unsigned keys)
{
    return keys==(ML_R|ML_X)||keys==(ML_R|ML_A)||keys==(ML_R|ML_B)||keys==(ML_R|ML_Y);
}

static struct {
    void *owner;
    unsigned epoch,key,previous;
} art_iai;

static WDecision world_art_iai(void *a,const WObservation *o)
{
    if(art_iai.owner!=a)return (WDecision){0};
    if(art_iai.epoch!=o->base.epoch||world.move!=W_SHEATHE||!o->alive||
       !o->grounded||!o->drawn){
        art_iai.owner=0;
        return (WDecision){0};
    }
    unsigned held=o->base.held;
    unsigned fresh=held&~art_iai.previous;
    art_iai.previous=held;
    if(held&~(ML_R|ML_X|ML_B))art_iai.key=0;
    else if(fresh&&(held==ML_R||held==ML_X))art_iai.key=held;
    if(!art_iai.key||!w_iai_ready(&world,o))return (WDecision){0};
    WDecision d=w_iai_attack(&world,o,art_iai.key);
    if(d.kind==W_ENTER){
        log_event("hunter_art_iai",o->base.actor,art_iai.key,d.slot,(u32)o->base.animation_frame);
        art_iai.owner=0;
    }
    return d;
}

static struct {
    void *owner;
    unsigned epoch,sample,group,action;
} art_entry_blend;

#if ML_ART_FIXTURE
static volatile unsigned art_fixture_loadout;
static void world_art_fixture(void)
{
    if(!art_fixture_loadout)return;
    void *a=world_local_hunter();
    if(!local_supported(a))return;
    void *profile=(void *)(uptr)word(a,0x13c4);
    unsigned id=art_fixture_loadout;
    *(uint16_t *)((uptr)profile+576)=(uint16_t)id;
    ((void (*)(void *))(main_base+0x2878a0))(a);
    art_fixture_loadout=0;
    const void *command=(const void *)(uptr)word(a,ART_COMMANDS);
    log_event("hunter_art_fixture",id,u16(a,10078),
        command?u16(command,ART_COMMAND_GROUP):0,command?u16(command,ART_COMMAND_ACTION):0);
}
#endif

/* Read the pad even while a common art is outside the sword handler. */
#if ML_ASCENDANCE_PROTOTYPE
static void world_radiance_collect_art(void *a);
#endif
static void world_art_poll(void)
{
#if ML_ART_FIXTURE
    world_art_fixture();
#endif
    static unsigned previous;
    /* Releasing a modifier leaves the chord's partner held. Only a new
     * press may replace the buffered chord with a different command. */
    unsigned fresh=held&~previous;
    previous=held;
    if(!pad_valid)return;
    void *a=world_local_hunter();
    if(!local_supported(a))return;
    unsigned art=world_native_art(a),route=world_art_route(a);
#if ML_ASCENDANCE_PROTOTYPE
    if(art)world_radiance_collect_art(a);
#endif
    world_art_slide_tick(a,art);
    unsigned recovery=art&&world_art_recovery(a);
    unsigned art_chain=art?world_art_chain(a):0;
    if(recovery)
        art_exit=(__typeof__(art_exit)){a,byte(a,AREA),sample_sequence,route};
    else if(art_exit.owner==a&&(art?art_exit.route!=route:!world_art_post_window(a)))
        art_exit.owner=0;

    unsigned solo=world_art_input_owner(a)&&art_return_input.keys==ML_R;
    if(solo&&held==ML_R){
        art_return_input.r_length=sample_sequence-art_return_input.r_pressed;
        float delta=real(a,0x1c);
        if(delta>0&&delta<30)art_return_input.r_ticks+=delta;
        if(art_return_input.magnitude<50){
            MLObservation input=observe(a,1);
            if(input.magnitude>=50){
                art_return_input.heading=input.heading;
                art_return_input.input_heading=input.input_heading;
                art_return_input.magnitude=input.magnitude;
            }
        }
    }
    unsigned post=!art&&world_art_post_window(a);
    if(world_art_input_owner(a)&&art_return_input.art_chain){
        if(art_return_input.art_chain==art_chain){
            /* Keep the input across Sakura's passes or Mirror's counter. */
            if(art_return_input.route!=route)art_return_input.recovery=0;
            art_return_input.route=route;
            art_return_input.sample=sample_sequence;
        }else if(!post){
            art_return_input.owner=0;
        }
    }
    unsigned pending=world_art_input_owner(a)&&sample_sequence-art_return_input.sample<=ART_RETURN_SAMPLES&&
        ((art&&art_return_input.route==route)||post);
    if(pending&&recovery&&!art_return_input.recovery){
        art_return_input.recovery=1;
        art_return_input.sample=sample_sequence;
        log_event("hunter_art_return_input",(u32)(uptr)a,u16(a,MOTION),(u32)real(a,FRAME),art_return_input.keys);
    }
    if((!art&&!post)||!held)return;
    /* R+B still owns the art exit when the player releases it and queues
     * their Iai key. A lone R or X without a sheathe chord keeps its old route. */
    if(pending&&art_return_input.keys==(ML_R|ML_B)&&fresh&&(held==ML_R||held==ML_X)){
        art_return_input.iai_key=held;
        art_return_input.sample=sample_sequence;
        art_return_input.recovery|=post;
        return;
    }
    if(!fresh&&(!art_return_input.owner||art_return_input.recovery||
       art_return_input.keys!=held))return;

    unsigned keys=world_art_keys(a);
    if(!keys){
        /* Pressing R alone can precede the second half of a chord. */
        if(held!=ML_R)art_return_input.owner=0;
        return;
    }
    if(post&&keys!=ML_X&&!(byte(a,DRAWN)&1)&&!world_art_draw_handoff(keys))return;
    if(!fresh&&(!recovery||!world_art_input_owner(a)||art_return_input.route!=route))return;
    recovery|=post;
    MLObservation input=observe(a,1);
    if(keys==ML_R&&solo&&!fresh&&art_return_input.magnitude>=50){
        input.heading=art_return_input.heading;
        input.input_heading=art_return_input.input_heading;
        input.magnitude=art_return_input.magnitude;
    }
    art_return_input=(__typeof__(art_return_input)){
        a,byte(a,AREA),sample_sequence,keys,route,recovery,input.heading,input.input_heading,input.magnitude,
        keys==ML_R&&solo&&!fresh?art_return_input.r_pressed:sample_sequence,
        keys==ML_R&&solo&&!fresh?art_return_input.r_length:0,0,art_chain,
        keys==ML_R&&solo&&!fresh?art_return_input.r_ticks:0};
    if(recovery)
        log_event("hunter_art_return_input",(u32)(uptr)a,u16(a,MOTION),
            (u32)real(a,FRAME),keys);
}

static unsigned world_art_return(void *a,const MLObservation *o,unsigned art)
{
    if(art){
        if(art_return_input.owner==a&&art_return_input.recovery&&
           art_return_input.route!=world_art_route(a)&&
           (!art_return_input.art_chain||art_return_input.art_chain!=world_art_chain(a)))
            art_return_input.owner=0;
        return 0;
    }
    unsigned keys=art_return_input.keys;
    unsigned queued=world_art_input_owner(a)&&art_return_input.recovery&&
        sample_sequence-art_return_input.sample<=ART_RETURN_SAMPLES;
    unsigned state=u16(a,STATE);
    queued=queued&&(state==1||state==2)&&o->native_action<=5&&!(o->held&~(keys|art_return_input.iai_key))&&
        !(word(a,AIR)&0x400)&&(keys!=ML_X||crimson_available(a));
    if(queued&&keys==ML_R&&o->held==ML_R&&
       (art_return_input.r_length<ART_R_CHORD_SAMPLES||
        (art_return_input.magnitude<50&&art_return_input.r_ticks<W_SPIRIT_FULL_HOLD_SECONDS*60.f)))
        return 0;
    art_return_input.owner=0;
    return queued?keys:0;
}
#endif
#if ML_ASCENDANCE_PROTOTYPE
enum { RADIANCE_SOURCE_NONE,RADIANCE_SOURCE_FREE,RADIANCE_SOURCE_NATIVE,
       RADIANCE_SOURCE_MOVE,RADIANCE_SOURCE_ART,RADIANCE_BUFFER_FRAMES=66,
       RADIANCE_BUFFER_SAMPLES=180 };
static struct {
    void *owner;
    unsigned area,sample,kind,route,motion;
    float frame;
} radiance_input;

static unsigned world_radiance_source(void *a)
{
    unsigned state=u16(a,STATE),action=u16(a,ACTION),motion=u16(a,MOTION);
    if(world.move){
        if(state==16&&world_bound&&world_bound_actor==(u32)(uptr)a&&
           action==world_bound_action&&motion==0x500+world.slot&&
           world.move!=W_SHEATHE&&world.move!=W_SHEATHE_END)
            return RADIANCE_SOURCE_MOVE;
        return RADIANCE_SOURCE_NONE;
    }
    if((state==1||state==2)&&action<=5)return RADIANCE_SOURCE_FREE;
    if(world_native_art(a))return RADIANCE_SOURCE_ART;
    if(w_native_roll(state,action)||(state==16&&
       (w_guild_attack(action)||w_guild_source(state,action,motion))))
        return RADIANCE_SOURCE_NATIVE;
    return RADIANCE_SOURCE_NONE;
}

static unsigned world_radiance_focus_allowed(void *a)
{
    return !(ml_pad_raw_bits()&PAD_FOCUS)||
        (world.move==W_CHIJIN&&world_radiance_source(a)==RADIANCE_SOURCE_MOVE);
}

static unsigned world_radiance_input_reserved(void *a,unsigned raw,unsigned keys)
{
    return (raw&PAD_BOOST)&&keys&&!(keys&~RADIANCE_INPUT)&&
        world_radiance_focus_allowed(a)&&ascendance_radiance_available(a)&&
        world_ready(a)&&world_radiance_source(a);
}

static void world_radiance_save(void *a,unsigned kind,unsigned motion,float frame)
{
    unsigned route=world_art_route(a);
    radiance_input=(__typeof__(radiance_input)){a,byte(a,AREA),sample_sequence,kind,route,motion,frame};
    log_event("ascendance_radiance_buffer",(u32)(uptr)a,route,motion,(u32)(frame*1000));
}

static void world_radiance_collect_art(void *a)
{
    unsigned raw=ascendance_pad_bits(a),keys=observe(a,0).held;
    if(keys!=RADIANCE_INPUT||!world_radiance_input_reserved(a,raw,keys))return;
    unsigned route=world_art_route(a),motion=u16(a,MOTION);
    if(radiance_input.owner==a&&radiance_input.kind==RADIANCE_SOURCE_ART&&
       radiance_input.route==route&&radiance_input.motion==motion){
        radiance_input.sample=sample_sequence;
        radiance_input.frame=real(a,FRAME);
    }else if(raw&~wilds_input_previous&(PAD_BOOST|PAD_X|0x2000u)){
        world_radiance_save(a,RADIANCE_SOURCE_ART,motion,real(a,FRAME));
    }
}

static unsigned world_radiance_attack_window(unsigned source,float frame)
{
    const WEvent *event=w_event(source,frame);
    if(!event)return 0;
    return ((event->input[WI_X.word]>>WI_X.shift)|
        (event->input[WI_A.word]>>WI_A.shift)|
        (event->input[WI_SHEATHE.word]>>WI_SHEATHE.shift))&2u;
}

static unsigned world_radiance_start(void *a,const WObservation *o,unsigned raw,unsigned edge)
{
    if(!ascendance_radiance_available(a)||!o->alive||!o->grounded||!o->drawn||
       !o->resources_ready||!world_radiance_focus_allowed(a)||(o->base.held&~RADIANCE_INPUT)){
        if(radiance_input.owner==a)
            log_event("ascendance_radiance_drop",(u32)(uptr)a,1,o->base.held,
                (u32)(o->base.animation_frame*1000));
        radiance_input.owner=0;
        return 0;
    }
    if(!(o->base.simulation_delta>0))return 0;
    unsigned kind=world_radiance_source(a),route=world_art_route(a),motion=o->base.motion;
    unsigned full=(raw&PAD_BOOST)&&o->base.held==RADIANCE_INPUT;
    unsigned sample=o->base.sample_sequence;
    if(full&&(edge&(PAD_BOOST|PAD_X|0x2000u))&&kind)
        world_radiance_save(a,kind,motion,o->base.animation_frame);
    unsigned same=kind==radiance_input.kind&&route==radiance_input.route&&
        motion==radiance_input.motion;
    if(radiance_input.owner!=a||radiance_input.area!=o->base.epoch||
       !kind||(!same&&kind!=RADIANCE_SOURCE_FREE)){
        if(radiance_input.owner==a)
            log_event("ascendance_radiance_drop",(u32)(uptr)a,2,route,motion);
        radiance_input.owner=0;
        return 0;
    }
    float frame=o->base.animation_frame;
    if(full&&same){
        radiance_input.sample=sample;
        radiance_input.frame=frame;
    }
    if(sample-radiance_input.sample>RADIANCE_BUFFER_SAMPLES||
       (same&&frame-radiance_input.frame>RADIANCE_BUFFER_FRAMES)){
        log_event("ascendance_radiance_drop",(u32)(uptr)a,3,sample-radiance_input.sample,
            (u32)(o->base.animation_frame*1000));
        radiance_input.owner=0;
        return 0;
    }

    unsigned ready=kind==RADIANCE_SOURCE_FREE||o->base.motion_finished;
    if(kind==RADIANCE_SOURCE_NATIVE)
        ready|=world_radiance_attack_window(w_guild_source(o->state,o->base.native_action,motion),frame);
    if(kind==RADIANCE_SOURCE_ART)ready=world_art_recovery(a);
    if(kind==RADIANCE_SOURCE_MOVE){
        const WildsClip *clip=wilds_clip(world.source);
        ready|=clip?(clip->cancel&&frame>=clip->cancel):world_radiance_attack_window(world.source,frame);
        if(world.source==170){
            const WEvent *event=w_event(world.source,frame);
            ready|=event&&((event->input[WI_ROLL.word]>>WI_ROLL.shift)&2u);
        }
        if(world.source==908||world.source==909||world.source==915)
            ready=world.source!=908||(frame>=20&&frame<50);
    }
    if(ready)radiance_input.owner=0;
    return ready;
}
#endif
static unsigned world_request(void *a,unsigned state,unsigned action,unsigned flags)
{
    MLObservation raw=observe(a,0);w_keys_tick(&world.keys,&raw);
    if(world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||(int16_t)u16(a,HP)<=0)return 0;
#if ML_WILDS_MOVES
    unsigned bits=
#if ML_ASCENDANCE_PROTOTYPE
        ascendance_pad_bits(a);
#else
        ml_pad_raw_bits();
#endif
    unsigned edge=bits&~wilds_input_previous;
#if ML_ASCENDANCE_PROTOTYPE
    if(world_radiance_input_reserved(a,bits,raw.held)&&
       ((state==16&&(w_guild_attack(action)||action==41))||
        ((state==1||state==2)&&action<=14&&action>5)))
        return 1;
#endif
    /* The native roll cancel can be evaluated between pad samples. Only
     * replace its sword request when X is held or buffered by that roll. */
    unsigned current=u16(a,STATE),current_action=u16(a,ACTION);
    unsigned native_art=world_native_art(a);
    if(!world.move&&w_charged_three_recovery(&world,sample_sequence)&&
       (current==1||current==2)&&current_action<=5&&!(raw.held&~ML_R)&&
       state==16&&(action==16||action==17))return 1;
    if(!world.move&&w_moving_spirit_recovery(&world,sample_sequence)&&
       (current==1||current==2)&&current_action<=5&&!(raw.held&~ML_R)&&
       (raw.magnitude>=50||world.wilds_slide_exit_r)&&state==16&&(action==16||action==17))return 1;
    unsigned art_solo=world_art_input_owner(a)&&art_return_input.keys==ML_R&&
        art_return_input.recovery&&sample_sequence-art_return_input.sample<=ART_RETURN_SAMPLES;
    if(native_art||world_art_post_window(a)||art_solo){
        unsigned art_keys=world_art_keys(a);
        if(world_art_input_owner(a)&&art_return_input.iai_key&&
           sample_sequence-art_return_input.sample<=ART_RETURN_SAMPLES)
            art_keys=art_return_input.keys;
        if(!art_keys&&(art_solo||(world_art_input_owner(a)&&art_return_input.route==world_art_route(a))))
            art_keys=art_return_input.keys;
        /* A native cancel can survive release of the pad edge. The art
         * buffer owns these commands until the first valid handoff. */
        if(art_keys==ML_R){
            if((native_art||((current==1||current==2)&&current_action<=5))&&
               state==16&&(action==16||action==17))return 1;
        }else if(art_keys&&(native_art||art_keys==ML_X||(byte(a,DRAWN)&1)||world_art_draw_handoff(art_keys))&&
           ((state==16&&(w_guild_attack(action)||action==41))||
            (state==1&&(action==2||action==6)&&(art_keys&ML_B))))return 1;
        if(native_art)return 0;
    }
    /* Start moving Spirit I only from the native draw-Spirit request.
     * Sprint stays native even while the stick and R are held. */
    if(!world.move&&state==16&&(action==20||action==21)&&!(byte(a,DRAWN)&1)){
        WObservation o={.base=raw,.state=current,.alive=1,.grounded=!(word(a,AIR)&0x400),
            .resources_ready=world_ready(a),.rank=crimson_available(a)?3:0};
        if(w_sheathed_slide(&o)){
            wilds_slide_draw_input=observe(a,1);
            world_wilds_pending=W_SLIDE;
            return 1;
        }
    }
    unsigned red_roll=w_native_roll(current,current_action)||wilds_roll_tail>0;
    unsigned upper=state==16&&(action<=7||action==25||action==41);
    unsigned spirit_one=!world.move&&w_spirit_one_crimson_source(current,current_action,u16(a,MOTION));
    if(!spirit_one&&!world.move&&red_roll&&upper&&(byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&
       crimson_available(a)&&!(raw.held&~(ML_X|ML_B))&&
       ((raw.held&ML_X)||wilds_roll_x||wilds_roll_input)&&world_ready(a)){
        if(!wilds_roll_x&&!wilds_roll_input){
            MLObservation input=observe(a,1);
            world_roll_crimson_input(&input);
        }
        wilds_roll_input=1;wilds_roll_x=0;world_wilds_pending=W_CHIJIN;
        log_event("wilds_roll_crimson",(u32)(uptr)a,current_action,action,raw.held);
        return 1;
    }

    /* GU's X insertion after Spirit I is action 22, separate from the
     * ordinary slash requests. It can execute before our F56 combo tick
     * and after the raw X tap has ended. The request itself owns the press. */
    if(spirit_one&&state==16&&action==22&&crimson_available(a)&&world_ready(a)&&
       (byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)){
        MLObservation input=observe(a,1);
        w_reserve_spirit_crimson(&world,&input);
        log_event("wilds_spirit_crimson_reserve",(u32)(uptr)a,current_action,action,(u32)(real(a,FRAME)*1000));
        return 1;
    }
    unsigned valor_spirit=!world.move&&valor_active(a)&&w_valor_spirit_one(current,current_action);
    unsigned guard_r=valor_active(a)&&valor_guard_pose(a)&&
        (world.spirit_crimson_guard_r||world.spirit_crimson_action!=current_action);
    unsigned red_native=!world.move&&current==16&&
        (spirit_one||valor_spirit||w_valor_chord(current,current_action,u16(a,MOTION),100000.f,ML_X,0)==W_CHIJIN);
    if(red_native&&(upper||(valor_spirit&&state==16&&action==22))&&
       crimson_available(a)&&world_ready(a)&&(byte(a,DRAWN)&1)&&
       !(word(a,AIR)&0x400)&&(raw.held==ML_X||
       (world.spirit_crimson_pending&&!(raw.held&~(ML_R|ML_X)))||
       ((spirit_one||valor_spirit||guard_r)&&raw.held==(ML_R|ML_X)&&(world.spirit_crimson_held&ML_R))))return 1;
    /* Reserve a fresh chord before GU turns its B edge into a native roll.
     * Hurt, knockback, death and scene changes still use the native path. */
    if(!spirit_one&&!world.move&&!(word(a,AIR)&0x400)&&world_ready(a)&&
       ((state==16&&(w_guild_attack(action)||action==41))||(state==1&&action<=14))){
        unsigned current=u16(a,STATE),act=u16(a,ACTION);
        unsigned valid=((current==1||current==2)&&act<=5)||(current==16&&act<154&&real(a,FRAME)>=24);
        unsigned mv=0;
        if(byte(a,DRAWN)&1){
            unsigned ordinary_x=upper&&current==16&&current_action<108&&!valor_guard_pose(a);
            if(crimson_available(a)&&valid&&raw.held==ML_X&&
               ((!valor_active(a)&&(edge&PAD_X))||ordinary_x)){
                mv=W_CHIJIN;wilds_roll_x=0;
            }
        }else if((edge&PAD_X)&&wilds_input_actor==(u32)(uptr)a&&
            wilds_input_epoch==byte(a,AREA)&&((state==16&&action<=5)||(state==1&&action==1))){
            WObservation o={.base=raw,.state=current,.alive=1,.grounded=1,.resources_ready=1,
                .rank=crimson_available(a)?3:0,.rising_sprint=world_rising_sprint(a)};
            if(world_red_draw_input(&o)&&w_red_draw(&o))mv=W_CHIJIN;
        }
        if(mv){world_wilds_pending=mv;log_event("wilds_input_reserve",(u32)(uptr)a,mv,state,action);return 1;}
    }
    if(!world.move&&(state==16||w_native_roll(state,action))&&valor_draw_cut(a)){
        valor_link_collect(a,&raw,edge,5);
        if(valor_link.keys)return 1;
        if(state==16&&(raw.held&~ML_Y)==ML_R){
            if(!valor_link.packet){
                valor_link.packet=action;valor_link.flags=flags;valor_link.elapsed=0;
            }
            return 1;
        }
    }
    if(!world.move&&valor_active(a)){
        if(raw.held==(ML_X|ML_A)){
            valor_x.actor=0;valor_link.keys=valor_guard.chord=0;
            if(world_wilds_pending==W_CHIJIN)world_wilds_pending=0;
            return 0;
        }
        if(valor_x.actor==a&&raw.held==ML_X&&upper)return 1;
        unsigned packet=(state==16&&(w_guild_attack(action)||(action>=87&&action<=144)))||
                        w_native_roll(state,action)||(state==1&&action==2);
        if(!packet||!(byte(a,DRAWN)&1)||(word(a,AIR)&0x400)||!world_ready(a))return 0;
        unsigned pose=valor_link_pose(a);
        valor_link_collect(a,&raw,edge,pose);
        unsigned spirit=state==16&&((action>=16&&action<=19)||(action>=23&&action<=27)||action==30||
            (action>=48&&action<=50)||action==116||action==117||action==130||(action>=134&&action<=141));
        if(pose&&valor_link.actor==a){
            if(valor_link.keys)return 1;
            if(valor_link.moving&&spirit&&!(action==130&&(word(a,10056)&0x400000u)))return 1;
            if((pose==1||pose==3)&&(word(a,10056)&0x400000u)&&state==16&&
               ((raw.held==ML_X&&(edge&PAD_X))||raw.held==(ML_R|ML_A)))return 1;
            /* A leading R in a native cancel can request Spirit before A/X
             * is sampled. Keep that exact request if R stays on its own. */
            if((pose==2||pose==3)&&spirit&&(raw.held==ML_R||valor_link.packet)){
                if(!valor_link.packet){valor_link.packet=action;valor_link.flags=flags;valor_link.elapsed=0;}
                return 1;
            }
        }
        if(valor_guard_pose(a)){
            valor_guard_input(a,raw.held,edge);
            if(valor_guard.chord)return 1;
            if(state==16&&(action==116||action==136)&&(raw.held==ML_R||valor_guard.spirit)){
                if(!valor_guard.spirit){
                    valor_guard.early_r=0;valor_guard.spirit=action;valor_guard.flags=flags;
                    valor_guard.deadline=real(a,FRAME)+6.0f;
                }
                return 1;
            }
        }
        return (edge&0xf200u)&&w_valor_chord(u16(a,STATE),u16(a,ACTION),u16(a,MOTION),
                                              real(a,FRAME),raw.held,bits&PAD_FOCUS);
    }
    /* GU heavy hits bypass the light-flinch predicate. Its damage handler
     * also submits KO, paralysis, sleep and other status actions in state 64. */
    if(world_action_armor_depth&&(world.move==W_RELEASE||world.move==W_FOCUS)&&u16(a,STATE)==16&&
       u16(a,ACTION)==w_action(world.move)&&state==64&&
       action!=26&&action!=39&&action!=53&&action!=102){
        log_event(world.move==W_FOCUS?"wilds_focus_armor":"wilds_release_armor",
            (u32)(uptr)a,action,(u32)real(a,FRAME),byte(a,0x3216));
        return 1;
    }
    if(!world.move&&(byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&world_ready(a)){
        unsigned src=w_guild_source(u16(a,STATE),u16(a,ACTION),u16(a,MOTION));
        if(!valor_active(a)&&state==16&&word(a,0x3330)!=3&&
           (w_spirit_charge_source(src)||src==112||w_ordinary_charge_source(src))&&
           raw.gauge<w_spirit_link_cost(action))return 1;
        unsigned ordinary=src==1||src==101||src==102||src==103||src==105||src==107;
        unsigned window=src==1||(ordinary&&real(a,FRAME)>=24);
        const WEvent *cancel=w_event(src,real(a,FRAME));
        unsigned sheath=cancel&&((cancel->input[WI_SHEATHE.word]>>WI_SHEATHE.shift)&3)==3;
        unsigned packet=(state==16&&w_guild_attack(action))||((state==1||state==2)&&action<=14);
        if(packet&&window&&raw.held==(ML_R|ML_B)&&
           (src==1||sheath))return 1;
        /* Keep a leading directional R undecided until the chord grace period
         * has elapsed. Only the core may dispatch its moving opener. */
        unsigned spirit_packet=(action>=16&&action<=19)||action==23||action==24||action==26||action==27||action==30||
            (action>=48&&action<=50);
        /* A queued ordinary Spirit tap belongs to the bridge until release.
         * GU may request the same successor on that exact update. */
        if(state==16&&spirit_packet&&world.wilds_r_pending&&
           w_spirit_charge_source(src)&&!(raw.held&~ML_R)){
            world_r_gate.active=0;return 1;
        }
        /* Keep the exact native tap, including fade-slash and insertion
         * variants, until release distinguishes it from a charge. */
        if(world_r_gate.active&&world_r_gate.button==WORLD_R_WAIT_RELEASE&&(raw.held&~ML_R))world_r_gate.active=0;
        if(!valor_active(a)&&state==16&&spirit_packet&&(src==112||w_ordinary_charge_source(src))&&
           raw.magnitude<50&&raw.held==ML_R&&cancel&&((cancel->input[WI_R.word]>>WI_R.shift)&3)){
            world_r_gate=(__typeof__(world_r_gate)){.active=1,.actor=(u32)(uptr)a,.epoch=byte(a,AREA),
                .motion=u16(a,MOTION),.generation=bind_generation,.action=action,.flags=flags,.button=WORLD_R_WAIT_RELEASE,
                .owner_action=u16(a,ACTION),.owner_state=u16(a,STATE)};
            return 1;
        }
        if(state==16&&spirit_packet&&raw.held==ML_R&&w_spirit_charge_source(src))return 1;
        if(state==16&&spirit_packet&&window&&raw.magnitude>=50&&raw.held==ML_R)return 1;
        if(u16(a,ACTION)==30&&src==109&&((state==1||state==2)&&action<=5)&&
           real(a,FRAME)<w_clip(109)->length-1)return 1;
    }

#endif
    if(world_floor_depth&&world_air_owner(a)&&state==64&&action==33)return 1;
    /* Only the native floor solver may resolve a short step under a
     * bound ground attack. Damage, true falls and Helm retain their paths. */
    unsigned roll_step=0;
#if ML_ASCENDANCE_PROTOTYPE
    if(state==4)roll_step=ascendance_resheathe_roll_active(a);
#endif
    if(!world_damage_depth&&state==4&&
       ((world_floor_depth&&action==1&&
        ((w_ground_step_move(world.move)&&world_bound&&world_bound_actor==(u32)(uptr)a&&
         u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move)&&u16(a,MOTION)==0x500+world.slot)
#if ML_ASCENDANCE_PROTOTYPE
        ||(w_ground_step_move(world.move)&&ascendance_resheathe_step_follow(a))
#endif
        ||roll_step))
       ||(action==14&&roll_step&&w_native_roll(u16(a,STATE),u16(a,ACTION))))){
        float floor=real(a,0x10f0),drop=real(a,0x44)-floor;
        if(w_ground_step_height(drop)){
#if ML_ASCENDANCE_PROTOTYPE
            ascendance_step_start(a,real(a,0x44),drop);
#endif
            set_real(a,0x44,floor);set_real(a,0x1014,0);
            put(a,AIR,word(a,AIR)&~0x400u);
            *(unsigned char *)((uptr)a+0x2505)=0;
#if ML_ASCENDANCE_PROTOTYPE
            if(action==14)ascendance_step_floor(a);
#endif
            log_event("world_step_down",(u32)(uptr)a,(u32)(drop*100),action,(u32)real(a,FRAME));
            return 1;
        }
    }
    if(world_damage_depth&&world_armor>0&&(int16_t)u16(a,HP)>0&&
       (state==4||state==8||state==64))return 1;
#if ML_WORLD_INPUT_VM
    unsigned attack=state==16&&w_guild_attack(action);
    /* Native stick+B uses side rolls 13/14 before the combo VM runs. */
    unsigned roll=w_native_roll(state,action);
    if(world.move){
        if((attack||roll||((state==1||state==2)&&action<=5)) &&
           u16(a,STATE)==16 && u16(a,ACTION)==w_action(world.move))return 1;
        world_physics_end(a);
        world_commit(a,w_leave(&world));world_cleanup(a);
        world.controls=(WControls){0};
        log_event("world_interrupt",(u32)(uptr)a,state,action,0);
        return 0;
    }
    if(world_r_gate.active&&(!attack||action!=world_r_gate.action))world_r_gate.active=0;
    if(!(attack||roll)||!(byte(a,DRAWN)&1)||
       (word(a,AIR)&0x400)||!world_ready(a))return 0;
    unsigned source=w_guild_source(u16(a,STATE),u16(a,ACTION),u16(a,MOTION));
    if(!source)return 0;
#if ML_WILDS_MOVES
    /* GU's stock round auto-sheathes. The Wilds R chain uses the ported
     * round instead, preserving drawn state and the following combo context. */
    if(attack&&source==108&&action==30)return 1;
#endif
    /* GU can offer III before the pad update or replay it after release.
     * The command VM alone dispatches solo R from these ordinary attacks. */
    if(attack&&(action==16||action==17||action==18||action==19||action==26||action==27)&&
       w_guild_spirit_opener(u16(a,ACTION),u16(a,MOTION),u16(a,0x250c)))return 1;
    unsigned partner=held&(ML_X|ML_A|ML_B|ML_Y|ML_ZR);
    const WEvent *event=w_event(source,source==1?0:real(a,FRAME));
    /* GU can offer the draw-upper before the command VM sees this pad
     * sample. The same thrust's X edge owns the native combo upper. */
    if(attack&&action==7&&((source==105&&u16(a,ACTION)==4)||(source==112&&u16(a,ACTION)==22))&&event&&
       ((event->input[WI_X.word]>>WI_X.shift)&3)&&
       ((held&ML_X)||w_command_has(world.controls.commands.buffered,25)||
        w_command_has(world.controls.commands.executable,25)))return 1;
    /* GU may submit a buffered solo after both physical buttons were
     * released, but before the World partner pulse or execute window.
     * Reserve only a chord actually collected on this same primary motion. */
    const WControls *controls=&world.controls;
    if(world_r_gate.active&&world_r_gate.actor==(u32)(uptr)a&&
       world_r_gate.generation==bind_generation&&world_r_gate.motion==u16(a,MOTION)&&
       world_r_gate.owner_action==u16(a,ACTION))return 1;
    if(event&&world.input_motion==u16(a,MOTION)&&!(held&(ML_Y|ML_ZR))){
#if ML_WILDS_MOVES
        if(attack&&!(held&~(ML_R|ML_A))&&world.foresight_press.motion==u16(a,MOTION)&&
           world.foresight_press.action==u16(a,ACTION)&&world.foresight_press.generation==bind_generation&&
           w_foresight_stage(source,event))return 1;
#endif
        static const unsigned ids[]={53,60,24};
        for(unsigned i=0;i<(source==1?1u:3u);++i){
            unsigned id=ids[i];
            WInput input=i==0?WI_THRUST:i==1?WI_FORESIGHT:WI_SHEATHE;
            if(!(i==1?w_foresight_stage(source,event):((event->input[input.word]>>input.shift)&3)))continue;
            if(world.gu_chord_wait==id||w_command_has(controls->commands.buffered,id)||
               w_command_has(controls->commands.executable,id))return 1;
        }
    }
    /* A late native solo can be submitted on the leading physical edge.
     * In an open chord window keep that exact GU packet until the leading
     * button's World confirmation, or until its overlapping chord wins. */
    if(attack&&(u16(a,STATE)==16||source==1)&&event&&world.input_motion==u16(a,MOTION)&&
       (held==ML_R||held==ML_X||held==ML_A)){
        unsigned button=held==ML_R?2:held==ML_A?1:0;
        WInput input=held==ML_A?WI_FORESIGHT:WI_THRUST;
        unsigned open=(held==ML_A?(w_foresight_stage(source,event)&2):((event->input[input.word]>>input.shift)&2))||
            (held==ML_R&&(((event->input[WI_SHEATHE.word]>>WI_SHEATHE.shift)|
                           w_foresight_stage(source,event))&2));
        const WButton *key=controls->buttons+button;
        /* Idle has the global execute bit, not per-command TIML bits. */
        if(source==1)open=held==ML_R||held==ML_X;
        if(open&&!key->held&&(key->waiting||!key->raw)){
            world_r_gate.active=1;world_r_gate.actor=(u32)(uptr)a;world_r_gate.epoch=byte(a,AREA);
            world_r_gate.motion=u16(a,MOTION);world_r_gate.generation=bind_generation;
            world_r_gate.action=action;world_r_gate.flags=flags;world_r_gate.button=button;
            world_r_gate.owner_action=u16(a,ACTION);world_r_gate.owner_state=u16(a,STATE);return 1;
        }
    }
    if(!(held&ML_R))return 0;
    if(source==1)return partner==ML_X;
    WInput input;
    if(partner==ML_X)input=WI_THRUST;
    else if(partner==ML_A)input=WI_FORESIGHT;
    else if(partner==ML_B)input=WI_SHEATHE;
    else return 0;
    return event && (partner==ML_A?w_foresight_stage(source,event):((event->input[input.word]>>input.shift)&3));
#else
    if(world.move){
        if((state==1&&action<=14)||state==2||state==16)return 1;
        world_commit(a,w_leave(&world));world_cleanup(a);
        log_event("world_interrupt",(u32)(uptr)a,state,action,0);
    }
    unsigned source=w_source(u16(a,MOTION));
    const WEvent *event=w_event(source,real(a,FRAME));
    if(!world.move&&u16(a,STATE)==16&&state==16&&action==16&&
       (source==105||source==107)&&held&ML_A&&!(held&(ML_X|ML_B))&&event&&
       ((event->input[WI_FORESIGHT.word]>>WI_FORESIGHT.shift)&3)&&world_ready(a)){
        /* A native buffered R is still a real command if its tap ended
         * before this request. A held A upgrades it to the World chord. */
        world.queued=W_FORESIGHT;world.queued_mask=ML_R|ML_A;world.queued_source=source;
        world.queued_word=WI_FORESIGHT.word;world.queued_shift=WI_FORESIGHT.shift;
        world.queued_heading=u16(a,HEADING);
        return 1;
    }
    /* GU can publish its buffered X request after the raw tap has ended.
     * Its native permission is authoritative for this ordinary GU motion.
     * Redirect draw-upper7 to combo-upper25, preserving the native timing
     * and current facing rather than inserting draw prep116 / a re-aim. */
    if(!world.move&&source==105&&u16(a,STATE)==16&&u16(a,ACTION)==4&&
       state==16&&action==7&&world_ready(a)&&!(word(a,AIR)&0x400)){
        if(held&ML_R)return 1;
        world.queued=0;world.consumed|=ML_X;
        log_event("world_loop_native",(u32)(uptr)a,action,25,(u32)(real(a,FRAME)*1000));
        world_commit(a,(WDecision){.kind=W_NATIVE,.native_action=25,.heading=u16(a,HEADING)});
        return 1;
    }
    if(!world.move&&u16(a,STATE)==16&&state==16&&action>=16&&action<154&&
       source>=101&&source<=110&&source!=104&&source!=106&&source!=108&&
       world_ready(a)&&!(word(a,AIR)&0x400)&&event&&
       ((event->input[WI_R.word]>>WI_R.shift)&3)&&
       (held==ML_R||(world.queued==0x300&&world.queued_source==source)))return 1;
    if(world_r_gate.active&&world_r_gate.actor==(u32)(uptr)a&&world_r_gate.epoch==byte(a,AREA)&&
       world_r_gate.motion==u16(a,MOTION)&&world_r_gate.generation==bind_generation&&
       state==16&&action==world_r_gate.action)return 1;
    if(!world.move&&u16(a,STATE)==16&&source&&event&&world_ready(a)&&
       !(word(a,AIR)&0x400)&&state==16&&action>=16&&action<154&&
       held==ML_R&&((event->input[WI_FORESIGHT.word]>>WI_FORESIGHT.shift)&3)&&
       (!(world.keys.ready&ML_R)||((event->input[WI_R.word]>>WI_R.shift)&3)!=3)){
        if(!world_r_gate.active){
            world_r_gate.active=1;world_r_gate.actor=(u32)(uptr)a;world_r_gate.epoch=byte(a,AREA);
            world_r_gate.motion=u16(a,MOTION);world_r_gate.generation=bind_generation;
            world_r_gate.action=action;world_r_gate.flags=flags;
            log_event("world_chord_wait",(u32)(uptr)a,action,u16(a,MOTION),(u32)real(a,FRAME));
        }
        return 1;
    }
    if(!world.move&&u16(a,STATE)==16&&state==16&&action<154&&
       !(held&(ML_R|ML_B|ML_Y))&&(held&(ML_X|ML_A))!=(ML_X|ML_A)&&
       ((source==105&&(held&ML_X))||(source==107&&(held&ML_A))))return 1;
    /* Native R can submit spirit-I before the common update when gauge>0.
     * Reserve the neutral R+X gesture at that request boundary as well. */
    if((held&(ML_R|ML_X))==(ML_R|ML_X)&&!(held&(ML_A|ML_B))&&
       (byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&world_ready(a)&&
       (u16(a,STATE)==1||u16(a,STATE)==2)&&u16(a,ACTION)<=5&&state==16)return 1;
    /* Reserve only a recognized source combo; native X+A fade stays native. */
    unsigned selection=(held&ML_R)&&((held&(ML_X|ML_A|ML_B))!=0);
    if(selection&&w_source(u16(a,MOTION))&&u16(a,STATE)==16&&
       ((state==16&&action<154)||(state==1&&(action==0||action==6))))return 1;
    return 0;
#endif
}
#if ML_DEBUG_FORCE
static unsigned world_debug_pending;
/* 调试：骨架取样。按招式帧分段（每 40 帧一处）把关节矩阵的位置部分打到日志里，
 * 用来在不看画面的情况下比对/识别骨骼（pose1..pose3 = 位置候选 + 旋转首列）。 */
static unsigned j_dummy;
static void world_pose_probe(void *a)
{
    static unsigned actor,motion,seen;
    unsigned m=u16(a,MOTION);
    if(!world.move||m<0x500u)return;
    unsigned snap=(unsigned)(real(a,FRAME)/40.0f);
    if((u32)(uptr)a!=actor||m!=motion){actor=(u32)(uptr)a;motion=m;seen=0xffffffffu;}
    if(snap==seen||snap>8u)return;
    seen=snap;
    log_event("posesnap",(u32)(uptr)a,(m<<8)|snap,(u32)(real(a,FRAME)*1000.0f),(u32)j_dummy);
    union{float f;u32 u;}v;
    unsigned count=snap==0u?256u:24u;
    for(unsigned j=0;j<count;++j){
        const float *p=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,j);
        unsigned tag=(m<<8)|(snap<<4)|j;
        if(!p){log_event("pose",(u32)(uptr)a,tag,0xdeadbeefu,0);continue;}
        v.f=p[12];u32 a0=v.u;v.f=p[13];u32 a1=v.u;v.f=p[14];u32 a2=v.u;
        log_event("pose1",(u32)(uptr)a,tag,a0,a1);
        log_event("pose2",(u32)(uptr)a,tag,a2,0);
    }
}
#endif

#if ML_WILDS_MOVES
static struct {
    void *owner;
    unsigned epoch,generation,sample,slot,dpad,next_motion;
} art_cancel;

static unsigned native_art_select(void *a)
{
    return ((unsigned (*)(void *))(main_base+0x2a8b34))(a);
}

static unsigned world_art_move_cancel(const WObservation *o)
{
    float frame=o->base.animation_frame;
#if ML_ASCENDANCE_PROTOTYPE
    if(musou.phase)return musou.phase==MUSOU_SEAT&&frame>=58.f;
    if(ascendance.double_cut)return world.slot==DOUBLE_DRAW_RETURN&&frame>=22.f;
    if(ascendance.radiance==RADIANCE_FLASH)return frame>=RADIANCE_FLASH_COUNTER_END;
    if(ascendance.radiance==RADIANCE_CHASE)return 0;
    if(ascendance.radiance==RADIANCE_FOLLOW){
        if(world.slot==PURSUIT_FIRST_SLOT)return !radiance_pursuit.rewarded&&frame>=PURSUIT_FIRST_BRANCH;
        return world.slot==PURSUIT_SECOND_SLOT&&frame>=PURSUIT_SECOND_HIT_END+3.f;
    }
    if(world.move==W_SMALL&&dividing_slot(world.slot))
        return frame>=(world.slot==DIVIDING_STEP_SLOT?70.f:42.f);
#endif
    if(world.source==904)return frame>=W_RELEASE_RECOVERY_CANCEL;
    if(world.move==W_FOCUS&&world.wilds_focus_break&&(world.source==905||world.source==913))return 0;
    const WildsClip *clip=wilds_clip(world.source);
    if(clip)return clip->cancel&&frame>=clip->cancel;
    if(world.move==W_SHEATHE)return w_iai_ready(&world,o);
    if(world.move>=W_RISE&&world.move<=W_DOWN)return 0;
    const WEvent *event=w_event(world.source,frame);
    return event&&((event->input[WI_ROLL.word]>>WI_ROLL.shift)&3)==3;
}

static void world_art_capture(void *a,const MLObservation *o)
{
    unsigned grounded_sheathed=pad_valid&&u16(a,STATE)==16&&
        !(word(a,AIR)&0x400)&&(int16_t)u16(a,HP)>0;
    unsigned next=art_cancel.owner==a&&art_cancel.epoch==o->epoch&&art_cancel.next_motion&&
        world.move&&art_cancel.next_motion==0x500+world.slot&&grounded_sheathed;
    if(next&&!world_bound)return;
    if(!world.move||!world_bound||world_bound_actor!=(u32)(uptr)a||
       (!o->eligible&&!grounded_sheathed)||o->motion!=0x500+world.slot||o->native_action!=world_bound_action){
        art_cancel.owner=0;
        return;
    }
    if(next){
        art_cancel.generation=o->bind_generation;
        art_cancel.sample=o->sample_sequence;
        art_cancel.next_motion=0;
    }
    if(o->simulation_delta<=0)return;

    unsigned raw=ml_pad_raw_bits();
    if(art_cancel.owner==a&&art_cancel.epoch==o->epoch&&art_cancel.generation==o->bind_generation&&
       art_cancel.dpad&&(raw&art_cancel.dpad)==art_cancel.dpad)
        art_cancel.sample=o->sample_sequence;

    unsigned (*pressed)(void *,unsigned)=
        (unsigned (*)(void *,unsigned))(*(uptr **)a)[0x2e8/4];
    if(!pressed(a,64)&&!pressed(a,65)&&!pressed(a,66))return;
    unsigned old_slot=word(a,ART_SELECTED);
    art_cancel.owner=0;
    if(native_art_select(a)){
        const void *options=(const void *)(uptr)word((const void *)main_base,0x1897f78);
        /* Type 0 uses the D-pad unless its roles are swapped with the stick. */
        unsigned dpad=options&&byte(options,0x446c)==0&&!byte(options,0x446f)?raw&0xf0u:0;
        art_cancel=(__typeof__(art_cancel)){a,o->epoch,o->bind_generation,o->sample_sequence,
            word(a,ART_SELECTED),dpad};
    }
    put(a,ART_SELECTED,old_slot);
}

static unsigned world_art_commit(void *a,const WObservation *o,WDecision d)
{
    /* A charging stance can wait for a separate release input. */
    unsigned waiting=world.source==908||world.source==909||world.source==915;
    unsigned expired=waiting&&o->base.sample_sequence-art_cancel.sample>ART_CANCEL_SAMPLES;
    if(art_cancel.owner!=a||art_cancel.epoch!=o->base.epoch||art_cancel.next_motion||
       art_cancel.generation!=o->base.bind_generation||
       expired||!o->alive||!o->grounded)return 0;
    if(d.kind==W_MOTION){
        art_cancel.next_motion=0x500+d.slot;
        return 0;
    }
    unsigned early=d.kind==W_KEEP&&world_art_move_cancel(o);
    if(d.kind!=W_IDLE&&!early)return 0;

    unsigned slot=art_cancel.slot,old_slot=word(a,ART_SELECTED);
    art_cancel.owner=0;
    if(slot>=ART_SLOT_COUNT)return 0;
    put(a,ART_SELECTED,slot);
    uint16_t group=0,action=0,flags=0;
    if(!native_art_select(a)||word(a,ART_SELECTED)!=slot||
       !((unsigned (*)(void *,uint16_t *,uint16_t *,uint16_t *))(main_base+0x2acb60))(a,&group,&action,&flags)||
       !group){
        put(a,ART_SELECTED,old_slot);
        return 0;
    }

    /* Let the native action request debit the gauge and start effects. */
    ((void (*)(void *))(main_base+0x2acbec))(a);
    unsigned moving_one=early&&world.move==W_SLIDE&&
        !(world.source>=W_SPIRIT_TWO_FORWARD&&world.source<=W_SPIRIT_TWO_BACK);
    if(early)d.rank_delta+=w_leave(&world).rank_delta;
#if ML_ASCENDANCE_PROTOTYPE
    WDecision exit={.kind=W_IDLE};
    ascendance_transition(a,&exit);
    ascendance.dividing_input=DIV_INPUT_NONE;
#endif
    d.kind=W_KEEP;
    world_commit(a,d);
    world_physics_end(a);
    world_cleanup(a);
    world.controls=(WControls){0};
    world.wilds_charge_link=world.wilds_three_exit_sample=world.wilds_three_exit_r=0;
    if(moving_one)world_art_slide_seed(a);
    else if(!w_moving_spirit_recovery(&world,o->base.sample_sequence))art_slide_from.owner=0;
    world.wilds_slide_exit_sample=world.wilds_slide_exit_r=0;
    art_entry_blend=(__typeof__(art_entry_blend)){a,byte(a,AREA),sample_sequence,group,action};
    ++submitting;
    virtual_request(a,group,action,flags|ART_REQUEST);
    --submitting;
    log_event("hunter_art_from_move",(u32)(uptr)a,slot,(group<<16)|action,(u32)o->base.animation_frame);
    return 1;
}
#endif
static unsigned world_update(void *a,const MLObservation *b)
{
#if ML_WILDS_MOVES
    unsigned art_return=0, art_joined=0;
    MLObservation art_wait_input;
    /* Fresh physical chords only. State, resource, red-rank and gauge gates
     * are validated again by w_wilds_enter when the request is committed. */
    unsigned raw=
#if ML_ASCENDANCE_PROTOTYPE
        ascendance_pad_bits(a);
#else
        ml_pad_raw_bits();
#endif
#if ML_ASCENDANCE_PROTOTYPE
    unsigned radiance_keys=0;
    MLObservation reserved_input;
    if(world_radiance_input_reserved(a,raw,b->held)){
        radiance_keys=b->held;
        reserved_input=*b;
        reserved_input.held=0;
        b=&reserved_input;
        wilds_roll_x=wilds_roll_input=0;
        if(world_wilds_pending==W_CHIJIN)world_wilds_pending=0;
    }
#endif
    unsigned edge=raw&~wilds_input_previous;
    if(wilds_input_actor!=(u32)(uptr)a||wilds_input_epoch!=byte(a,AREA)){
        wilds_input_actor=(u32)(uptr)a;wilds_input_epoch=byte(a,AREA);edge=world_wilds_pending=0;
        wilds_roll_x=wilds_roll_input=0;wilds_roll_tail=0;wilds_focus_active=0;
        focus_lead_time=focus_strike_time=0;focus_native.active=0;
    }
     wilds_input_previous=raw;
    {
     unsigned state=u16(a,STATE),action=u16(a,ACTION),mv=0;
     unsigned native_art=world_native_art(a);
     art_return=world_art_return(a,b,native_art);
     if(!native_art&&!art_return&&world_art_input_owner(a)&&art_return_input.keys==ML_R){
        art_wait_input=*b;
        art_wait_input.held=0;
        b=&art_wait_input;
     }
     unsigned ready=!native_art&&!world.move&&(byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&
        !w_native_spirit_one(state,action)&&
        (int16_t)u16(a,HP)>0&&world_ready(a)&&b->simulation_delta>0&&
        (((state==1||state==2)&&action<=5)||(state==16&&action<154&&real(a,FRAME)>=24));
     unsigned rolling=w_native_roll(state,action);
     if(!crimson_available(a)||(b->held&~(ML_X|ML_B))||world.move){
        wilds_roll_tail=0;wilds_roll_x=0;
     }else if(rolling){
        wilds_roll_tail=.24f;
     }else if(wilds_roll_tail>0&&b->simulation_delta>0&&b->simulation_delta<30){
        wilds_roll_tail-=b->simulation_delta/60;
        if(wilds_roll_tail<=0)wilds_roll_x=0;
     }
     if(wilds_roll_tail>0&&(b->held&ML_X)&&(edge&PAD_X))world_roll_crimson_input(b);
     if(ready&&!art_return&&crimson_available(a)){
        if(wilds_roll_x&&wilds_roll_tail>0){mv=W_CHIJIN;wilds_roll_input=1;}
        else if(!valor_active(a)&&b->held==ML_X&&(edge&PAD_X))mv=W_CHIJIN;
        if(mv){world_wilds_pending=mv;wilds_roll_x=0;log_event("wilds_chord",(u32)(uptr)a,mv,raw,0);}
     }

#if ML_DEBUG_FORCE
     if((raw&0x400u)&&(edge&0x4000u)){
        unsigned before=((unsigned (*)(void *))(main_base+0x119ed80))(a);
        ((void (*)(void *,int))(main_base+0x119ed98))(a,1);
        log_event("debug_rank",(u32)(uptr)a,before,((unsigned (*)(void *))(main_base+0x119ed80))(a),raw);
     }
#endif
    }
#endif
#if ML_DEBUG_FORCE
    /* 调试台：按住 ZL + Y 依次强制起他的招（见切/突刺/纳刀/小居合/大居合/兜割下劈）。 */
    {static unsigned chord_last,idx;
     u32 raw=ml_pad_raw_bits();unsigned on=((raw&0x400u)&&(raw&0x8000u))?1u:0u;
     if(on&&!chord_last){static const unsigned seq[]={W_FORESIGHT,W_THRUST,W_SHEATHE,W_SMALL,W_BIG,W_DOWN,
            W_CHIJIN,W_CHARGE,W_SLIDE,W_RELEASE,W_FOCUS};
        world_debug_pending=seq[idx%11u];++idx;
        log_event("debug_force",(u32)(uptr)a,world_debug_pending,idx,raw);}
     chord_last=on;}
#endif
#if ML_WILDS_MOVES
    world_art_capture(a,b);
#endif
    world_loop_heading(a);
#if ML_DEBUG_FORCE
    world_pose_probe(a);
#endif
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    world_audio_tick(a);
#endif
    world_fx_tick(a);
    WObservation o={.base=*b,.state=u16(a,STATE),.previous_weapon_action=u16(a,0x250c),.drawn=byte(a,DRAWN)&1,
        .alive=(int16_t)u16(a,HP)>0,.grounded=!(word(a,AIR)&0x400),
        .rank=((unsigned (*)(void *))(main_base+0x119ed80))(a),.resources_ready=world_ready(a)};
#if ML_WILDS_MOVES
    static unsigned rank_actor,rank_previous;
    if(rank_actor!=(u32)(uptr)a||o.rank!=3||!o.alive){
        red_rounds=red_iai_used=0;red_aura_hold=0;
    }
    if(rank_actor==(u32)(uptr)a&&rank_previous<3&&o.rank==3&&o.alive){
        set_real(a,0x332c,RED_AURA_MAX);red_aura_hold=RED_AURA_HOLD;
        ((void (*)(void *,int,int))(main_base+0x2a309c))(a,100,-1);
        put(a,0x2780,100);o.base.gauge=100;
        log_event("wilds_red_fill",(u32)(uptr)a,rank_previous,o.rank,100);
    }
    rank_actor=(u32)(uptr)a;rank_previous=o.rank;
    unsigned focus_active=(raw&PAD_FOCUS)&&o.alive&&o.resources_ready;
    if(b->simulation_delta>0&&b->simulation_delta<30){
        float dt=b->simulation_delta/60;
        if(focus_lead_time>0)focus_lead_time-=dt;
        if(focus_strike_time>0)focus_strike_time-=dt;
    }
    /* A chord pressed during the focus combo belongs to that combo. Do not carry it into idle. */
    if(world.move==W_FOCUS||!focus_active||!o.drawn||!o.grounded){
        focus_lead_time=focus_strike_time=0;
    }else if(b->held==(ML_R|ML_Y)){
        if(edge&(PAD_R|PAD_Y|PAD_FOCUS)){
            focus_lead_time=0;focus_strike_time=.32f;focus_native.active=0;
        }
    }else if(b->held&~(ML_R|ML_Y)){
        focus_lead_time=focus_strike_time=0;focus_native.active=0;
    }else if((b->held==ML_R&&(edge&PAD_R))||(b->held==ML_Y&&(edge&PAD_Y))){
        focus_lead_time=.10f;
    }
    o.valor_style=hunter_style(a)==5;
    o.valor=valor_active(a);
    if(valor_foresight_round.actor==a&&
       (valor_foresight_round.epoch!=byte(a,AREA)||o.state!=16||b->native_action!=30))
        valor_foresight_round.actor=0;
    /* Blue is native tier 4. The imported moves use the red (tier 3)
     * hit tables and landing routes; GU owns the actual aura value. */
    unsigned blue=o.valor&&o.rank==4;
    if(blue)o.rank=3;
    if(o.valor&&!world.move&&(edge&0xf200u))
        o.valor_move=w_valor_chord(o.state,b->native_action,b->motion,b->animation_frame,b->held,raw&PAD_FOCUS);
    if(o.valor&&!world.move&&o.alive&&o.drawn&&o.grounded&&o.resources_ready&&valor_guard_pose(a)){
        valor_guard_input(a,b->held,edge);
        if(valor_guard.chord)
            o.valor_move=w_valor_chord(o.state,b->native_action,b->motion,b->animation_frame,valor_guard.chord,0);
    }else{
        valor_guard.actor=0;valor_guard.early_r=valor_guard.spirit=valor_guard.chord=0;
    }
    unsigned link_pose=o.valor&&!world.move&&o.alive&&o.drawn&&o.grounded&&o.resources_ready?
        valor_link_pose(a):0;
    unsigned draw_cancel=!world.move&&o.alive&&o.grounded&&o.drawn&&o.resources_ready?valor_draw_cut(a):0;
    o.valor_draw_cancel=draw_cancel;
    if(draw_cancel)link_pose=5;
    unsigned valor_gp_start=0;
    if(link_pose){
        valor_link_collect(a,b,edge,link_pose);
        if(valor_link.sample!=b->sample_sequence){
            valor_link.sample=b->sample_sequence;
            if(b->simulation_delta>0&&b->simulation_delta<30)valor_link.elapsed+=b->simulation_delta;
        }
        if(blue&&(link_pose==1||link_pose==3||link_pose==4)&&(edge&0xf200u)){
            if(link_pose!=4&&b->held==ML_X&&(edge&PAD_X))o.valor_move=W_CHIJIN;
            if(b->held==(ML_R|ML_A)&&(link_pose==1||(link_pose==4&&b->animation_frame<=8)))
                o.valor_move=W_FORESIGHT;
        }
        if(link_pose==5){
            if(b->animation_frame>=draw_cancel)o.valor_move=draw_cut_move(valor_link.keys);
        }else if(valor_link.keys)
            o.valor_move=w_valor_chord(o.state,b->native_action,b->motion,b->animation_frame,valor_link.keys,0);
        if(o.valor_move||(b->held&~ML_R))valor_link.moving=0;
        unsigned slide_ready=blue?
            (link_pose==4&&u16(a,MOTION)==0x5c0&&!(word(a,9488)&16u)&&real(a,FRAME)>0):
            valor_link.elapsed>=6;
        if(valor_link.actor==a&&valor_link.epoch==byte(a,AREA)&&valor_link.moving&&slide_ready){
            o.valor_move=W_SLIDE;valor_link.moving=0;
            o.base.heading=valor_link.heading;o.base.input_heading=valor_link.input_heading;o.base.magnitude=100;
        }
        /* GU can leave a drawn hunter walking when stick and R arrive
         * together. Start native GP I first, with the same directional link. */
        valor_gp_start=blue&&link_pose==1&&valor_link.moving&&valor_link.elapsed>=6;
    }else valor_link.actor=0;
    o.focus_strike=focus_strike_time>0;
    if(o.valor_move==W_FOCUS&&!o.focus_strike)o.valor_move=0;
    o.has_focus_heading=focus_active&&world_camera_heading(a,&o.focus_camera_heading);
    unsigned aim;
    unsigned focus_heading=world_focus_heading(a,&aim);
    ml_wilds_focus(focus_heading&&o.drawn&&b->simulation_delta>0);
    if(focus_active!=wilds_focus_active){
        wilds_focus_active=focus_active;
        log_event("wilds_focus_mode",(u32)(uptr)a,focus_active,raw,focus_heading?aim:0);
    }
    if(edge&PAD_LOCK)log_event("wilds_focus_lock_key",(u32)(uptr)a,raw,focus_active,0);
    if(focus_heading&&!world.move&&o.drawn&&o.grounded&&
       (o.state==1||o.state==2)&&b->native_action<=5){
        if(b->magnitude<50){put(a,HEADING,aim);put(a,TARGET,aim);}
        o.base.heading=aim;
    }
    /* The red-blade gate reads the weapon-handling aura word elsewhere in this
     * file; the virtual accessor can report a lower level while sprinting, so
     * promote the observation when the aura word says the blade is red. */
    if(crimson_available(a)&&o.rank<3)o.rank=3;
    world_crimson_sprint_tick(a,&o);
    o.rising_sprint=world_rising_sprint(a);
    if(edge&PAD_X)
        log_event("wilds_x_state",(u32)(uptr)a,((o.state<<16)|b->native_action)&0xffffffffu,
            (o.drawn?1u:0)|(o.grounded?2u:0)|(world.move?4u:0)|((b->held&0xffffu)<<8),
            (o.rank&255u)|((o.rising_sprint?1u:0)<<8)|((word(a,0x3330)&0xffu)<<16));
    if(!world.move&&b->simulation_delta>0&&w_red_draw(&o)&&
       (((edge&PAD_X)&&world_red_draw_input(&o))||
        (wilds_roll_tail>0&&wilds_roll_x))){
        if(wilds_roll_x){wilds_roll_input=1;wilds_roll_x=0;}
        world_wilds_pending=W_CHIJIN;
        if(edge&PAD_X)log_event("wilds_sprint_draw_pending",(u32)(uptr)a,b->held,o.rank,o.state);
    }
#endif
    o.contact=world_contact(a);
#if ML_WILDS_MOVES
    /* The second strike row belongs to the returning blade. */
    if(world.valor_foresight&&world.source==918&&world_hit!=163)o.contact=0;
#endif
    o.counter=world_counter_seen;
    if(world_air_owner(a))o.grounded=real(a,0x44)<=real(a,0x10f0)+1&&real(a,0x1014)<=0;
    /* Guild's aerial spirit action uses AIR=0x2008, not the ground handler's 0x400 bit. */
    if(o.state==16&&((b->native_action>=72&&b->native_action<=75&&b->motion>=0x599&&b->motion<=0x59c)||b->motion==0x598))
        o.grounded=real(a,0x44)<=real(a,0x10f0)+1&&real(a,0x1014)<=0;
    if(o.contact&&!world_contact_seen){world_contact_seen=1;
        log_event("world_contact",(u32)(uptr)a,world.source,word(a,0x30d0),word(a,0x30cc));}
    if(world.actor!=(u32)(uptr)a||world.epoch!=o.base.epoch){
        world_normal_gauge_tenths=0;
        world_physics_end(a);world_cleanup(a);world_regen=world_helm_regen=world_regen_fraction=world_armor=0;
        world_follow_clear();
    }
    if(world_last_sample!=sample_sequence){
        world_last_sample=sample_sequence;float dt=o.base.simulation_delta/60.0f;
        if(dt>0&&dt<.5f){
            world_detector_tick(a,dt);
            if(o.alive&&o.resources_ready)world_follow_tick(a,dt);
            if(world_armor>0)world_armor-=dt;
            if(world_regen>0||world_helm_regen>0){
                /* World capture: consecutive gauge pulses are 61 simulation frames apart. */
                world_regen_hud_frame+=o.base.simulation_delta;
                if(world_regen_hud_frame>=61.0f)world_regen_hud_frame-=61.0f;
                float used=0;
                if(world_regen>0){float t=world_regen<dt?world_regen:dt;world_regen-=t;used+=t;}
                if(world_helm_regen>0){float t=world_helm_regen<dt?world_helm_regen:dt;world_helm_regen-=t;used+=t;}
                /* Source2123a72..2123b50 adds dt / rate15 to a0..1 gauge.
                 * Helm and Iai have independent timers and add together. */
                world_regen_fraction+=used*(100.0f/15.0f);
                unsigned points=(unsigned)world_regen_fraction;
                if(points){world_regen_fraction-=(float)points;
                    unsigned gauge=word(a,0x2780),next=gauge+points;
                    put(a,0x2780,next>100?100:next);
                    if(next>=100)world_regen_fraction=0;
                    if(gauge<100&&next>=100){
                        typedef void (__attribute__((pcs("aapcs-vfp"))) *FullGauge)(void *,float);
                        ((FullGauge)(main_base+0x119ee7c))(a,1800.0f);
                    }}
            }else world_regen_hud_frame=0;
        }
        if(!o.alive||!o.resources_ready){world_normal_gauge_tenths=0;world_regen=world_helm_regen=world_armor=0;world_follow_clear();}
    }
    if(world.move&&u16(a,ACTION)==w_action(world.move)&&!world_bound){
        world_bind_motion(a);
        o.base=observe(a,1);
        log_event("world_bound",(u32)(uptr)a,world.move,u16(a,MOTION),word(a,0x1c));
        if(world.move==W_GROUND)log_event("world_ground_phase",(u32)(uptr)a,world.source,
            world.ground_node,(u32)(real(a,FRAME)*1000));
    }
    unsigned heading;
#if ML_WILDS_MOVES
    unsigned easing=entry_turn.actor==(u32)(uptr)a&&entry_turn.epoch==byte(a,AREA)&&
        world.move&&world.source==entry_turn.source&&world_bound;
    if(easing){
        if(real(a,FRAME)>=entry_turn_delay)world.heading=world_turn_step(a,&entry_turn,0);
        put(a,HEADING,world.heading);put(a,TARGET,world.heading);o.base.heading=world.heading;
    }
    if(!easing&&focus_heading&&w_iai_ready(&world,&o)
#if ML_ASCENDANCE_PROTOTYPE
       &&ascendance.radiance!=RADIANCE_HOLD
#endif
       ){
        aim=world_focus_body_step(a,aim);
        put(a,HEADING,aim);put(a,TARGET,aim);o.base.heading=world.heading=aim;
    }
#endif
#if ML_WILDS_MOVES
    if(world_focus_thrust_heading(a,&heading)||(!easing&&world_focus_air_heading(a,&heading))){
        put(a,HEADING,heading);put(a,TARGET,heading);o.base.heading=world.heading=heading;
    }else if(!easing)
#endif
#if ML_ASCENDANCE_PROTOTYPE
    if(world_bound&&(world.slot==DIVIDING_STEP_SLOT||(ascendance.double_cut&&ascendance.double_step&&double_draw_slot(world.slot)))){
        put(a,HEADING,world.heading);put(a,TARGET,world.heading);o.base.heading=world.heading;
    }else
#endif
    if(w_rotation(&world,&o,&heading)){
        put(a,HEADING,heading);put(a,TARGET,heading);o.base.heading=heading;
    }
#if ML_WILDS_MOVES
    /* Movement intent cannot rotate a landing/release or a committed round
     * away from the contact that started the chain. */
    if(world_bound&&(world.move==W_LAND||world.move==W_RELEASE||world.source==917)){
        put(a,HEADING,world.heading);put(a,TARGET,world.heading);o.base.heading=world.heading;
    }
#endif
    if(world.move==W_BIG&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        /* World's animated root turns the actor, not just the skeleton's
         * extraction space. Keep the entry aim separate from that half turn.
         * Native GU pose and floor/collision evaluation remain authoritative. */
        unsigned facing=(world.heading+w_root_heading(world.source,real(a,FRAME)))&65535u;
#if ML_ASCENDANCE_PROTOTYPE
        if(ascendance_independent_flash())facing=world.heading;
#endif
        put(a,HEADING,facing);put(a,TARGET,facing);o.base.heading=facing;
    }
#if ML_WILDS_MOVES
    if(world_bound&&(byte(a,DRAWN)&1)&&u16(a,MOTION)==0x500+world.slot&&
       ((world.source==917&&!world.red_foresight_chain&&real(a,FRAME)>=W_ROUND_SHEATHE_FRAME)||
        (world.source==904&&real(a,FRAME)>=W_RELEASE_SHEATHE_FRAME))){
        ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,0);
        o.drawn=byte(a,DRAWN)&1;
        log_event("wilds_sheathed",(u32)(uptr)a,world.source,(u32)real(a,FRAME),byte(a,DRAWN));
    }
#endif
#if ML_WILDS_MOVES
    /* Let GU resolve a staggered X+A before committing a drawn blue X.
     * Roll and draw requests already have their own native input windows. */
    if(o.valor&&!world.move&&o.alive&&o.grounded&&o.resources_ready&&o.drawn&&
       !wilds_roll_input&&!w_native_roll(o.state,b->native_action)&&
       (o.valor_move==W_CHIJIN||world_wilds_pending==W_CHIJIN)){
        o.valor_move=0;world_wilds_pending=0;
        if(!valor_x.actor)
            valor_x=(__typeof__(valor_x)){.actor=a,.epoch=byte(a,AREA),.state=o.state,
                .action=b->native_action,.sample=b->sample_sequence,.heading=b->heading,
                .input=b->input_heading,.magnitude=b->magnitude};
    }
    if(valor_x.actor){
        if(valor_x.actor!=a||valor_x.epoch!=byte(a,AREA)||o.state!=valor_x.state||
           b->native_action!=valor_x.action||!o.valor||world.move||!o.alive||!o.grounded||
           !o.drawn||!o.resources_ready||(b->held&~ML_X))valor_x.actor=0;
        else if(valor_x.sample!=b->sample_sequence){
            valor_x.sample=b->sample_sequence;valor_x.elapsed+=b->simulation_delta;
            if(valor_x.elapsed>=5){
                o.valor_move=W_CHIJIN;o.base.heading=valor_x.heading;
                o.base.input_heading=valor_x.input;o.base.magnitude=valor_x.magnitude;
                valor_x.actor=0;
            }
        }
    }
#endif
#if ML_WILDS_MOVES
    if(world_potion_draw(&o)){
        valor_x.actor=0;wilds_roll_input=0;world_wilds_pending=W_CHIJIN;
        log_event("wilds_potion_draw",(u32)(uptr)a,o.state,o.base.native_action,o.base.magnitude);
    }
#endif
    WDecision d={0};
#if ML_ASCENDANCE_PROTOTYPE
    if(radiance_keys)o.base.held=radiance_keys;
    ascendance_tick(a,&o,raw,edge);
    o.ascendance_active=(world.source==918||world.source==W_SPIRIT_TWO_FORWARD)&&
        ascendance.remaining>0&&ascendance_owner(a);
    if(o.ascendance_active&&world.source==W_SPIRIT_TWO_FORWARD&&b->magnitude*5>128)
        o.dividing_stick_heading=b->magnitude>=50?b->input_heading:stick_world_heading(a);
    unsigned radiance_start=world_radiance_start(a,&o,raw,edge);
    d=ascendance_radiance_command(a,&o,raw,edge,radiance_start);
    if(radiance_keys)o.base.held=0;
#endif
#if ML_WILDS_MOVES
    /* A buffered art follow-up owns the first free update, before ordinary
     * combo processing can replay the same held buttons without blending. */
    if(art_return&&d.kind==W_KEEP){
        WObservation next=o;
        next.base.heading=art_return_input.heading;
        next.base.input_heading=art_return_input.input_heading;
        next.base.magnitude=art_return_input.magnitude;
        if(art_return==ML_R){
            unsigned moving_two=art_slide_from.owner==a&&art_slide_from.epoch==byte(a,AREA)&&
                art_slide_from.seen&&sample_sequence-art_slide_from.sample<=ART_SLIDE_SAMPLES;
            unsigned charge=o.base.held==ML_R&&art_return_input.r_ticks>=W_SPIRIT_FULL_HOLD_SECONDS*60.f;
            /* Sakura and the evasion arts may finish with the blade stowed. */
            unsigned draw=!next.drawn&&(charge||next.base.magnitude>=50);
            if(draw)next.drawn=1;
            d=w_art_spirit(&world,&next,moving_two,charge);
            if(draw){
                if(d.kind==W_ENTER)((void (*)(void *,unsigned))(main_base+0x2a2560))(a,1);
                else d=(WDecision){0};
            }
            if(d.kind==W_NATIVE){
                art_exit.owner=0;
                log_event("hunter_art_to_spirit",(u32)(uptr)a,d.native_action,art_return_input.r_length,0);
            }
        }else{
            unsigned draw=!next.drawn&&world_art_draw_handoff(art_return);
            if(draw)next.drawn=1;
            d=w_art_follow(&world,&next,art_return);
            if(draw&&d.kind==W_ENTER)
                ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,1);
        }
        if(d.kind!=W_KEEP)art_slide_from.owner=0;
        if(d.kind==W_ENTER){
            art_joined=1;
            art_exit.owner=0;
            d.motion_contract=1;d.blend=8;d.speed=1;
            if(d.move==W_SHEATHE)
                art_iai=(__typeof__(art_iai)){a,b->epoch,art_return_input.iai_key,b->held};
        }
    }
    if(d.kind==W_KEEP)d=world_art_iai(a,&o);
#endif
#if ML_ASCENDANCE_PROTOTYPE
    unsigned dividing_bound=world.move==W_SMALL&&dividing_slot(world.slot)&&world_bound&&
        o.state==16&&b->native_action==w_action(W_SMALL)&&b->motion==0x500+world.slot;
    if(d.kind==W_KEEP&&dividing_bound&&o.grounded&&o.drawn&&o.alive&&o.resources_ready){
        unsigned step=world.slot==DIVIDING_STEP_SLOT;
        unsigned eligible=!step||b->animation_frame>=35;
        if(b->held==(ML_R|ML_B)&&(edge&(PAD_R|0x4000))){
            ascendance.dividing_input=eligible?DIV_INPUT_SHEATHE:DIV_INPUT_NONE;
        }else if(b->held==ML_B&&(edge&0x4000)){
            ascendance.dividing_input=eligible?DIV_INPUT_ROLL:DIV_INPUT_NONE;
            ascendance.dividing_input_heading=b->magnitude>=50?b->input_heading:b->heading;
        }else if(b->held==(ML_R|ML_A)&&(edge&(PAD_R|0x2000))){
            ascendance.dividing_input=eligible?DIV_INPUT_FORESIGHT:DIV_INPUT_NONE;
        }else if(b->held==(ML_R|ML_X)&&(edge&(PAD_R|PAD_X))){
            ascendance.dividing_input=eligible?DIV_INPUT_THRUST:DIV_INPUT_NONE;
        }else if((edge&PAD_R)&&b->held==ML_R){
            ascendance.dividing_input=eligible?DIV_INPUT_SPIRIT_HELD:DIV_INPUT_NONE;
            ascendance.dividing_hold_ticks=0;
            ascendance.dividing_sample=b->sample_sequence;
        }else if(b->held==(ML_X|ML_A)&&(edge&(PAD_X|0x2000))){
            ascendance.dividing_input=eligible?DIV_INPUT_FADE:DIV_INPUT_NONE;
            ascendance.dividing_fade_action=w_wilds_fade_action(&o);
        }else if(b->held==ML_X&&(edge&PAD_X)){
            ascendance.dividing_input=eligible?DIV_INPUT_PRIMARY:DIV_INPUT_NONE;
            ascendance.dividing_input_heading=b->input_heading;
            ascendance.dividing_magnitude=b->magnitude;
        }else if(b->held==ML_A&&(edge&0x2000)){
            ascendance.dividing_input=eligible?DIV_INPUT_SECONDARY:DIV_INPUT_NONE;
        }else if(b->held!=(ML_R|ML_A)&&b->held!=(ML_R|ML_X)&&b->held!=(ML_X|ML_A)&&b->held!=ML_X&&b->held!=ML_A&&(b->held&~(ML_R|ML_B))){
            ascendance.dividing_input=DIV_INPUT_NONE;
        }
        if(ascendance.dividing_input==DIV_INPUT_SPIRIT_HELD){
            if(!(b->held&ML_R)){
                ascendance.dividing_input=DIV_INPUT_ROUND;
            }else if(ascendance.dividing_sample!=b->sample_sequence){
                ascendance.dividing_sample=b->sample_sequence;
                if(b->simulation_delta>0&&b->simulation_delta<=15)
                    ascendance.dividing_hold_ticks+=b->simulation_delta;
                if(ascendance.dividing_hold_ticks>=W_SPIRIT_FULL_HOLD_SECONDS*60.f)
                    ascendance.dividing_input=DIV_INPUT_CHARGE;
            }
        }
        if(b->animation_frame>=(step?168:140)||b->motion_finished){
            d=w_leave(&world);d.kind=W_IDLE;
        }else if(b->animation_frame>=(step?70:42)){
            if(ascendance.dividing_input==DIV_INPUT_PRIMARY){
                ascendance.dividing_input=DIV_INPUT_NONE;
                if(o.rank==3){
                    WObservation next=o;
                    next.base.input_heading=ascendance.dividing_input_heading;
                    next.base.magnitude=ascendance.dividing_magnitude;
                    d=w_wilds_enter(&world,&next,W_CHIJIN);
                }else{
                    d=w_leave(&world);d.kind=W_NATIVE;d.native_action=0;d.heading=b->heading;
                }
                if(d.kind!=W_KEEP)log_event("dividing_primary",(u32)b->animation_frame,o.rank,d.slot,step);
            }else if(ascendance.dividing_input==DIV_INPUT_THRUST||ascendance.dividing_input==DIV_INPUT_FORESIGHT){
                unsigned foresight=ascendance.dividing_input==DIV_INPUT_FORESIGHT;
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_dividing_spirit_attack(&world,&o,ML_R|(foresight?ML_A:ML_X));
                if(d.kind==W_ENTER)log_event(foresight?"dividing_foresight":"dividing_thrust",(u32)b->animation_frame,o.rank,d.slot,step);
            }else if(ascendance.dividing_input==DIV_INPUT_FADE){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_leave(&world);d.kind=W_NATIVE;d.native_action=ascendance.dividing_fade_action;d.heading=b->heading;
                log_event("dividing_fade",(u32)b->animation_frame,o.rank,d.native_action,step);
            }else if(ascendance.dividing_input==DIV_INPUT_SECONDARY){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_leave(&world);d.kind=W_NATIVE;d.native_action=25;d.heading=b->heading;
                log_event("dividing_secondary",(u32)b->animation_frame,o.rank,d.native_action,step);
            }else if(ascendance.dividing_input==DIV_INPUT_ROLL){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_leave(&world);d.kind=W_ROLL;d.heading=ascendance.dividing_input_heading;
                log_event("dividing_roll",(u32)b->animation_frame,d.heading,step,0);
            }else if(ascendance.dividing_input==DIV_INPUT_SHEATHE){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_dividing_sheathe(&world,&o);
                if(d.kind==W_ENTER)log_event("dividing_sheathe",(u32)b->animation_frame,d.slot,step,0);
            }else if(ascendance.dividing_input==DIV_INPUT_CHARGE){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_wilds_round_charge(&world,&o);
                if(d.kind==W_ENTER)
                    log_event("dividing_charge",world.slot,(u32)b->animation_frame,(u32)ascendance.dividing_hold_ticks,d.slot);
            }else if(ascendance.dividing_input==DIV_INPUT_ROUND){
                ascendance.dividing_input=DIV_INPUT_NONE;
                d=w_wilds_round(&world,&o);
                if(d.kind==W_ENTER){
                    if(step){
                        /* Keep the planted step pose while the round starts at F0. */
                        d.motion_contract=1;d.blend=8;d.speed=1;
                    }
                    log_event("dividing_round",(u32)b->animation_frame,d.slot,step,0);
                }
            }
        }
    }else if(d.kind==W_KEEP&&!world.move&&ascendance_owner(a)&&ascendance.remaining>0&&
             o.grounded&&o.drawn&&o.resources_ready&&o.state==16&&b->native_action==26&&
             b->motion==0x500+108&&b->animation_frame<=2){
        const void *res=(const void *)(uptr)word(a,0xf18);
        const void *bank=res?(const void *)(uptr)word(res,0x64):0;
        unsigned buffered=ascendance.dividing_step_pending&&sample_sequence-ascendance.dividing_step_sample<=12;
        ascendance.dividing_step_pending=0;
        unsigned step=buffered||b->magnitude*5>128;
        unsigned slot=step?DIVIDING_STEP_SLOT:DIVIDING_STAND_SLOT;
        const void *clip=bank&&u16(bank,6)>slot?
            (const void *)(uptr)word(bank,8+4*slot):0;
        if(clip&&word(clip,8)==(step?DIVIDING_STEP_FRAMES:DIVIDING_STAND_FRAMES)){
            d=w_radiance_enter(&world,&o,W_SMALL);
            ascendance.dividing_input=DIV_INPUT_NONE;
            d.slot=world.slot=slot;
            if(step){
                ascendance.dividing_travel_frame=-1;
                unsigned heading=buffered?ascendance.dividing_step_heading:
                    b->magnitude>=50?b->input_heading:stick_world_heading(a);
                d.heading=world.heading=heading;
            }
            d.motion_contract=1;d.blend=10;d.start=0;d.speed=1;
            log_event(step?"ascendance_dividing_step":"ascendance_dividing_stand",(u32)(uptr)a,b->native_action,(u32)b->animation_frame,d.slot);
        }
        if(d.kind==W_KEEP)d=w_step(&world,&o);
    }else
#endif
    if(d.kind==W_KEEP)d=w_step(&world,&o);
#if ML_WILDS_MOVES
    if(valor_gp_start&&d.kind==W_KEEP&&!world_wilds_pending){
        d=(WDecision){.kind=W_NATIVE,.native_action=130,.heading=valor_link.heading,
            .valor_slide=1,.valor_slide_heading=valor_link.heading,.valor_slide_input=valor_link.input_heading};
    }
    if(d.kind==W_NATIVE&&d.native_action==130&&d.valor_slide){
        valor_link=(__typeof__(valor_link)){.actor=a,.epoch=byte(a,AREA),.owner=130,.moving=1,
            .heading=d.valor_slide_heading,.input_heading=d.valor_slide_input};
    }
#endif
#if ML_WILDS_MOVES
    if(d.kind==W_ENTER&&d.move==W_FOCUS){
        focus_lead_time=focus_strike_time=0;focus_native.active=0;
        log_event("wilds_focus_chord",(u32)(uptr)a,raw,0,0);
    }
    /* 组合键起的五招（走他自己的 enter/commit，动画与物理都是他那套） */
    if(world_wilds_pending){
        unsigned mv=world_wilds_pending;world_wilds_pending=0;
        WObservation request=o;
        if(mv==W_CHIJIN&&wilds_roll_input){
            request.base.input_heading=wilds_roll_heading;
            request.base.magnitude=wilds_roll_magnitude;
        }
        if(mv==W_SLIDE){
            request.base.held=wilds_slide_draw_input.held;
            request.base.input_heading=wilds_slide_draw_input.input_heading;
            request.base.magnitude=wilds_slide_draw_input.magnitude;
        }
        wilds_roll_input=0;
        if(d.kind!=W_ENTER){
            WDecision requested=w_wilds_enter(&world,&request,mv);
            if(requested.kind==W_ENTER&&mv==W_CHIJIN&&request.rising_sprint&&
               request.base.held==(ML_R|ML_X))
                log_event("wilds_rising_sprint_draw",(u32)(uptr)a,request.state,request.base.native_action,requested.slot);
            if(requested.kind!=W_KEEP)d=requested;
        }
    }
#endif
#if ML_DEBUG_FORCE
    /* 调试台：ZL + Y 依次强制起招（他的六招 + 我们的五招），用来验证"我们能不能从代码里驱动他的状态机"。 */
    if(world_debug_pending){unsigned mv=world_debug_pending;world_debug_pending=0;
        d=w_debug_enter(&world,&o,mv);
#if ML_WILDS_MOVES
        wilds_fired=0;wilds_hit_generation=~0u;
#endif
    }
#endif
#if ML_WORLD_INPUT_VM
    world_input_trace(a,&o);
#endif
#if ML_WILDS_MOVES
    if(d.kind==W_IDLE&&world.wilds_slide_exit_sample==b->sample_sequence&&world.wilds_slide_exit_sample)
        world_art_slide_seed(a);
    if(world_art_commit(a,&o,d))return 1;
    if(d.kind==W_IDLE&&world.wilds_three_exit_sample==b->sample_sequence)
        log_event("charged_three_recovery",(u32)(uptr)a,world.wilds_three_exit_r,
            b->held,(u32)(world.wilds_bridge_hold*1000));
#endif
    world_commit(a,d);w_input_finish(&world,&o,d);
#if ML_WILDS_MOVES
    if(art_joined&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move)){
        if(!world_bound)world_bind_motion(a);
        log_event("hunter_art_to_move",(u32)(uptr)a,art_return,d.slot,(u32)d.blend);
    }
#endif
#if ML_ASCENDANCE_PROTOTYPE
    if(d.kind==W_KEEP)ascendance_sheathe_pose_tick(a,&o);
#endif
#if ML_WILDS_MOVES
    if(wilds_clip(world.source))wilds_hit_tick(a);
    if(world.move==W_CHARGE&&(world.source==908||world.source==909||world.source==915)){
        unsigned percent=world.wilds_elapsed>=W_CHARGE_READY_SECONDS?100:
            (unsigned)(world.wilds_elapsed*(100.f/W_CHARGE_READY_SECONDS));
        ml_wilds_charge(percent+1);
    }else ml_wilds_charge(0);
    /* Horizontal root is authored in the LMT's native usage-4 track. */
#endif
#if ML_WILDS_MOVES && ML_WORLD_INPUT_VM
    if((link_pose==2||link_pose==3||link_pose==5)&&d.kind==W_KEEP&&valor_link.packet&&!valor_link.keys&&valor_link.elapsed>=6){
        unsigned action=valor_link.packet,flags=valor_link.flags;valor_link.packet=0;
        ++submitting;virtual_request(a,16,action,flags);--submitting;
        return 1;
    }
    if(d.kind==W_KEEP&&valor_guard.spirit&&valor_guard_same(a)&&valor_guard_pose(a)&&
       o.base.animation_frame>=valor_guard.deadline){
        unsigned action=valor_guard.spirit,flags=valor_guard.flags;
        valor_guard.spirit=0;
        ++submitting;virtual_request(a,16,action,flags);--submitting;
        return 1;
    }
    if(world_focus_replay(a,&o,d))return 1;
#endif
    if(world_r_gate.active){
        unsigned valid=world_r_gate.actor==(u32)(uptr)a&&world_r_gate.epoch==byte(a,AREA)&&
            o.alive&&o.drawn&&o.grounded&&o.resources_ready&&!world.move&&
            u16(a,MOTION)==world_r_gate.motion&&bind_generation==world_r_gate.generation;
#if ML_WORLD_INPUT_VM
        if(!valid||u16(a,STATE)!=world_r_gate.owner_state||u16(a,ACTION)!=world_r_gate.owner_action)world_r_gate.active=0;
        else if(world_r_gate.button==WORLD_R_WAIT_RELEASE){
            if(b->held&~ML_R)world_r_gate.active=0;
            else if(!(b->held&ML_R)){
                world_r_gate.active=0;
                if(o.rank!=3&&b->gauge<w_spirit_link_cost(world_r_gate.action))return 1;
                ++submitting;virtual_request(a,16,world_r_gate.action,world_r_gate.flags);--submitting;
                return 1;
            }
        }else if(!world.controls.buttons[world_r_gate.button].waiting&&!world.gu_chord_wait){
            unsigned action=world_r_gate.action,flags=world_r_gate.flags;world_r_gate.active=0;
            ++submitting;virtual_request(a,16,action,flags);--submitting;
            return 1;
        }
#else
        if(!valid||u16(a,STATE)!=16||(held&(ML_A|ML_X|ML_B|ML_Y))||world.queued){
            world_r_gate.active=0;
        }else{
            const WEvent *solo=w_event(w_source(u16(a,MOTION)),real(a,FRAME));
            unsigned stage=solo?(solo->input[WI_R.word]>>WI_R.shift)&3:0;
            if(!stage)world_r_gate.active=0;
            else if((world.keys.ready&ML_R)&&stage==3){
                unsigned action=world_r_gate.action,flags=world_r_gate.flags;world_r_gate.active=0;
                ++submitting;virtual_request(a,16,action,flags);--submitting;
                log_event("world_chord_solo",(u32)(uptr)a,action,flags,0);
                return 1;
            }
        }
 #endif
    }
    if(world.move&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        const WEvent *e=w_event(world.source,real(a,FRAME));
        if(e){
            /* Constant-speed cuts and boosted finishers preserve native hitstop. */
            if(world.source!=906&&world.source!=907
#if ML_ASCENDANCE_PROTOTYPE
               &&world_bound_attack_rate<=1.f
#endif
              ){
                float speed=world_motion_speed(a,e);
                set_real(a,0x50c,speed);set_real(a,0x14b0,speed);
            }
#if ML_WILDS_MOVES
            /* 我们的五招用自己那套判定（wilds_rows）；他这套按事件开行会 world_close_hit
             * 把刚开的行关掉（练气解放"掉刃色但没伤害"就是这个），所以这里跳过。 */
            if(world.move==W_THRUST||world.source==109)wilds_equipment_shape(a);
#if ML_ASCENDANCE_PROTOTYPE
            const MusouStroke *stroke=musou.phase==MUSOU_FLURRY?musou_clip(u16(a,MOTION)):0;
            if(world.move==W_SMALL&&dividing_slot(world.slot)){
                float frame=real(a,FRAME);
                float hit_start=world.slot==DIVIDING_STEP_SLOT?54:26;
                unsigned active=frame>=hit_start&&frame<hit_start+8;
                if(active)wilds_equipment_shape_for(a,W_BLADE_DIVIDING);
                unsigned opening=active&&world_hit!=DIVIDING_NORMAL_HIT;
                world_hit_tick(a,&(WEvent){.hit=active?DIVIDING_NORMAL_HIT:-1});
                if(active&&world_attack_owned)put(a,0x30a8,WILDS_SHAPE);
                if(opening&&world_attack_owned){
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
                    lingyue_attack_sound(a,world.slot==DIVIDING_STEP_SLOT?WILDS_SOUND_HELM_CUT:WILDS_SOUND_IAI_WHOOSH);
#endif
                    log_event("ascendance_dividing_edge",(u32)(uptr)a,(u32)frame,world_attack_id,0);
                }
            }else if(ascendance_independent_flash())ascendance_flash_blade(a);
            else if(musou.phase==MUSOU_ENTRY||musou.phase==MUSOU_SEAT)world_hit_tick(a,&(WEvent){.hit=-1});
            else if(stroke)ascendance_musou_blade(a,stroke);
            else if(ascendance.double_cut||(ascendance.radiance==RADIANCE_FOLLOW&&radiance_pursuit.independent))ascendance_follow_blade(a);
            else if(ascendance.radiance!=RADIANCE_CHASE)
#endif
            if(!wilds_clip(world.source)){
#if ML_ASCENDANCE_PROTOTYPE
                /* Fit the two existing GU hit intervals to the source return clip. */
                if(musou.phase==MUSOU_CUT&&world.slot==40){
                    const WEvent *ret=w_event(world.source,real(a,FRAME)*(39.f/32));
                    if(ret)lingyue_musou_finish_hit(a,ret);
                }else if(musou.phase==MUSOU_DASH||musou.phase==MUSOU_CUT){
                    lingyue_musou_finish_hit(a,e);
                }else
#endif
                world_hit_tick(a,e);
            }
#else
            world_hit_tick(a,e);
#endif
        }
        world_physics_tick(a);
#if ML_ASCENDANCE_PROTOTYPE
        /* The reference ramps ActionMotionSpeed every update. Only the phases
         * that have an authored curve report here; everything else keeps the
         * value the event path wrote. */
        if(world_bound_attack_rate>1.f){
            ascendance_apply_attack_rate(a,world_bound_attack_rate);
        }else{
            float frame_speed=ascendance_frame_speed(a);
            if(frame_speed>=0){
                set_real(a,0x50c,frame_speed);set_real(a,0x14b0,frame_speed);
            }
        }
#endif
        if(world.move==W_SHEATHE&&real(a,FRAME)>=w_clip(world.source)->length-1){
            set_real(a,0x50c,0);set_real(a,0x14b0,0);
        }
    }
    return world.move||d.kind==W_ENTER||d.kind==W_NATIVE||d.kind==W_NATIVE_R||d.kind==W_ROLL||d.kind==W_IDLE;
}
#include "world_damage_compat.h"
static unsigned world_install(void)
{
#if ML_WORLD_FX_PROBE
    if(!world_fx_install())return 0;
#endif
#if (ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION) && WORLD_AUDIO_WEAPON
    if(!world_audio_install())return 0;
#endif
    const u32 entries[][3]={
#if ML_WILDS_MOVES
        {0xbac204,0xe92d4ff0u,0xe24dd014u},
        {0x1fec4,0xe92d4ff0u,0xe24dd034u},
        {0x280c1c,0xe30014f4u,0xe3002508u},
        {0x657338,0xe92d4bf0u,0xe1a04000u},
        {0x9a828,0xe92d48f0u,0xe24dd008u},
        {0x2807a0,0xe30114a4u,0xe7902001u},
        {0x119ed98,0xe92d4070u,0xe1a05001u},
        {0x2a33a8,0xe92d4830u,0xe1a05001u},
#endif
#if ML_ASCENDANCE_PROTOTYPE
        {0x17f188,0xe92d48f0u,0xe1a04003u},
#endif
        {0x2a309c,0xe92d4070u,0xe1a04000u},
        {0x64b414,0xe92d48f0u,0xe1a04000u},
        {0x23d80,0xe92d4070u,0xed2d8b02u},
        {0x2ee508,0xe92d4ff0u,0xe28db01cu},
        {0x2f0c9c,0xe92d47f0u,0xe1a07002u},
        {0x2f4ee0,0xe92d4070u,0xe1a04000u},
        {0x169548,0xe92d4830u,0xe1a04000u},
        {0x119eef0,0xe92d4830u,0xe1a04000u},
        {0x16bb38,0xe92d4ff0u,0xe28db01cu},
        {0x16c0c0,0xe92d4ff0u,0xe28db01cu},
        {0x171350,0xe92d4ff0u,0xe24dd004u},
        {0x474674,0xe92d4830u,0xe1a04000u},
        {0x17260c,0xe92d4ff0u,0xe24dd0e4u},
        {0x2e846c,0xe92d4df0u,0xe28db018u}
    };
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i){
        const u32 *entry=(const u32 *)(main_base+entries[i][0]);
        const u32 *detour=entry[0]==0xe51ff004u&&entry[1]&&!(entry[1]&3)?(const u32 *)(uptr)entry[1]:0;
        unsigned valid=entries[i][0]==0x2ee508?world_player_damage_entry_matches(entry,detour):
            entries[i][0]==0x2e846c?world_floor_entry_matches(entry,detour):
            entry[0]==entries[i][1]&&entry[1]==entries[i][2];
        if(!valid){
            log_event("world_entry_refused",entries[i][0],entry[0],entry[1],entries[i][1]);
            return 0;
        }
    }
    const u32 *monster_entry=(const u32 *)(main_base+0x170968);
    const u32 *monster_detour=monster_entry[0]==0xe51ff004u&&monster_entry[1]&&!(monster_entry[1]&3)?(const u32 *)(uptr)monster_entry[1]:0;
    if(!world_damage_entry_matches(monster_entry,monster_detour)){
        log_event("world_entry_refused",0x170968,monster_entry[0],monster_entry[1],0xe92d41f0u);return 0;
    }
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
    if(!world_install_impact_sound())return 0;
#endif
    forge_hook_create((void *)(main_base+0x2a309c),(void *)world_gauge_gain,(void **)&world_original_gauge_gain);
    forge_hook_create((void *)(main_base+0x2ee508),(void *)world_damage,(void **)&world_original_damage);
    forge_hook_create((void *)(main_base+0x2f4ee0),(void *)world_flinch_resist,(void **)&world_original_flinch_resist);
    forge_hook_create((void *)(main_base+0x2f0c9c),(void *)world_hp,(void **)&world_original_hp);
#if ML_ASCENDANCE_PROTOTYPE
    forge_hook_create((void *)(main_base+0x17f188),(void *)step_round_pair,(void **)&step_round_original_pair);
    if(!step_round_original_pair)return 0;
#endif
#if ML_ASCENDANCE_PROTOTYPE
    forge_hook_create((void *)(main_base+0x2e7aa0),(void *)ascendance_wall,(void **)&ascendance_original_wall);
    if(!ascendance_original_wall)return 0;
#endif
    forge_hook_create((void *)(main_base+0x2e846c),(void *)world_floor,(void **)&world_original_floor);
    forge_hook_create((void *)(main_base+0x169548),(void *)world_attack_scale,(void **)&world_original_attack_scale);
    forge_hook_create((void *)(main_base+0x64b414),(void *)world_spirit_hud,(void **)&world_original_spirit_hud);
    forge_hook_create((void *)(main_base+0x119eef0),(void *)world_timers,(void **)&world_original_timers);
    forge_hook_create((void *)(main_base+0x16bb38),(void *)world_player_query,(void **)&world_original_player_query);
    forge_hook_create((void *)(main_base+0x16c0c0),(void *)world_monster_query,(void **)&world_original_monster_query);
    forge_hook_create((void *)(main_base+0x170968),(void *)world_monster_damage,(void **)&world_original_monster_damage);
    forge_hook_create((void *)(main_base+0x474674),(void *)world_shell_position,(void **)&world_original_shell_position);
    forge_hook_create((void *)(main_base+0x17260c),(void *)world_attack_shapes,(void **)&world_original_attack_shapes);
    forge_hook_create((void *)(main_base+0x23d80),(void *)world_camera,(void **)&world_original_camera);
    const u32 *view_entry=(const u32 *)(main_base+0x1c33c);
    if(view_entry[0]==0xe92d4800u&&view_entry[1]==0xe281c040u)
        forge_hook_create((void *)view_entry,(void *)world_view,(void **)&world_original_view);
    if(!world_original_view)
        log_event("world_steady_view_unavailable",0x1c33c,view_entry[0],view_entry[1],0);
#if ML_WILDS_MOVES
    forge_hook_create((void *)(main_base+0x119ed98),(void *)world_rank_delta,(void **)&world_original_rank_delta);
    if(!world_original_rank_delta)return 0;
    forge_hook_create((void *)(main_base+0x2b7240),(void *)world_open_hit,(void **)&world_original_open_hit);
    if(!world_original_open_hit)return 0;
    forge_hook_create((void *)(main_base+0xbac204),(void *)world_pad_map,(void **)&world_original_pad_map);
    forge_hook_create((void *)(main_base+0x280c1c),(void *)round_frame_crossing,(void **)&original_frame_crossing);
    forge_hook_create((void *)(main_base+0x1fec4),(void *)world_target_selection,(void **)&world_original_target_selection);
    forge_hook_create((void *)(main_base+0x657338),(void *)world_target_panel,(void **)&world_original_target_panel);
    forge_hook_create((void *)(main_base+0x9a828),(void *)world_monster_reaction,(void **)&world_original_monster_reaction);
    /* Only the extended wound stagger uses this entry. A monster port may own it. */
    const u32 *action_entry=(const u32 *)(main_base+0x76154);
    if(action_entry[0]==0xe92d4ff0u&&action_entry[1]==0xe24dd014u)
        forge_hook_create((void *)action_entry,(void *)world_monster_action,(void **)&world_original_monster_action);
    if(!world_original_monster_action)
        log_event("wilds_focus_stagger_unavailable",0x76154,action_entry[0],action_entry[1],0);
    forge_hook_create((void *)(main_base+0x2807a0),(void *)world_focus_turn,(void **)&world_original_turn);
    if(!world_original_pad_map||!world_original_target_selection||!original_frame_crossing||!world_original_target_panel||!world_original_monster_reaction||!world_original_turn)return 0;
#endif
    return world_original_gauge_gain&&world_original_spirit_hud&&world_original_flinch_resist&&world_original_damage&&world_original_hp&&world_original_floor&&world_original_attack_scale&&world_original_timers&&world_original_player_query&&world_original_monster_query&&world_original_monster_damage&&world_original_shell_position&&world_original_attack_shapes&&world_original_camera;
}
