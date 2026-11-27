/* World cFinishStepJump copies ActionParam4[0] (runtime 0x9f0):
 * vector +0x10, gravity +0x20, time rate +0x24. cFinishAirAttack
 * applies ActionParam4[1] at source trigger bit4, frame30 (runtime0xa20).
 * File offsets1476..1512 are retained in world_physics_source.json.
 * GU velocities use simulation frames; the native floor solver owns position.
 */
static NativeCall world_original_floor;
#if ML_ASCENDANCE_PROTOTYPE
static struct {void *actor; unsigned epoch,stage; float last_frame,prep,prep_frame;} step_foresight_round;
/* Grip yaw/pitch in half-angle tangents, keyed by roundslash action frame. */
static const float step_round_grip_keys[][2]={
    {0.0000000f,0.0000000f},
    {-0.0261859f,0.1183578f},
    {0.0261859f,0.2400787f},
    {0.2308682f,0.3249197f},
    {0.4348124f,0.3249197f},
    {0.5205670f,0.3057307f},
    {0.4877326f,0.2679492f},
    {0.4400105f,0.2080003f},
    {0.3939105f,0.1449931f},
    {0.3689195f,0.1051042f},
    {0.3530484f,0.0804704f},
    {0.2991196f,0.0591211f},
    {0.2230854f,0.0410563f},
    {0.1662021f,0.0262760f},
    {0.1400120f,0.0147803f},
    {0.1213351f,0.0065690f},
    {0.1054145f,0.0016423f},
    {0.0874887f,0.0000000f},
    {0.0787017f,0.0000000f},
    {0.0699268f,0.0000000f},
    {0.0611626f,0.0000000f},
    {0.0524078f,0.0000000f},
    {0.0392901f,0.0000000f},
    {0.0261859f,0.0000000f},
    {0.0130907f,0.0000000f},
    {0.0611626f,0.0000000f},
    {0.1316525f,0.0000000f},
    {0.1227845f,0.0000000f},
    {0.0524077f,0.0000000f},
    {0.0436609f,0.0000000f},
    {0.1006947f,0.0000000f},
    {0.0830936f,0.0000000f},
    {0.0567841f,0.0000000f},
    {0.0567842f,0.0000000f},
    {0.3888787f,0.0000000f},
    {0.4091108f,0.0000000f},
    {-0.0087268f,0.0000000f},
    {-0.1289900f,-0.0725581f},
    {-0.1414309f,-0.0927672f},
    {-0.0867852f,-0.0212089f},
    {-0.0498705f,0.0371926f},
    {-0.0495205f,0.0114323f},
    {-0.0297667f,-0.0300287f},
    {0.0037525f,-0.0419998f},
    {0.0201613f,-0.0434861f},
    {-0.0007854f,-0.0456720f},
    {-0.0708038f,-0.0350955f},
    {-0.1857001f,-0.0204232f},
    {-0.3509790f,-0.0153601f},
    {-0.5672713f,-0.0331734f},
    {-0.8358333f,-0.0824785f},
    {-1.1795769f,-0.1515036f},
    {-1.5855170f,-0.2478454f},
    {-1.4288256f,-0.2396730f},
    {-1.2424396f,-0.2261019f},
    {-1.0577451f,-0.2082724f},
    {-0.8877688f,-0.1873081f},
    {-0.7349867f,-0.1643003f},
    {-0.6028765f,-0.1403000f},
    {-0.4797637f,-0.1154386f},
    {-0.3674374f,-0.0907173f},
    {-0.2669133f,-0.0671151f},
    {-0.1792497f,-0.0456014f},
    {-0.1060319f,-0.0271476f},
    {-0.0495988f,-0.0127335f},
    {-0.0130423f,-0.0033508f},
    {0.0000000f,0.0000000f}
};
enum { STEP_ROUND_GRIP_END = sizeof(step_round_grip_keys)/sizeof(step_round_grip_keys[0])-1 };
static unsigned step_round_grip_offsets(void *a,float *yaw,float *pitch)
{

    if(step_foresight_round.actor!=a||byte(a,AREA)!=step_foresight_round.epoch||
       u16(a,STATE)!=16||(word(a,AIR)&0x400))return 0;
    float f=real(a,FRAME);
    if(step_foresight_round.stage==1 && u16(a,MOTION)==0x500+224){
        if(!(f>=0))return 0;
        /* Command 39 is the solo Spirit follow-up; its buffered grant survives release. */
        unsigned ready=world.source==140&&world.hit_success&&world.counter_success&&
            (w_command_has(world.controls.commands.buffered,39)||
             w_command_has(world.controls.commands.executable,39));
        float dt=f-step_foresight_round.prep_frame;
        if(dt<0||step_foresight_round.prep_frame<0)dt=0;
        step_foresight_round.prep_frame=f;
        float p=step_foresight_round.prep+(ready?dt:-dt)/6.f;
        if(p<0)p=0;
        if(p>1)p=1;
        step_foresight_round.prep=p;
        /* The return and native roundslash use different drawn sockets. */
        *yaw=.185339f*p*p*(3-2*p);
        *pitch=0;
        return p>0;
    }
    if(step_foresight_round.stage!=2||u16(a,ACTION)!=30||!(f>=0&&f<STEP_ROUND_GRIP_END))return 0;
    if(f>=STEP_ROUND_GRIP_END){*yaw=*pitch=0;return 1;}
    unsigned frame_index=(unsigned)f;
    float fraction=f-frame_index;
    *yaw=step_round_grip_keys[frame_index][0]+fraction*(step_round_grip_keys[frame_index+1][0]-step_round_grip_keys[frame_index][0]);
    *pitch=step_round_grip_keys[frame_index][1]+fraction*(step_round_grip_keys[frame_index+1][1]-step_round_grip_keys[frame_index][1]);
    if(f<5&&step_foresight_round.prep>0){
        static const float carry[]={1,.81f,.67f,.35f,.05f,0};
        unsigned i=(unsigned)f;
        float p=step_foresight_round.prep;
        float offset=-.185339f*p*p*(3-2*p)*(carry[i]+(f-i)*(carry[i+1]-carry[i]));
        *yaw=(*yaw+offset)/(1-*yaw*offset);
    }
    return 1;
}
/* The pair solver adds each hunter correction to a shared accumulator. */
static u32 (*step_round_original_pair)(u32,void *,void *,void *,void *);
static struct { void *actor; unsigned area,sample; } pursuit_settle;

