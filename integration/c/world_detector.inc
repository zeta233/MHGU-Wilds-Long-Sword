/* World counter shells: source object.col group0 is a radius120 capsule,
 * endpoints(0,0,0)/(0,50,0), spawned at the owner's current position.
 * Detect against GU's already transformed attack shapes before body damage.
 * No global hurtbox is enlarged and no native collision report is fabricated.
 */
typedef struct {float remaining,point[3];} WDetector;
static WDetector world_detectors[3];
static unsigned world_detector_spawned;
static u32 (*world_original_player_query)(void *,void *,void *);
static u32 (*world_original_attack_shapes)(void *);
static unsigned world_detected_attack;
#if ML_WILDS_MOVES
static unsigned world_body_actor,world_body_epoch,world_body_sample;
static void *world_body_owner;
#endif
#if ML_WILDS_MOVES || ML_ASCENDANCE_PROTOTYPE
static void *world_attack_monster(const void *attack)
{
    /* Native 16938c uses the original monster when a shell owns the hitbox. */
    if(byte(attack,0x10)==1)return (void *)(uptr)word(attack,0xc);
    if(byte(attack,0x6e)==1)return (void *)(uptr)word(attack,0x70);
    return 0;
}
#endif
#if ML_ASCENDANCE_PROTOTYPE
typedef struct {
    const void *attack,*owner;
    unsigned id;
    float duration;
} WAttackClock;
static WAttackClock world_attack_clocks[128];
static int (*world_original_attack_tick)(void *);
static float world_body_offset_delay;
static OffsetRule world_body_offset_rule;

static OffsetRule world_offset_rule(const void *attack)
{
    if(byte(attack,0x10)!=1)return OFFSET_NORMAL;
    const void *owner=world_attack_monster(attack);
    if(!owner)return OFFSET_NORMAL;
    unsigned species=word(owner,0)-main_base,action=u16(owner,0x73e0),motion=u16(owner,MOTION);
    unsigned group=word(attack,0x88),type=byte(attack,0x3a);
    if(species==0x17cab40){
        if(action==0x1e07&&motion==517&&type==1&&(group==16||group==31))
            return OFFSET_BREAK; /* Lagiacrus jumping bite. */
        if(action==0x1007&&motion==514&&type==2&&group==3)
            return OFFSET_FORCE; /* First tail sweep, before the return sweep. */
    }
    if(species==0x17eecd8&&action==0x207&&motion==536&&type==31&&group==1)
        return OFFSET_STOCK_ONLY; /* Seregios's sweeping leg attack. */
    return OFFSET_NORMAL;
}

static int world_attack_tick(void *attack)
{
    if(byte(attack,5)&&world_attack_monster(attack)){
        unsigned id=u16(attack,0x34);
        const void *owner=(const void *)(uptr)word(attack,0xc);
        WAttackClock *clock=&world_attack_clocks[id%128];
        if(clock->attack!=attack||clock->id!=id||clock->owner!=owner)
            *clock=(WAttackClock){attack,owner,id,real(attack,0x60)};
    }
    return world_original_attack_tick(attack);
}

