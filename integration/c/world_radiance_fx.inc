/* The projected cut keeps its birth point as the hunter moves past it. */
enum { RADIANCE_DRAW, RADIANCE_CONTACT, RADIANCE_FAN, RADIANCE_OVERLAY_COUNT };
enum { RADIANCE_HEAT_BASE=3,RADIANCE_HEAT_TIP=4 };
typedef struct {
    void *actor;
    unsigned area,shown;
    float born,position[3],axis[3];
} WRadianceOverlay;
static WRadianceOverlay world_radiance_layers[RADIANCE_OVERLAY_COUNT];
static struct {
    void *actor;
    unsigned area,active;
    float born,released,release_gain;
} world_radiance_heat;
extern void ml_radiance_screen(unsigned,unsigned,unsigned,unsigned);

static void world_radiance_birth(void *a,unsigned layer,const float *position,unsigned heading,float pitch)
{
    WRadianceOverlay *effect=world_radiance_layers+layer;
    *effect=(WRadianceOverlay){.actor=a,.area=byte(a,AREA),.born=world_elapsed,
        .axis={world_sin(heading),pitch,world_sin(heading+16384)}};
    for(unsigned k=0;k<3;++k)effect->position[k]=position[k];
}

static void world_radiance_anchor(void *a,float *point,unsigned heading,float forward,float height)
{
    point[0]=real(a,0x40)+forward*world_sin(heading);
    point[1]=real(a,0x44)+height;
    point[2]=real(a,0x48)+forward*world_sin(heading+16384);
}

static void world_radiance_flash_fx(void *a,float frame)
{
    unsigned heading=world.heading;
    float position[3];
    world_radiance_anchor(a,position,heading,195,175);
    world_radiance_birth(a,RADIANCE_DRAW,position,heading+16384,.40f);
    log_event("radiance_flash_foreground_fx",(u32)(uptr)a,(u32)(1000*frame),bind_generation,0);
    log_event("radiance_flash_blur_fx",(u32)(uptr)a,(u32)(1000*frame),bind_generation,0);
}

static void world_radiance_contact_fx(void *a,const void *contact)
{
    world_radiance_birth(a,RADIANCE_CONTACT,contact,world.heading+16384,.18f);
    log_event("radiance_flash_contact_fx",(u32)(uptr)a,(u32)(1000*real(a,FRAME)),bind_generation,0);
}

static void world_radiance_second_fx(void *a,float frame)
{
    if(world_sfx_seen&(1u<<22))return;
    world_sfx_seen|=1u<<22;
    unsigned heading=u16(a,HEADING);
    float position[3];
    world_radiance_anchor(a,position,heading,74,72);
    world_radiance_birth(a,RADIANCE_FAN,position,heading,.32f);
    log_event("radiance_second_fan_fx",(u32)(uptr)a,(u32)(1000*frame),bind_generation,0);
}

static unsigned world_radiance_project(const float *point,unsigned view)
{
    _Alignas(16) float position[4]={point[0],point[1],point[2],1.f}, screen[4];
    ((void (*)(float *,const float *,unsigned))(main_base+0x551340))(screen,position,view);
    /* Include a quarter-screen margin so a large slash clips cleanly. */
    if(!(screen[3]>0&&screen[2]>0&&screen[0]>=-320&&screen[0]<1600&&screen[1]>=-180&&screen[1]<900))return 0;
    unsigned x=1+(unsigned)((screen[0]+320)*(4093.f/1920));
    unsigned y=1+(unsigned)((900-screen[1])*(4093.f/1080));
    return x|(y<<12);
}

#if ML_RECOMP_RUNTIME
#include "radiance_packet.inc"
#endif

static float world_radiance_ease(float t)
{
    t=t<0?0:t>1?1:t;
    return t*t*(3-2*t);
}