static void pursuit_settle_begin(void *a)
{
    pursuit_settle.actor=a;
    pursuit_settle.area=byte(a,AREA);
    pursuit_settle.sample=sample_sequence;
}

static struct { void *actor; unsigned area,sample; } pursuit_musou_bridge;

static void pursuit_musou_bridge_begin(void *a)
{
    pursuit_musou_bridge.actor=a;
    pursuit_musou_bridge.area=byte(a,AREA);
    pursuit_musou_bridge.sample=sample_sequence;
}

static struct { void *actor; unsigned area,sample; } pursuit_cut_bridge;
static struct { void *actor; unsigned area,sample; } pursuit_flash_bridge;

static void pursuit_flash_bridge_begin(void *a)
{
    pursuit_flash_bridge.actor=a;
    pursuit_flash_bridge.area=byte(a,AREA);
    pursuit_flash_bridge.sample=sample_sequence;
}

static void pursuit_cut_bridge_begin(void *a)
{
    pursuit_cut_bridge.actor=a;
    pursuit_cut_bridge.area=byte(a,AREA);
    pursuit_cut_bridge.sample=sample_sequence;
}

static u32 step_round_pair(u32 manager,void *a,void *hunter_vector,void *monster,void *monster_vector)
{
    if(!a||(word(a,AIR)&0x400))
        return step_round_original_pair(manager,a,hunter_vector,monster,monster_vector);

    float frame=real(a,FRAME),weight=1;
    unsigned round=a==step_foresight_round.actor&&step_foresight_round.stage==2&&
        byte(a,AREA)==step_foresight_round.epoch&&u16(a,STATE)==16&&
        u16(a,ACTION)==30&&u16(a,MOTION)==0x57f;
    if(round&&frame>=0&&frame<5){
        /* Preserve the opening contact and ease back to native strength at F4. */
        weight=.45f+.30f*frame;
        if(frame>1){
            float remaining=frame<4 ? (4.f-frame)/3.f : 0.f;
            weight=1.f-.25f*remaining*remaining;
        }
    }
    unsigned settle=a==pursuit_settle.actor&&byte(a,AREA)==pursuit_settle.area&&
        sample_sequence-pursuit_settle.sample<=20&&u16(a,MOTION)==0x501&&frame>=0&&frame<8;
    if(settle&&!round&&frame<5){
        /* Restore body separation over the same idle frames that recover the stance. */
        float phase=frame*.2f;
        weight=.33f+.67f*phase*phase*(3.f-2.f*phase);
    }
    /* The old pose survives one update after the Musou decision. */
    unsigned bridge=a==pursuit_musou_bridge.actor&&byte(a,AREA)==pursuit_musou_bridge.area&&
        sample_sequence-pursuit_musou_bridge.sample<=2&&u16(a,STATE)==16&&u16(a,MOTION)==0x500+PURSUIT_SECOND_SLOT;
    if(bridge)weight=.33f;
    /* The transition has selected the second cut, but its pose is pending.
     * Cap the total horizontal push submitted by the native pair solver. */
    unsigned cut_bridge=a==pursuit_cut_bridge.actor&&byte(a,AREA)==pursuit_cut_bridge.area&&
        sample_sequence-pursuit_cut_bridge.sample<=2&&u16(a,STATE)==16&&
        world.slot==PURSUIT_SECOND_SLOT&&u16(a,MOTION)==0x500+PURSUIT_FIRST_SLOT;
    unsigned flash_bridge=a==pursuit_flash_bridge.actor&&byte(a,AREA)==pursuit_flash_bridge.area&&
        sample_sequence-pursuit_flash_bridge.sample<=3&&u16(a,STATE)==16&&
        radiance_pursuit_handoff_slot(world.slot);
    if(weight>=1&&!round&&!cut_bridge&&!flash_bridge&&!settle)
        return step_round_original_pair(manager,a,hunter_vector,monster,monster_vector);

    float x=real(a,0x1040),z=real(a,0x1048);
    u32 result=step_round_original_pair(manager,a,hunter_vector,monster,monster_vector);
    float dx=real(a,0x1040)-x,dz=real(a,0x1048)-z;
    if(cut_bridge||flash_bridge){
        float total_x=x+dx,total_z=z+dz;
        float squared=total_x*total_x+total_z*total_z;
        const float limit=15.f;
        if(squared>limit*limit){
            float scale=limit/att_sqrt(squared);
            set_real(a,0x1040,total_x*scale);
            set_real(a,0x1048,total_z*scale);
        }
    }
    if(dx!=0||dz!=0){
        if(bridge){
            /* Keep deep overlaps from turning this handoff into a single long step. */
            const float limit=15.f;
            float length_squared=dx*dx+dz*dz;
            if(length_squared*weight*weight>limit*limit)
                weight=limit/att_sqrt(length_squared);
        }
        if(weight<1){set_real(a,0x1040,x+dx*weight);set_real(a,0x1048,z+dz*weight);}
        if(settle){
            /* Recover body separation progressively instead of spending the
             * pending overlap on the first held-sword idle frame. */
            const float limit=12.f+3.f*frame;
            float total_x=real(a,0x1040),total_z=real(a,0x1048);
            float squared=total_x*total_x+total_z*total_z;
            if(squared>limit*limit){
                float scale=limit/att_sqrt(squared);
                set_real(a,0x1040,total_x*scale);set_real(a,0x1048,total_z*scale);
            }
        }
        if(round&&frame>=0&&frame<5)
            log_event("step_round_push",word(a,FRAME),word(&dx,0),word(&dz,0),word(&weight,0));
    }
    return result;
}
static void step_round_ease_floor(void *a)
{
    if(step_foresight_round.actor!=a||step_foresight_round.stage!=2)return;
    float f=real(a,FRAME);
    if(byte(a,AREA)!=step_foresight_round.epoch||u16(a,STATE)!=16||u16(a,ACTION)!=30||
       u16(a,MOTION)!=0x500+127||!(f>=0&&f<STEP_ROUND_GRIP_END)||(word(a,AIR)&0x400)){
        step_foresight_round.actor=0;return;
    }
    if(f>=2||f<=step_foresight_round.last_frame)return;
    step_foresight_round.last_frame=f;
    float weight=.35f+.35f*f;
    if(weight>1)weight=1;
    float x=real(a,0x1000),z=real(a,0x1008);
    float dx=real(a,0x40)-x,dz=real(a,0x48)-z;
    set_real(a,0x40,x+dx*weight);set_real(a,0x48,z+dz*weight);
    ((void (*)(void *))(main_base+0x53a9fc))(a);
    log_event("step_round_ease",(u32)f,(u32)((dx*dx+dz*dz)*100),0,0);
}
#endif
static unsigned world_air_owned,world_air_actor,world_air_epoch,world_down_applied;
static unsigned world_floor_depth,world_saved_mode,world_saved_tag;
static float world_saved_gravity;
static float world_rise_dx,world_rise_dz,world_rise_hold;
static unsigned world_air_sample,world_air_steering,world_air_lift_sample;
static float world_air_lift_age,world_air_lift_adjust;
static unsigned world_air_owner(void *a)
{
    return world_air_owned&&world_air_actor==(u32)(uptr)a&&world_air_epoch==byte(a,AREA);
}
static void world_zero_velocity(void *a)
{
    for(unsigned i=0;i<3;++i){put(a,0x1010+4*i,0);put(a,0x1020+4*i,0);}
}
static float world_sin(unsigned angle)
{
    int n=(int)(angle&65535u);if(n>32768)n-=65536;
    if(n>16384)n=32768-n;else if(n<-16384)n=-32768-n;
    float x=(float)n*(3.14159265358979323846f/32768.0f),t=x*x;
    return x*(1+t*(-1.0f/6+t*(1.0f/120+t*(-1.0f/5040+t*(1.0f/362880-t/39916800)))));
}
static void world_physics_end(void *a)
{
    if(!world_air_owned)return;
    if(world_air_owner(a)){
        unsigned airborne=real(a,0x44)>real(a,0x10f0)+1;
        *(unsigned char *)((uptr)a+0x2505)=airborne?2:world_saved_mode;
        *(unsigned char *)((uptr)a+0x2ed3)=airborne?1:world_saved_tag;
        if(!airborne)world_zero_velocity(a);
        set_real(a,0x1024,airborne&&world_saved_gravity>=0?-.2272727f:world_saved_gravity);
    }
    world_air_owned=world_down_applied=world_air_steering=0;
}
static void world_physics_begin(void *a,unsigned move)
{
    if(move<W_RISE||move>W_DOWN){world_physics_end(a);return;}
    if(!world_air_owner(a)){
        world_air_owned=1;world_air_actor=(u32)(uptr)a;world_air_epoch=byte(a,AREA);
        world_saved_mode=byte(a,0x2505);world_saved_tag=byte(a,0x2ed3);
        world_saved_gravity=real(a,0x1024);
    }
    *(unsigned char *)((uptr)a+0x2505)=2;
    *(unsigned char *)((uptr)a+0x2ed3)=1;
    if(move==W_RISE){
        world_zero_velocity(a);
        /* Two live World runs: 16 updates at 0.02, ending at F0.32.
         * This timer begins at GU motion bind, after the hit request. */
        world_rise_hold=16.0f/60.0f;world_air_sample=sample_sequence;
        set_real(a,0x1010,world_rise_dx/60);set_real(a,0x1018,world_rise_dz/60);
    }else if(move==W_AIR){
        /* Keep gravity native and spread the launch impulse over the join.
         * The temporary velocity corrections sum to zero at unit timesteps. */
        world_air_lift_sample=sample_sequence;
        world_air_lift_age=0;
        world_air_lift_adjust=-8;
        set_real(a,0x1014,1050.0f*1.3f/60.0f+world_air_lift_adjust);
        set_real(a,0x1024,-1250.0f*1.3f*1.3f/3600.0f);
        put(a,0x1010,0);put(a,0x1018,0);
        world_air_steering=1;world_air_sample=sample_sequence-1;
        world_down_applied=0;
    }
}
static void world_down_touchdown(void *a)
{
    /* A floor contact consumes the pending dive impulse. Leave gravity and
     * the floor position to GU so a later change of support is still native. */
    if(!world_down_applied)
        log_event("world_helm_skip_dive",(u32)(uptr)a,
            (u32)(real(a,FRAME)*1000.f),word(a,0x44),word(a,0x10f0));
    world_down_applied=1;
    put(a,0x1014,0);
}
static void world_physics_tick(void *a)
{
    if(!world_air_owner(a)||!world.move||!world_bound||u16(a,MOTION)!=0x500+world.slot||
       world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA))return;
    *(unsigned char *)((uptr)a+0x2505)=2;
    *(unsigned char *)((uptr)a+0x2ed3)=1;
    if(world.move==W_RISE){
        if(world_air_sample!=sample_sequence){
            world_air_sample=sample_sequence;float dt=real(a,0x1c)/60;
            if(dt>0&&dt<.5f)world_rise_hold-=dt;
        }
        float speed=world_rise_hold>0.00001f?.02f:1;
        set_real(a,0x50c,speed);set_real(a,0x14b0,speed);
    }
    if(world.move==W_AIR&&world_air_lift_age<6&&world_air_lift_sample!=sample_sequence){
        world_air_lift_sample=sample_sequence;
        float frames=real(a,0x1c),velocity=real(a,0x1014);
        if(velocity<=0)world_air_lift_age=6;
        else if(frames>0&&frames<=4){
            static const float corrections[]={-8,-4,0,4,5,3,0};
            world_air_lift_age+=frames;
            if(world_air_lift_age>6)world_air_lift_age=6;
            unsigned i=(unsigned)world_air_lift_age;
            float offset=corrections[i];
            if(i<6)offset+=(corrections[i+1]-offset)*(world_air_lift_age-i);
            set_real(a,0x1014,velocity+offset-world_air_lift_adjust);
            world_air_lift_adjust=offset;
        }
    }
    if(world_air_steering&&world_air_sample!=sample_sequence){
        world_air_sample=sample_sequence;
        /* The jump is steerable through its apex, until the dive takes
         * ownership of velocity (MOT173 trigger, frame 30). */
        if(world.move==W_DOWN&&real(a,FRAME)>=30){
            put(a,0x1010,0);put(a,0x1018,0);
            world_air_steering=0;
        }else{
            MLObservation stick=observe(a,1);
            /* Wilds cKabutowariRunUpJump: 2 m/s horizontal velocity
             * at 1.3 time rate. GU uses centimetres per simulation frame. */
            float speed=stick.magnitude>=50?(200.f*1.3f/60.f)*stick.magnitude/128.f:0;
            float blend=real(a,0x1c)*.3f;
            if(blend>1)blend=1;
            if(blend>0){
                float x=real(a,0x1010),z=real(a,0x1018);
                set_real(a,0x1010,x+(speed*world_sin(stick.input_heading)-x)*blend);
                set_real(a,0x1018,z+(speed*world_sin(stick.input_heading+16384)-z)*blend);
            }
        }
    }
    if(world.move!=W_DOWN||real(a,FRAME)<30)return;
    if(real(a,0x44)<=real(a,0x10f0)+1&&real(a,0x1014)<=0){
        world_down_touchdown(a);
        return;
    }
    unsigned initial=!world_down_applied;
    if(initial||(real(a,0x44)>real(a,0x10f0)+1&&real(a,0x1024)>=0)){
        /* Zeroed gravity can strand this private action above the native floor.
         * Restore only the owned airborne dive, retaining existing fall speed. */
        if(initial){put(a,0x1010,0);put(a,0x1018,0);world_down_applied=1;}
        if(initial||real(a,0x1014)>=0)set_real(a,0x1014,-3000.0f*1.5f/60.0f);
        set_real(a,0x1024,-1200.0f*1.5f*1.5f/3600.0f);
        log_event(initial?"world_dive":"world_dive_resume",(u32)(uptr)a,(u32)real(a,FRAME),word(a,0x44),word(a,0x10f0));
    }
}
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
static void ascendance_step_start(void *a,float origin,float drop);
#endif