static float world_offset_delay(const void *attack)
{
    if(byte(attack,0x10)!=1){
        void *source=world_attack_monster(attack);
        if(source)
            log_event("ascendance_offset_shell",word(attack,0xc),(u32)(uptr)source,
                byte(attack,0x3a),u16(attack,0x34));
    }
    if(!world_original_attack_tick)return 0;
    unsigned id=u16(attack,0x34);
    const WAttackClock *clock=&world_attack_clocks[id%128];
    const void *owner=(const void *)(uptr)word(attack,0xc);
    if(!owner||clock->attack!=attack||clock->owner!=owner||clock->id!=id)return 0;
    /* GU 16919c counts down +0x5c before hitbox activation and +0x60
     * during activity. Use the initial active duration and remaining time. */
    float total=clock->duration,remaining=real(attack,0x60);
    /* A 9999-frame travelling shell ends on impact or despawn. Its fallback
     * lifetime does not describe the duration of a timed impact hitbox. */
    if(!(total>0&&total<9999.f&&remaining>0&&remaining<=total))return 0;
    float rate=(byte(attack,0x31)&2)?real(owner,0x50c):1;
    if(!(rate>0))return 0;
    float frames=(remaining-total*.6f)/rate;
    if(frames<0)frames=0;
    if(frames>10)frames=10;
    log_event("ascendance_offset_clock",id,(u32)(total*1000),(u32)(remaining*1000),(u32)(frames*1000));
    return frames;
}
#endif
static void world_detector_clear(void)
{
    for(unsigned i=0;i<3;++i)world_detectors[i].remaining=0;
    world_detector_spawned=0;
    world_detected_attack=0;
}
static void world_detector_spawn(void *a,unsigned index,float seconds)
{
    WDetector *d=world_detectors+index;d->remaining=seconds;
    for(unsigned k=0;k<3;++k)d->point[k]=real(a,0x40+4*k);
    world_detector_spawned|=1u<<index;
    log_event("world_detector_spawn",(u32)(uptr)a,world.source,index,(u32)real(a,FRAME));
}
static unsigned world_foresight_invulnerable(float frame)
{
    if(world.move!=W_FORESIGHT||!world.foresight_gauge||frame<0)return 0;
#if ML_WILDS_MOVES
    if(world.source==918)return frame<60;
#endif
    return frame<45;
}

