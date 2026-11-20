/* Private native sound resources. The probe mailbox exists only in an explicit
 * audio development build; it does not infer or substitute source events. */
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
#include "world_audio_events.h"
#include "world_audio_index.h"
#if ML_WILDS_MOVES
#include "wilds_audio_data.h"
static uint64_t wilds_audio_seen[2];
static unsigned wilds_follow_audio_seen;
#else
#define WILDS_AUDIO_RESOURCE_COUNT WORLD_AUDIO_RESOURCE_COUNT
#define WILDS_AUDIO_REQUEST_COUNT WORLD_AUDIO_REQUEST_COUNT
#endif
static const WAudioCueRange *world_audio_active_range;
static unsigned world_audio_range_source=~0u;
static void *world_audio_archive,*world_audio_request;
static unsigned world_audio_failed,world_audio_ready;
#if ML_WORLD_AUDIO_PROBE
static volatile unsigned world_audio_probe_command;
#else
static const unsigned world_audio_probe_command;
#endif
static unsigned world_audio_seen[(WORLD_AUDIO_CUE_COUNT+31)/32],world_audio_actor,world_audio_epoch,world_audio_sample;
static float world_audio_clock;
typedef struct {unsigned active,request,actor,epoch,local;float remaining,point[3];} WAudioJob;
static WAudioJob world_audio_jobs[16];
#if WORLD_AUDIO_WEAPON
static unsigned world_audio_last_choice[WORLD_AUDIO_WEAPON_GROUP_COUNT],world_audio_rng;
static u32 (*world_audio_original_position)(void *);
static unsigned world_audio_live_actor,world_audio_live_epoch;
typedef struct {unsigned serial,actor,epoch;} WAudioSpatialVoice;
static WAudioSpatialVoice world_audio_spatial_voices[128];
static unsigned world_audio_weapon_point(void *a,unsigned request,float *point)
{
    if(request<6||request>=WORLD_AUDIO_CONDITIONAL_FIRST)return 0;
#if WORLD_AUDIO_GROUND
    if(request==WORLD_AUDIO_HELM_WAVE)return 0;
#endif
    void *weapon=(void *)(uptr)word(a,0x23a0);
    if(!weapon||word(weapon,0x30)!=(u32)(uptr)a||word(weapon,0x13d0)!=9)return 0;
    uptr map=word(weapon,0x498);if(!map)return 0;
    const float *m=world_raw_bone(weapon,*(const unsigned char *)map);
    if(!m)return 0;
    const float *offset=world_audio_weapon_points[request];
    for(unsigned k=0;k<3;++k)point[k]=m[12+k]+m[k]*offset[0]+m[4+k]*offset[1]+m[8+k]*offset[2];
    return 1;
}
static u32 world_audio_position(void *voice)
{
    /* This native callback runs immediately before distance/panning is
     * evaluated. Only our private requests are changed; the normal mixer,
     * attenuation and voice lifecycle still belong to GU. */
    if(world_audio_ready&&word(voice,12)==(u32)(uptr)world_audio_request&&word(voice,172)==1&&
       word(voice,20)==world_audio_live_actor&&world_audio_live_actor){
        void *manager=*(void **)(main_base+0x2126d8c);
        uptr first=(uptr)manager+64,at=(uptr)voice;
        if(manager&&at>=first&&(at-first)%2784==0&&(at-first)/2784<128){
            WAudioSpatialVoice *v=world_audio_spatial_voices+(at-first)/2784;
            unsigned serial=word(voice,52),actor=world_audio_live_actor;
            if(v->serial!=serial||v->actor!=actor){
                *v=(WAudioSpatialVoice){serial,actor,world_audio_live_epoch};
                if(word(voice,16)>=6)log_event("world_audio_follow",actor,word(voice,16),at,serial);
            }
            void *a=(void *)(uptr)actor;float point[3];
            if(v->epoch==world_audio_live_epoch&&word(a,0)==main_base+VT&&
               byte(a,AREA)==v->epoch&&(int16_t)u16(a,HP)>0&&world_audio_weapon_point(a,word(voice,16),point))
                for(unsigned k=0;k<3;++k)set_real(voice,64+4*k,point[k]);
        }
    }
    return world_audio_original_position(voice);
}
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
#include "gu_round_banks.h"
static void **gu_round_resources;
static void *(*gu_original_sound_bank)(void *,unsigned);
static void *gu_round_sound_bank(void *emitter,unsigned group)
{
    void *original=gu_original_sound_bank(emitter,group);
    if(!original||!gu_round_resources||(group!=2&&group!=4))return original;
    void *a=(void *)(uptr)world_audio_live_actor;
    if(!a||(uptr)emitter!=(uptr)a+0x111c||world_contact_audio_scope==2)return original;
    unsigned motion=u16(a,MOTION);
    if(!world_contact_audio_scope&&motion!=0x500+127&&motion!=0x500+148&&motion!=0x500+219)return original;
    static const char prefix[]="sound\\se\\pl\\wp\\swo\\";
    const char *path=(const char *)original+8;
    for(unsigned k=0;k<sizeof(prefix)-1;++k)if(path[k]!=prefix[k])return original;
    unsigned id=255;
    if(group==2){
        const char *p=path+sizeof(prefix)-1;
        if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'||p[2]<'0'||p[2]>'9')return original;
        id=(p[0]-'0')*100+(p[1]-'0')*10+p[2]-'0';
        if(id>=255)return original;
    }
    unsigned i=gu_round_request_index[id];
    return i?gu_round_resources[i]:original;
}
#endif
static unsigned world_audio_install(void)
{
    if(*(u32 *)(main_base+0xbe28c4)!=0xe92d4ff0u||*(u32 *)(main_base+0xbe28c8)!=0xe28db01cu)return 0;
    forge_hook_create((void *)(main_base+0xbe28c4),(void *)world_audio_position,(void **)&world_audio_original_position);
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
    const u32 *resolve=(const u32 *)(main_base+0x4d5e14);
    if(resolve[0]!=0xe1a02000u||resolve[1]!=0xe3a00000u)return 0;
    forge_hook_create((void *)resolve,(void *)gu_round_sound_bank,(void **)&gu_original_sound_bank);
    if(!gu_original_sound_bank)return 0;
#endif
    return world_audio_original_position!=0;
}
#endif