static void world_radiance_heat_overlay(void *a,unsigned view)
{
    unsigned area=byte(a,AREA);
    if(world_radiance_heat.actor!=a||world_radiance_heat.area!=area)
        world_radiance_heat=(__typeof__(world_radiance_heat)){.actor=a,.area=area};
    float frame=real(a,FRAME);
    unsigned pursuit=ascendance_owner(a)&&radiance_pursuit.independent&&
        world.actor==(u32)(uptr)a&&world.epoch==area&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move);
    unsigned chase=pursuit&&ascendance.radiance==RADIANCE_CHASE&&world.move==W_THRUST&&
        (world.slot==PURSUIT_BRAKE_SLOT||frame>=PURSUIT_LOOP_START);
    unsigned first=pursuit&&ascendance.radiance==RADIANCE_FOLLOW&&world.move==W_SMALL&&
        world.slot==PURSUIT_FIRST_SLOT&&
        (u16(a,MOTION)!=0x500+PURSUIT_FIRST_SLOT||frame<19.f||radiance_pursuit.rewarded);
    unsigned windup=pursuit&&ascendance.radiance==RADIANCE_FOLLOW&&world.move==W_SMALL&&
        world.slot==PURSUIT_SECOND_SLOT&&
        (u16(a,MOTION)!=0x500+PURSUIT_SECOND_SLOT||frame<PURSUIT_SECOND_HIT_BEGIN);
    unsigned wanted=chase||first||windup;
    if(wanted&&!world_radiance_heat.active){
        world_radiance_heat.active=1;
        world_radiance_heat.born=world_elapsed;
        world_radiance_heat.release_gain=0;
        log_event("radiance_heat_start",(u32)(uptr)a,world.slot,(u32)(frame*1000),0);
    }
    float gain=world_radiance_ease((world_elapsed-world_radiance_heat.born)/.10f);
    if(!wanted&&world_radiance_heat.active){
        world_radiance_heat.active=0;
        world_radiance_heat.released=world_elapsed;
        world_radiance_heat.release_gain=gain;
        log_event("radiance_heat_release",(u32)(uptr)a,world.slot,(u32)(frame*1000),0);
    }
    if(!wanted)gain=world_radiance_heat.release_gain*
        (1-world_radiance_ease((world_elapsed-world_radiance_heat.released)/.14f));
    if(!(gain>0))return;
    void *weapon=(void *)(uptr)word(a,0x23a0);
    const float *blade=weapon&&word(weapon,0x30)==(u32)(uptr)a?world_bone(weapon,0):0;
    if(!blade)return;
    float base[3],tip[3];
    for(unsigned k=0;k<3;++k){
        base[k]=blade[12+k]+blade[8+k]*32;
        tip[k]=blade[12+k]+blade[8+k]*210;
    }
    unsigned start=world_radiance_project(base,view),finish=world_radiance_project(tip,view);
    if(!start||!finish)return;
    unsigned intensity=(unsigned)(gain*255.f);
    ml_radiance_screen(RADIANCE_HEAT_BASE,start,finish,intensity);
#if ML_RECOMP_RUNTIME == 1
    world_radiance_packet(RADIANCE_HEAT_BASE,start,intensity);
    world_radiance_packet(RADIANCE_HEAT_TIP,finish,intensity);
#endif
}

static void world_radiance_overlay(void *a,void *camera)
{
    static const float lifetime[]={.23f,.70f,.215f};
    int view=world_screen_view(camera);
    if(view<0||!(real(a,0x1c)>0))return;
    world_radiance_heat_overlay(a,(unsigned)view);
    for(unsigned layer=0;layer<RADIANCE_OVERLAY_COUNT;++layer){
        WRadianceOverlay *effect=world_radiance_layers+layer;
        float age=world_elapsed-effect->born;
        if(effect->actor!=a||effect->area!=byte(a,AREA)||age<0||age>=lifetime[layer]){
            effect->actor=0;
            continue;
        }
        float end[3];
        for(unsigned k=0;k<3;++k)end[k]=effect->position[k]+130*effect->axis[k];
        unsigned start=world_radiance_project(effect->position,(unsigned)view);
        if(!start)continue;
        unsigned finish=world_radiance_project(end,(unsigned)view);
#if ML_RECOMP_RUNTIME
        world_radiance_packet(layer,start,(u32)(age*480));
#endif
        if(layer!=RADIANCE_CONTACT||age<.36f)ml_radiance_screen(layer,start,finish,(u32)(age*480));
        if(!effect->shown){
            effect->shown=1;
            log_event("radiance_foreground_projected",layer,start,finish,(u32)(age*1000));
        }
    }
}