static u32 world_floor(void *a,u32 b,u32 c,u32 d)
{
#if ML_WILDS_MOVES
    world_focus_walk_pose(a);
#endif
#if ML_ASCENDANCE_PROTOTYPE
    step_round_ease_floor(a);
#endif
    unsigned owned=local_supported(a)&&world.actor==(u32)(uptr)a&&world.move&&
        world.epoch==byte(a,AREA)&&(world_air_owner(a)||w_ground_step_move(world.move));
    if(!owned){
#if ML_ASCENDANCE_PROTOTYPE
        if(ascendance_resheathe_roll_active(a)){
            ++world_floor_depth;
            u32 result=world_original_floor(a,b,c,d);
            --world_floor_depth;
            ascendance_step_floor(a);
            return result;
        }
        ascendance_step_floor(a);
#endif
        return world_original_floor(a,b,c,d);
    }
    world_physics_tick(a);
#if ML_ASCENDANCE_PROTOTYPE
    ascendance_dash_floor(a,0);
    ascendance_pursuit_floor(a,0);
#endif
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
    unsigned side_rise=world.move==W_SLIDE&&
        (world.source==W_SPIRIT_TWO_LEFT||world.source==W_SPIRIT_TWO_RIGHT)&&
        !world_floor_depth&&!(word(a,AIR)&0x400);
    float previous_y=side_rise?real(a,0x44):0;
#endif
    ++world_floor_depth;
    u32 result=world_original_floor(a,b,c,d);
    --world_floor_depth;
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
    if(side_rise&&!(word(a,AIR)&0x400)){
        float floor=real(a,0x10f0),rise=floor-previous_y;
        if(real(a,0x44)==floor&&rise>=20&&rise<=55)
            ascendance_step_start(a,previous_y,-rise);
    }
#endif
    /* The pre-floor tick sees the last floor sample. Consume the impulse
     * immediately if this solve has just set the descending root down. */
    if(!world_floor_depth&&world_air_owner(a)&&world.move==W_DOWN&&world_bound&&
       u16(a,MOTION)==0x500+world.slot&&real(a,FRAME)>0&&
       real(a,0x44)<=real(a,0x10f0)&&real(a,0x1014)<=0)
        world_down_touchdown(a);
#if ML_ASCENDANCE_PROTOTYPE
    ascendance_step_floor(a);
#endif
    return result;
}
