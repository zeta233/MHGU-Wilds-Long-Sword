/* The projected cut keeps its birth point as the hunter moves past it. */
enum { RADIANCE_DRAW, RADIANCE_CONTACT, RADIANCE_FAN, RADIANCE_OVERLAY_COUNT };
typedef struct {
    void *actor;
    unsigned area,shown;
    float born,position[3],axis[3];
} WRadianceOverlay;
static WRadianceOverlay world_radiance_layers[RADIANCE_OVERLAY_COUNT];
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
    world_radiance_anchor(a,position,heading,74,118);
    world_radiance_birth(a,RADIANCE_FAN,position,heading,.32f);
    log_event("radiance_second_fan_fx",(u32)(uptr)a,(u32)(1000*frame),bind_generation,5);
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

static void world_radiance_overlay(void *a,void *camera)
{
    static const float lifetime[]={.23f,.70f,.23f};
    int view=world_screen_view(camera);
    if(view<0||!(real(a,0x1c)>0))return;
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