static void world_detector_begin(void *a)
{
    world_detector_clear();
    if(world.move==W_SMALL)world_detector_spawn(a,0,.12f);
    else if(world.move==W_BIG)world_detector_spawn(a,0,.25f);
    else if(world.move==W_FORESIGHT&&world.foresight_gauge){
#if ML_WILDS_MOVES
        if(world.source==918)return; /* The red check follows the current position. */
#endif
        world_detector_spawn(a,0,.2f);
    }
}
static void world_detector_tick(void *a,float dt)
{
#if ML_ASCENDANCE_PROTOTYPE
    /* Flash uses its animation clock for both collision and body protection. */
    if(ascendance_independent_flash())return;
#endif
    for(unsigned i=0;i<3;++i)world_detectors[i].remaining-=dt;
#if ML_WILDS_MOVES
    if(world.source==918)return;
#endif
    if(world.move==W_FORESIGHT&&world.foresight_gauge&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        for(unsigned i=1;i<3;++i)
            if(!(world_detector_spawned&(1u<<i))&&real(a,FRAME)>=10.0f*i)
                world_detector_spawn(a,i,.2f);
    }
}
static void world_counter_accept(void *a,unsigned type)
{
    if(world_counter_seen)return;
    world_counter_seen=1;
#if ML_ASCENDANCE_PROTOTYPE
    unsigned radiance_entry=world.move==W_SHEATHE&&ascendance.radiance==RADIANCE_HOLD&&
        ascendance.radiance_entry_counter;
#else
    unsigned radiance_entry=0;
#endif
#if ML_WILDS_MOVES
    if(world.source==918&&!world.valor_foresight)world_red_time(a,RED_AURA_REFUND,919);
#endif
#if ML_WORLD_PRESENTATION
    /* World shell 11's accepted contact calls group 2004, record 0
     * (request 25510). A swing or an outgoing sword hit cannot trigger it. */
    if(world.move==W_BIG||radiance_entry||(world.move==W_FORESIGHT&&world.foresight_gauge))
        world_audio_epv(a,25510,(const float *)((uptr)a+0x40));
    unsigned big_flash=world.move==W_BIG;
#if ML_ASCENDANCE_PROTOTYPE
    /* Radiance owns its release foreground and uses the actual blade contact
     * for the local impact glow. The generic Iai flash would stack over it. */
    if(ascendance_independent_flash())big_flash=0;
#endif
    if(big_flash||radiance_entry)world_big_counter_flash(a);
#endif
    float duration=world.move==W_FORESIGHT?1.0f:2.0f;
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance_independent_flash())duration=.22f;
#endif
    if(!radiance_entry&&world_armor<duration)world_armor=duration;
    log_event("world_counter",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type);
    ((void (*)(void *,u32,uptr,u32))(main_base+0x282624))(a,9,(uptr)a+0x2550,1);
}
#if ML_ASCENDANCE_PROTOTYPE
static void world_flash_offset(void *a,void *owner,unsigned type,OffsetRule rule,float delay)
{
    if(world_counter_seen)return;
    ascendance.flash_offset_owner=owner;
    ascendance.flash_offset_rule=rule;
    ascendance.flash_offset_due=world_elapsed+delay/60;
    if(radiance_pursuit.target&&radiance_pursuit.target!=owner)
        radiance_pursuit=(__typeof__(radiance_pursuit)){0};
#if ML_OFFSET_ALWAYS_STAGGER
    /* Uncounterable attacks are rejected before reaching this function. */
    rule=ascendance.flash_offset_rule=OFFSET_FORCE;
    ascendance.flash_offset_due=world_elapsed;
    ascendance_offset_test_target(owner);
#endif
    world_counter_accept(a,type);
    if(rule!=OFFSET_NORMAL)log_event("ascendance_offset_rule",(u32)(uptr)owner,rule,(u32)(real(a,FRAME)*1000),type);
    ascendance_flash_offset_credit();
    log_event("ascendance_flash_offset",(u32)(uptr)a,(u32)(uptr)owner,(u32)(real(a,FRAME)*1000),type);
}
#endif
static unsigned world_counter_type(unsigned type)
{
    return !(type>=25&&type<=29)&&type!=58&&type!=67&&type!=68&&type!=70;
}
static void world_detect_attack(void *manager,void *attack,void *a)
{
    if(a==(void *)(uptr)world.actor&&local_supported(a)&&world.epoch==byte(a,AREA)&&
       (int16_t)u16(a,HP)>0&&!world_counter_seen&&world.move&&world_bound&&
       u16(a,MOTION)==0x500+world.slot&&world_counter_type(byte(attack,0x3a))&&
       /* Native16938c identifies monster ownership through either collider
        * owner kind10 or originating owner kind6e (monster projectiles). */
       (byte(attack,0x10)==1||byte(attack,0x6e)==1)&&
       word(attack,0xc)!=(u32)(uptr)a){
        unsigned red=0,flash=0;
#if ML_ASCENDANCE_PROTOTYPE
        flash=ascendance_independent_flash();
        if(flash&&!ascendance_flash_offset_window(real(a,FRAME)))return;
#endif
#if ML_WILDS_MOVES
        red=world.move==W_FORESIGHT&&world.source==918;
#endif
        if(red&&!world_foresight_invulnerable(real(a,FRAME)))return;
        void *resource=(void *)(uptr)word(attack,0x84);
        void *group=resource?((void *(*)(void *,unsigned))(*(uptr **)resource)[0x44/4])
            (resource,word(attack,0x88)):0;
        unsigned count=group?word(group,4):0,first=word(attack,0x1c);
        if(count<=64&&first<64&&count<=64-first){
            for(unsigned j=0;j<count&&!world_counter_seen;++j){
                void *entries=(void *)(uptr)word(group,8);
                if(!entries||!word(entries,j*4))continue;
                const void *record=(const void *)((uptr)manager+0x2030+(first+j)*32);
                unsigned type=byte(record,0);float shape[12]={0},contact[4]={0};
                if(type<1||type>3)continue;
                for(unsigned k=0;k<3;++k)shape[k]=real(record,8+4*k);
                if(type==1)shape[3]=real(record,4);
                else{for(unsigned k=0;k<3;++k)shape[4+k]=real(record,20+4*k);shape[8]=real(record,4);}
                unsigned fn=type==1?0x168198:type==2?0x167b5c:0x1683dc;
                for(unsigned i=0;i<((red||flash)?1u:3u)&&!world_counter_seen;++i){
                    WDetector *d=world_detectors+i;
                    if(!red&&!flash&&d->remaining<=0)continue;
                    const float *point=(red||flash)?(const float *)((uptr)a+0x40):d->point;
                    float capsule[12]={point[0],point[1],point[2],0,
                        point[0],point[1]+50,point[2],0,120,0,0,0};
                    if(((unsigned (*)(const void *,const void *,void *))(main_base+fn))(shape,capsule,contact)){
                        log_event("world_detector_contact",(u32)(uptr)a,word(attack,0xc),
                            byte(attack,0x10)|(byte(attack,0x3a)<<8),type|(i<<8));
#if ML_ASCENDANCE_PROTOTYPE
                        if(flash){
                            OffsetRule rule=world_offset_rule(attack);
                            if(rule==OFFSET_BREAK){
                                log_event("ascendance_offset_break",word(attack,0xc),word(attack,0x88),
                                    u16(attack,0x34),(u32)(real(a,FRAME)*1000));
                                return;
                            }
                            world_flash_offset(a,world_attack_monster(attack),
                                byte(attack,0x3a),rule,rule==OFFSET_STOCK_ONLY?0:world_offset_delay(attack));
                        }else
#endif
                            world_counter_accept(a,byte(attack,0x3a));
                        world_detected_attack=(u32)(uptr)attack;
                    }
                }
            }
        }
    }
}
static u32 world_attack_shapes(void *manager)
{
    u32 result=world_original_attack_shapes(manager);
    void *a=(void *)(uptr)world.actor;
    if(!a||!world.move||!world_bound||world_counter_seen)return result;
    /* Native16b15c performs damage-packet and previous-attack-ID filtering
     * before16bb38. Counter shells must not inherit those body-only filters.
     *17260c has just updated every active attack's world-space geometry. */
    void *attack=(void *)(uptr)word(manager,0x2020);
    for(unsigned visited=0;attack&&visited<4096&&!world_counter_seen;++visited){
        if(byte(attack,7)==world.epoch&&(byte(attack,8)&1)&&
           ((unsigned (*)(void *))(main_base+0x169328))(attack))
            world_detect_attack(manager,attack,a);
        attack=(void *)(uptr)word(attack,0x2c);
    }
    return result;
}
static u32 world_player_query(void *manager,void *attack,void *a)
{
    unsigned watched=a==(void *)(uptr)world.actor&&world_detected_attack==(u32)(uptr)attack;
    unsigned before=byte(a,0x31e0);
    u32 result=world_original_player_query(manager,attack,a);
#if ML_WILDS_MOVES
    /* Remember a real monster-body collision until GU processes its damage
     * packet. The collision pass can run before the hunter update. */
    if(!before&&byte(a,0x31e0)&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
       (byte(attack,0x10)==1||byte(attack,0x6e)==1)&&word(attack,0xc)!=(u32)(uptr)a){
        world_body_actor=(u32)(uptr)a;world_body_epoch=world.epoch;
        world_body_sample=sample_sequence;
        world_body_owner=world_attack_monster(attack);
#if ML_ASCENDANCE_PROTOTYPE
        unsigned flash=ascendance_independent_flash()||
            (!world.move&&valor_active(a)&&(ascendance_pad_bits(a)&PAD_BOOST));
        world_body_offset_rule=world_offset_rule(attack);
        world_body_offset_delay=flash&&world_body_offset_rule!=OFFSET_BREAK&&
            world_body_offset_rule!=OFFSET_STOCK_ONLY&&!world_counter_seen?world_offset_delay(attack):0;
#endif
    }
#endif
    if(watched){
        log_event("world_detector_body",(u32)(uptr)a,before,byte(a,0x31e0),(u32)real(a,FRAME));
        world_detected_attack=0;
    }
    return result;
}