#if ML_WILDS_MOVES && WORLD_AUDIO_WEAPON
static unsigned wilds_wind_actor,wilds_wind_epoch,wilds_wind_last,wilds_wind_speed=1;
static float wilds_wind_previous[3];
static unsigned wilds_audio_random(unsigned seed)
{
    static unsigned state;
    if(!state)state=seed^sample_sequence^0x9e3779b9u;
    state^=state<<13;state^=state>>17;state^=state<<5;
    return state;
}
static void wilds_sample_wind_speed(void *a,float frames)
{
    float point[3];
    if(frames<=0||frames>6||!world_audio_weapon_point(a,6,point)){
        wilds_wind_actor=0;wilds_wind_speed=1;return;
    }
    float distance2=0;
    unsigned actor=(u32)(uptr)a,epoch=byte(a,AREA);
    for(unsigned k=0;k<3;++k){
        point[k]-=real(a,0x40+k*4);
        float delta=point[k]-wilds_wind_previous[k];
        distance2+=delta*delta;
        wilds_wind_previous[k]=point[k];
    }
    // GU blade displacement per animation frame, sampled 80 units from the hilt.
    // Select three rendered points on the Wilds velocity RTPC.
    wilds_wind_speed=1;
    if(actor==wilds_wind_actor&&epoch==wilds_wind_epoch){
        float square=distance2/(frames*frames);
        wilds_wind_speed=square<36.f?0:square>400.f?2:1;
    }
    wilds_wind_actor=actor;wilds_wind_epoch=epoch;
}
static unsigned wilds_resolve_wind(void *a,unsigned request)
{
    if(request<wilds_swing_requests[0]||request>wilds_swing_requests[2])return request;
    // Both of this weapon's two-entry containers avoid the preceding media.
    unsigned variant=wilds_wind_last?((wilds_wind_last-1)^3):(wilds_audio_random((u32)(uptr)a)&3);
    wilds_wind_last=variant+1;
    const unsigned (*swings)[4]=world.source==904?wilds_release_speed_swings:wilds_speed_swings;
    return swings[wilds_wind_speed][variant];
}
#endif
static void world_audio_play(void *a,unsigned request,const float *point)
{
    void *sm=*(void **)(main_base+0x1896f5c);if(!sm||!world_audio_ready)return;
#if ML_WILDS_MOVES && WORLD_AUDIO_WEAPON
    request=wilds_resolve_wind(a,request);
#endif
    unsigned group=byte(a,0x111a)?13:12;
    if(point)
        ((void (*)(void *,u32,void *,u32,void *,const float *,void *))(main_base+0x4ee980))
            (sm,group,world_audio_request,request,a,point,a);
    else
        ((void (*)(void *,u32,void *,u32,void *,void *))(main_base+0x4ee900))
            (sm,group,world_audio_request,request,a,a);
    log_event("world_audio_play",(u32)(uptr)a,request,(u32)(uptr)world_audio_request,(u32)(world_audio_clock*1000000));
}
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
static unsigned world_audio_impact(unsigned id,enum WAudioImpact impact,unsigned ordinal,const float *point)
{
    void *a=(void *)(uptr)world.actor;
    if(!world_audio_ready||!point||!a||a!=(void *)(uptr)world_audio_actor||
       byte(a,AREA)!=world_audio_epoch||!*(void **)(main_base+0x1896f5c))return 0;
    const unsigned *requests=wilds_release_blade_impact_requests;
    const char *event="wilds_native_hit_audio";
    switch(impact){
    case W_SOUND_THRUST_BLADE: requests=wilds_combat_blade_impact_requests;event="wilds_thrust_blade_audio";break;
    case W_SOUND_HELM_BLADE: requests=wilds_helm_impact_requests;event="wilds_helm_blade_audio";break;
    case W_SOUND_RELEASE_BLADE: requests=wilds_combat_blade_impact_requests;event="wilds_release_blade_audio";break;
    case W_SOUND_RELEASE_FOLLOW: requests=wilds_shell_impact_requests;event="wilds_release_impact_audio";break;
    case W_SOUND_HELM_FOLLOW: requests=wilds_shell_impact_requests;event="wilds_helm_impact_audio";break;
    case W_SOUND_IAI_FOLLOW: requests=wilds_iai_impact_requests;event="wilds_iai_impact_audio";break;
    case W_SOUND_CRIMSON_FINISH: event="wilds_crimson_finish_audio";break;
    case W_SOUND_BLADE: break;
    }
    static unsigned last_blade,last_follow;
    unsigned *last=(requests==wilds_release_blade_impact_requests||requests==wilds_combat_blade_impact_requests)?&last_blade:&last_follow;
    unsigned random=wilds_audio_random((u32)(uptr)a);
    unsigned choice=*last?(random%2+*last)%3:random%3;
    *last=choice+1;
    unsigned request=requests[choice];
    world_audio_play(a,request,point);
    if(impact==W_SOUND_THRUST_BLADE||impact==W_SOUND_HELM_BLADE||impact==W_SOUND_RELEASE_BLADE){
        unsigned response=wilds_body_impact_requests[choice];
        world_audio_play(a,response,point);
        log_event("wilds_damage_audio",world.actor,id,response,world.source);
    }else if(impact==W_SOUND_HELM_FOLLOW||impact==W_SOUND_RELEASE_FOLLOW){
        log_event("wilds_damage_audio",world.actor,id,request,ordinal);
    }
    if(impact==W_SOUND_CRIMSON_FINISH){
        const unsigned *accent=world.source==907?wilds_release_impact_requests:wilds_iai_impact_requests;
        world_audio_play(a,accent[choice],point);
        log_event(event,world.actor,id,accent[choice],world.source);
    }else log_event(event,world.actor,id,request,world.source);
    return 1;
}
#endif
static void world_audio_begin(void *a)
{
#if ML_WILDS_MOVES
    wilds_audio_seen[0]=wilds_audio_seen[1]=0;
    wilds_follow_audio_seen=0;
#endif
    for(unsigned i=0;i<sizeof(world_audio_seen)/sizeof(*world_audio_seen);++i)world_audio_seen[i]=0;
    world_audio_actor=(u32)(uptr)a;world_audio_epoch=byte(a,AREA);
}
static void world_audio_schedule(void *a,unsigned request,float delay,const float *point)
{
    if(delay<=0){world_audio_play(a,request,point);return;}
    for(unsigned i=0;i<16;++i)if(!world_audio_jobs[i].active){
        WAudioJob *job=world_audio_jobs+i;
        *job=(WAudioJob){.active=1,.request=request,.actor=(u32)(uptr)a,.epoch=byte(a,AREA),
                         .local=point==0,.remaining=delay};
        if(point)for(unsigned k=0;k<3;++k)job->point[k]=point[k];
        return;
    }
    log_event("world_audio_queue_full",(u32)(uptr)a,request,0,0);
}
#if WORLD_AUDIO_WEAPON
#if ML_WILDS_MOVES
static unsigned wilds_swing_request(unsigned frame)
{
    return wilds_swing_requests[frame%(sizeof(wilds_swing_requests)/sizeof(*wilds_swing_requests))];
}
#endif
static void world_audio_weapon(void *a,unsigned source_request)
{
    log_event("world_weapon_audio_cue",(u32)(uptr)a,source_request,(u32)(real(a,FRAME)*1000),world.source);
#if ML_WILDS_MOVES
    unsigned request;
    switch(source_request){
    case 5620: case 5621: case 5625:
        if(world.move==W_BIG||(world.move>=W_RISE&&world.move<=W_LAND))return;
        request=wilds_swing_request((unsigned)real(a,FRAME));break;
    case 5675:
        request=world.move==W_SHEATHE_END?WILDS_SOUND_DRAW:
                world.move==W_SHEATHE?WILDS_SOUND_HANDLING:WILDS_SOUND_SHEATH;break;
    case 5616:
        if(world.move==W_BIG)return;
        request=world.move==W_SMALL?wilds_swing_request((unsigned)real(a,FRAME)):WILDS_SOUND_HANDLING;
        break;
    case 5622:
        request=world.move==W_SHEATHE?WILDS_SOUND_SHEATH:WILDS_SOUND_HANDLING;break;
    case 5605: case 5610: case 5623: case 5670:
        if(world.move==W_BIG||(world.move>=W_RISE&&world.move<=W_LAND))return;
        request=WILDS_SOUND_HANDLING;break;
    default:
        /* Keep footsteps on GU's ground and material timeline. */
        request=WILDS_AUDIO_REQUEST_COUNT;break;
    }
    if(request<WILDS_AUDIO_REQUEST_COUNT){
        world_audio_play(a,request,0);
        log_event("wilds_audio_cue",(u32)(uptr)a,world.source,request,(u32)real(a,FRAME));
        return;
    }
#endif
    for(unsigned i=0;i<WORLD_AUDIO_WEAPON_GROUP_COUNT;++i){
        const WAudioWeaponGroup *g=world_audio_weapon_groups+i;if(g->source_request!=source_request)continue;
        if(!world_audio_rng)world_audio_rng=(u32)(uptr)a^sample_sequence^0x9e3779b9u;
        unsigned r=world_audio_rng;r^=r<<13;r^=r>>17;r^=r<<5;world_audio_rng=r;
        unsigned last=world_audio_last_choice[i],count=g->count;
        unsigned choice=r%(last&&count>1?count-1:count);
        if(last&&count>1&&choice>=last-1)++choice;
        world_audio_last_choice[i]=choice+1;
        unsigned request=world_audio_weapon_choices[g->offset+choice];
        float point[3]={real(a,0x40),real(a,0x44),real(a,0x48)};
        world_audio_weapon_point(a,request,point);
        world_audio_schedule(a,request,g->delay,point);
        log_event("world_weapon_audio_choice",(u32)(uptr)a,source_request,request,i);
    }
}
#endif
static void world_audio_epv(void *a,unsigned request,const float *origin)
{
    if(!world_audio_ready)return;
#if !ML_WILDS_MOVES
    unsigned yaw=word(a,HEADING);
    void *camera=(void *)(uptr)world_camera_object;
    if(camera&&word(camera,0x85c)==(u32)(uptr)a){
        float x=origin[0]-real(camera,0x40),z=origin[2]-real(camera,0x48);
        if(x*x+z*z>.00001f)yaw=world_direction(x,z);
    }
    float sn=world_sin(yaw),co=world_sin(yaw+16384);
#endif
    log_event("world_audio_cue",(u32)(uptr)a,request,(u32)(real(a,FRAME)*1000),(u32)(world_audio_clock*1000000));
#if ML_WILDS_MOVES
    unsigned native;
    switch(request){
    case 25510:
        native=world.move==W_BIG?WILDS_SOUND_IAI_COUNTER:WILDS_SOUND_FORESIGHT_COUNTER;
        break;
    case 25505: case 25525: native=WILDS_SOUND_SPIRIT;break;
    case 25515: return;
    case 25520: case 25530: native=WILDS_SOUND_HELM;break;
    case 25604: native=WILDS_SOUND_EQUIPMENT;break;
    case 25601: case 25602: case 25603:
        if(world.move==W_BIG)return;
        native=WILDS_SOUND_COUNTER;break;
    case 25605: case 25606: native=WILDS_SOUND_COUNTER;break;
    default: return;
    }
    world_audio_play(a,native,request==25510?0:origin);
    log_event("wilds_audio_cue",(u32)(uptr)a,world.source,native,(u32)real(a,FRAME));
#else
    for(unsigned n=0;n<sizeof(world_audio_stems)/sizeof(*world_audio_stems);++n){
        const WAudioStem *stem=world_audio_stems+n;if(stem->source_request!=request)continue;
        float p[3]={origin[0]+co*stem->x+sn*stem->z,origin[1]+stem->y,origin[2]-sn*stem->x+co*stem->z};
        world_audio_schedule(a,stem->request,stem->delay,p);
    }
#endif
}
#if WORLD_AUDIO_GROUND
static void world_audio_blade(void *a,const float *point)
{
#if ML_WILDS_MOVES
    if(!world_audio_impact(world_attack_id,W_SOUND_HELM_BLADE,0,point))world_audio_play(a,WORLD_AUDIO_HELM_WAVE,point);
#else
    world_audio_play(a,WORLD_AUDIO_HELM_WAVE,point);
#endif
}
#if !ML_WILDS_MOVES
static void world_audio_tail(void *a,float delay,const float *point)
{
    /* Preserve EPV spatial offsets at submission, including the two voices. */
    for(unsigned i=0;i<sizeof(world_audio_jobs)/sizeof(*world_audio_jobs);++i)if(!world_audio_jobs[i].active){
        world_audio_jobs[i]=(WAudioJob){.active=1,.request=25530,.actor=(u32)(uptr)a,.epoch=byte(a,AREA),
                                       .remaining=delay,.point={point[0],point[1],point[2]}};return;
    }
    log_event("world_audio_queue_full",(u32)(uptr)a,25530,0,0);
}
#endif
#else
static void world_audio_blade(void *a,const float *point){(void)a;(void)point;}
#if !ML_WILDS_MOVES
static void world_audio_tail(void *a,float d,const float *p){(void)a;(void)d;(void)p;}
#endif
#endif
static void world_audio_load(void *a)
{
    if(world_audio_failed||world_audio_ready)return;
    if(!world_audio_archive){
        void *manager=*(void **)(main_base+0x2126a64);if(!manager)return;
        uptr *vt=*(uptr **)manager;
        world_audio_archive=((void *(*)(void *,void *,const char *,unsigned))vt[0x30/4])
            (manager,(void *)(main_base+0x2124df0),"arc/forge/world_audio",1);
        if(!world_audio_archive){world_audio_failed=1;return;}
        log_event("world_audio_queued",(u32)(uptr)a,(u32)(uptr)world_audio_archive,(u32)(uptr)&world_audio_probe_command,0);
    }
    if(!world_audio_ready){
        unsigned flags=byte(world_audio_archive,0x50);
        if(flags&0x50){world_audio_failed=1;log_event("world_audio_failed",(u32)(uptr)a,flags,0,0);return;}
        if(!(flags&1))return;
        unsigned n=word(world_audio_archive,0x68);void **objects=*(void ***)((uptr)world_audio_archive+0x64);
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
        if(n!=GU_ROUND_RESOURCE_COUNT||!objects)return;
#else
        if(n!=WILDS_AUDIO_RESOURCE_COUNT||!objects)return;
#endif
        for(unsigned i=0;i<n;++i){
            if(!objects[i])return;
            unsigned child=byte(objects[i],0x50);
            if(child&0x50){world_audio_failed=1;log_event("world_audio_failed",(u32)(uptr)a,child,i,0);return;}
            if(!(child&1))return;
            if(*(uptr *)objects[i]==main_base+0x1794210&&world_resource_path((const char *)objects[i]+8,"sound/se/pl/wp/swo/world/snd_world"))
                world_audio_request=objects[i];
        }
        if(!world_audio_request)return;
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
        gu_round_resources=objects;
#endif
        world_audio_ready=1;
        log_event("world_audio_ready",(u32)(uptr)a,(u32)(uptr)world_audio_request,word(world_audio_request,0x68),0);
    }
}
static void world_audio_tick(void *a)
{
    if(world_audio_failed)return;
#if WORLD_AUDIO_WEAPON
    world_audio_live_actor=(u32)(uptr)a;world_audio_live_epoch=byte(a,AREA);
#endif
    world_audio_load(a);
    if(!world_audio_ready)return;
    unsigned cmd=world_audio_probe_command;
#if ML_WORLD_AUDIO_PROBE
    world_audio_probe_command=0;
#endif
    if(cmd>=1&&cmd<=WILDS_AUDIO_REQUEST_COUNT)world_audio_play(a,cmd-1,(const float *)((uptr)a+0x40));
    if(world_audio_sample!=sample_sequence){
        world_audio_sample=sample_sequence;float dt=real(a,0x1c)/60.0f;
#if ML_WILDS_MOVES && WORLD_AUDIO_WEAPON
        wilds_sample_wind_speed(a,real(a,0x1c));
#endif
        if(dt>0&&dt<.5f)world_audio_clock+=dt;
        for(unsigned i=0;i<16;++i){
            WAudioJob *j=world_audio_jobs+i;if(!j->active)continue;
            if(j->actor!=(u32)(uptr)a||j->epoch!=byte(a,AREA)||(int16_t)u16(a,HP)<=0){j->active=0;continue;}
            if(dt>0&&dt<.5f)j->remaining-=dt;
            /* A relative countdown avoids a growing clock's float ULP
             * rounding a source0.4s delay up by a whole simulation frame. */
            if(j->remaining<=.000001f){
                if(j->request==25530)world_audio_epv(a,j->request,j->point);
                else world_audio_play(a,j->request,j->local?0:j->point);
                j->active=0;
            }
        }
    }
    if((world.move==W_GROUND && world.source!=363)||!world_bound||!world.move||world_audio_actor!=(u32)(uptr)a||world_audio_epoch!=byte(a,AREA)||
       (int16_t)u16(a,HP)<=0||u16(a,MOTION)!=0x500+world.slot)return;
#if ML_WILDS_MOVES
    if(world.source==917||world.source==109)return;
    /* Wilds motions 233, 235, 237 and 252 mapped to the GU clip phases.
     * A short vault can go directly from air to landing. Play the swing
     * at that transition if the descent clip was bypassed. */
    static const WildsAudioCue follow_cues[]={
        {170,10,WILDS_SOUND_THRUST_CHARGE},{170,83,WILDS_SOUND_THRUST_CUT},
        {172,1,WILDS_SOUND_HELM_JUMP},
        {173,0,WILDS_SOUND_HELM_CUT},
        {174,0,WILDS_SOUND_HELM_CUT},
        {174,1,WILDS_SOUND_HELM_LAND_GEAR},{174,6,WILDS_SOUND_HELM},
        {308,16,WILDS_SOUND_IAI_CUT},{308,17,WILDS_SOUND_IAI_WHOOSH},{308,129,WILDS_SOUND_IAI_RECOVERY},
    };
    unsigned source=world.move==W_BIG?308:world.source;
    for(unsigned i=0;i<sizeof(follow_cues)/sizeof(*follow_cues);++i){
        const WildsAudioCue *cue=follow_cues+i;
        /* These cues use a separate bitset from the original combat table. */
        unsigned mask=1u<<i;
        if(cue->source!=source||(wilds_follow_audio_seen&mask)||real(a,FRAME)<cue->frame)continue;
        wilds_follow_audio_seen|=mask;
        if(cue->request==WILDS_SOUND_HELM_CUT){
            if(world_helm_wind_played)continue;
            world_helm_wind_played=1;
        }else if(cue->frame<world_entry_start&&cue->request!=WILDS_SOUND_HELM_LAND_GEAR)continue;
        world_audio_play(a,cue->request,0);
        log_event("wilds_audio_cue",(u32)(uptr)a,world.source,cue->request,(u32)real(a,FRAME));
    }
    if(world.source>=170&&world.source<=174)return;
    if((world.source>=901&&world.source<=918)||(world.source>=W_CRIMSON_STATIONARY&&world.source<=W_CRIMSON_SPIN)){
        for(unsigned i=0;i<sizeof(wilds_audio_cues)/sizeof(*wilds_audio_cues);++i){
            const WildsAudioCue *cue=wilds_audio_cues+i;uint64_t bit=(uint64_t)1<<(i%64);
            if(cue->source!=world.source||(wilds_audio_seen[i/64]&bit))continue;
            float frame=cue->frame;
            if(real(a,FRAME)<frame)continue;
            wilds_audio_seen[i/64]|=bit;
            if(frame<world_entry_start)continue;
            /* The native local-player route keeps personal sword effects
             * audible as the camera moves and the blade leaves the view. */
            world_audio_play(a,cue->request,0);
            log_event("wilds_audio_cue",(u32)(uptr)a,world.source,cue->request,(u32)real(a,FRAME));
        }
        return;
    }
#endif
    if(world_audio_range_source!=world.source){world_audio_range_source=world.source;world_audio_active_range=world_audio_cue_range(world.source);}
    if(!world_audio_active_range)return;
    for(unsigned n=0;n<world_audio_active_range->count;++n){
        unsigned i=world_audio_cue_indices[world_audio_active_range->first+n];
        const WAudioCue *cue=world_audio_cues+i;
        unsigned mask=1u<<(i%32),bucket=i/32;
        if((world_audio_seen[bucket]&mask)||real(a,FRAME)<cue->frame)continue;
        world_audio_seen[bucket]|=mask;
        if(cue->frame<world_entry_start)continue;
        if((cue->work==2007&&!world.foresight_gauge)||((cue->request==25525||cue->work==2005)&&!world.rank))continue;
#if WORLD_AUDIO_WEAPON
        if(!cue->work){world_audio_weapon(a,cue->request);continue;}
#endif
        float origin[3];for(unsigned k=0;k<3;++k)origin[k]=real(a,0x40+4*k);
        /* Source work3014 uses helper47 composed with its parent function12.
         * Other Iai work records use the effect's anchored actor position. */
        if(cue->work==3014)world_joint47(a,origin);
        world_audio_epv(a,cue->request,origin);
    }
}
#endif
