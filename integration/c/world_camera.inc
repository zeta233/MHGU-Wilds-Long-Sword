/* Source Iai trigger31 requests an absolute camera turn twice (20/37).
 * World camera150/340 telemetry proves positive pitch raises its eye, while
 * GU camera9d8 lowers it; target pitch therefore maps to -3degrees here.
 * GU yaw describes look->eye rather than eye->look, hence the half turn.
 * Source nearest-main-bone selection currently uses GU posed bones: a rig
 * adaptation, pending per-monster semantic main-bone mapping.
 */
static void (*world_original_camera)(void *);
static unsigned world_camera_actor,world_camera_epoch,world_camera_seen;
static unsigned world_camera_sample,world_camera_active,world_camera_object;
static unsigned world_camera_origin[2];
static int world_camera_delta[2];
static float world_camera_age,world_camera_duration;
#if ML_WILDS_MOVES
enum { WORLD_FOCUS_CAMERA_TAIL_SAMPLES=36 };
static unsigned world_camera_iai_sample,world_camera_focus;
static struct {
    void *actor;
    unsigned area,sample,round,focus;
} world_camera_foresight;
static unsigned world_camera_iai_recent(void *a)
{
    return world_camera_actor==(u32)(uptr)a&&world_camera_epoch==byte(a,AREA)&&
        world_camera_iai_sample&&sample_sequence-world_camera_iai_sample<=WORLD_FOCUS_CAMERA_TAIL_SAMPLES;
}
static unsigned world_camera_foresight_recent(void *a)
{
    return world_camera_foresight.actor==a&&world_camera_foresight.area==byte(a,AREA)&&
        sample_sequence-world_camera_foresight.sample<=WORLD_FOCUS_CAMERA_TAIL_SAMPLES;
}

static void world_camera_track_foresight(void *a,unsigned focus)
{
    unsigned area=byte(a,AREA);
    unsigned recent=world_camera_foresight_recent(a);
    if(world.actor==(u32)(uptr)a&&world.epoch==area&&u16(a,STATE)==16&&
       u16(a,ACTION)==w_action(world.move)&&u16(a,MOTION)==0x500+world.slot){
        if(world.move==W_FORESIGHT){
            if(!recent||world_camera_foresight.round)
                world_camera_foresight=(__typeof__(world_camera_foresight)){.actor=a,.area=area};
            world_camera_foresight.sample=sample_sequence;
        }else if((world.source==109||world.source==917)&&(recent||world.red_foresight_chain)){
            if(!recent)
                world_camera_foresight=(__typeof__(world_camera_foresight)){.actor=a,.area=area};
            world_camera_foresight.sample=sample_sequence;
            world_camera_foresight.round=1;
        }
    }
    if(focus&&world_camera_foresight_recent(a))world_camera_foresight.focus=1;
}
#endif
#if !ML_SWITCH_GUARD_DIAGNOSTICS
static u32 (*world_original_view)(void *,void *);
static void *world_player_view_camera;
static struct {
    void *camera,*actor;
    unsigned area,source;
    float distance_squared;
#if ML_WILDS_MOVES
    unsigned focus_sample,focus_held,focus_chain_sample;
    float focus_limit,rendered_distance;
#endif
#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE && ML_WORLD_PRESENTATION
    unsigned framing_sample,pursuit_target,musou_target;
    float pursuit_amount,pursuit_from,pursuit_frames;
    float musou_framing[2];
#endif
} world_view_baseline;

#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE && ML_WORLD_PRESENTATION
enum { W_RAD_VIEW_DRAW, W_RAD_VIEW_CONTACT, W_RAD_VIEW_FAN, W_RAD_VIEW_CUES };
static struct {
    void *actor;
    unsigned area,active;
    float born[W_RAD_VIEW_CUES];
} world_radiance_view;

static void world_radiance_view_cue(void *a,unsigned cue)
{
    unsigned area=byte(a,AREA);
    if(world_radiance_view.actor!=a||world_radiance_view.area!=area){
        world_radiance_view.actor=a;
        world_radiance_view.area=area;
        world_radiance_view.active=0;
    }
    world_radiance_view.born[cue]=world_elapsed;
    world_radiance_view.active|=1u<<cue;
}

static float world_view_bound(float x,float limit)
{
    return x>limit?limit:x< -limit?-limit:x;
}

static unsigned world_view_framing(void *a,const float delta[3],float radius,float headroom,
                                   float shift[3],float eye_shift[3])
{
    unsigned focus=(ml_pad_raw_bits()&PAD_FOCUS)!=0;
    // A dispatch can change the route one frame before the motion layer
    // binds its replacement clip. Track the owned action across that handoff.
    unsigned local=ascendance_owner(a)&&world.actor==(u32)(uptr)a&&
        u16(a,STATE)==16&&u16(a,ACTION)==w_action(world.move);
    unsigned owned=!focus&&local;
    unsigned bound=u16(a,MOTION)==0x500+world.slot;
    unsigned pursuit=owned&&((ascendance.radiance==RADIANCE_CHASE&&world.move==W_THRUST)||
        (ascendance.radiance==RADIANCE_FOLLOW&&world.move==W_SMALL&&
        (world.slot==PURSUIT_FIRST_SLOT&&(!bound||radiance_pursuit.rewarded||real(a,FRAME)<19.f)||
         world.slot==PURSUIT_SECOND_SLOT&&(!bound||real(a,FRAME)<PURSUIT_SECOND_HIT_END))));
    unsigned dash=owned&&((musou.phase==MUSOU_DASH&&world.move==W_BIG)||
        (musou.phase==MUSOU_CUT&&world.move==W_SMALL&&world.slot==40));
    float horizontal=att_sqrt(delta[0]*delta[0]+delta[2]*delta[2]);
    if(!(radius>0&&horizontal>0))return 0;
    if(world_view_baseline.framing_sample!=sample_sequence){
        world_view_baseline.framing_sample=sample_sequence;
        float frames=real(a,0x1c);
        if(!(frames>0))frames=0;
        if(frames>4)frames=4;
        if(pursuit!=world_view_baseline.pursuit_target){
            world_view_baseline.pursuit_target=pursuit;
            world_view_baseline.pursuit_from=world_view_baseline.pursuit_amount;
            world_view_baseline.pursuit_frames=0;
            log_event("world_view_pursuit",(u32)(uptr)a,pursuit,world.slot,(u32)radius);
        }
        float duration=pursuit?39.f:9.6f;
        if(world_view_baseline.pursuit_frames<duration)world_view_baseline.pursuit_frames+=frames;
        float t=world_view_baseline.pursuit_frames/duration;
        if(t>1)t=1;
        float blend=t*t*t*(10+t*(-15+6*t));
        float from=world_view_baseline.pursuit_from;
        world_view_baseline.pursuit_amount=from+(pursuit-from)*blend;

        // Frame part of the dash back toward its starting point, with enough
        // room for the hunter even when the camera is already against a wall.
        if(dash!=world_view_baseline.musou_target){
            world_view_baseline.musou_target=dash;
            log_event("world_view_musou",(u32)(uptr)a,dash,world.slot,(u32)radius);
        }
        float dx=dash?(musou.start_x-real(a,0x40))*.5f:0;
        float dz=dash?(musou.start_z-real(a,0x48))*.5f:0;
        float side=(dx*delta[2]-dz*delta[0])/horizontal;
        float forward=(dx*delta[0]+dz*delta[2])/horizontal;
        world_view_baseline.musou_framing[0]+=
            (world_view_bound(side,110.f)-world_view_baseline.musou_framing[0])*frames/(dash?4.f:12.f);
        world_view_baseline.musou_framing[1]+=
            (world_view_bound(forward,65.f)-world_view_baseline.musou_framing[1])*frames/(dash?4.f:12.f);
        if(!dash)for(unsigned i=0;i<2;++i)
            if(world_view_baseline.musou_framing[i]<.05f&&world_view_baseline.musou_framing[i]>-.05f)
                world_view_baseline.musou_framing[i]=0;
    }
    // The render eye can dip toward the hunter's height when the native
    // camera has space. Leave its collision state and orbit angles intact.
    float amount=world_view_baseline.pursuit_amount;
    float room=world_view_bound((radius-110.f)/160.f,1.f);
    amount*=room>0?room:0;
    float lower=world_view_bound(headroom-60.f,58.f);
    float drop=lower>0?lower*amount:0;
    float side=world_view_bound(world_view_baseline.musou_framing[0]-55.f*amount,radius*.13f);
    float forward=world_view_bound(world_view_baseline.musou_framing[1],radius*.08f);
    shift[0]=(side*delta[2]+forward*delta[0])/horizontal;
    shift[1]=8.f*amount;
    shift[2]=(forward*delta[2]-side*delta[0])/horizontal;

    float shake_x=0,shake_y=0,push=radius*.045f*amount;
    unsigned radiance=local&&(ascendance.radiance==RADIANCE_FLASH||
        ascendance.radiance==RADIANCE_CHASE||ascendance.radiance==RADIANCE_FOLLOW);
    if(radiance&&world_radiance_view.actor==a&&world_radiance_view.area==byte(a,AREA)){
        static const float duration[]={.30f,.16f,.34f};
        static const float travel[]={58.f,0.f,28.f};
        static const float shake[]={9.f,4.f,13.f};
        for(unsigned cue=0;cue<W_RAD_VIEW_CUES;++cue){
            if(!(world_radiance_view.active&(1u<<cue)))continue;
            float t=(world_elapsed-world_radiance_view.born[cue])/duration[cue];
            if(t<0||t>=1){
                world_radiance_view.active&=~(1u<<cue);
                continue;
            }
            float fade=1.f-t;
            float envelope=ascendance_ease(t/.13f)*fade*fade;
            unsigned phase=(unsigned)(65536.f*t*3.5f);
            push+=travel[cue]*envelope;
            shake_x+=shake[cue]*envelope*world_sin(phase);
            shake_y+=shake[cue]*envelope*.60f*world_sin(phase+16384u);
        }
        if(ascendance.radiance==RADIANCE_FOLLOW&&world.slot==PURSUIT_FIRST_SLOT&&
           radiance_pursuit.first_hit_hold>0){
            float elapsed=PURSUIT_FIRST_HIT_HOLD-radiance_pursuit.first_hit_hold;
            if(elapsed>=0&&elapsed<PURSUIT_FIRST_HIT_HOLD){
                unsigned phase=(unsigned)(elapsed*65536.f*.29f);
                float envelope=ascendance_ease(elapsed/3.f)*ascendance_ease(radiance_pursuit.first_hit_hold/4.f);
                shake_x+=2.6f*envelope*world_sin(phase);
                shake_y+=1.6f*envelope*world_sin(phase+16384u);
            }
        }
    }
    float clearance=room>0?room:0;
    float push_ratio=world_view_bound(push/radius,.12f)*clearance;
    if(delta[1]>0&&headroom-drop-delta[1]*push_ratio<55.f){
        float safe=(headroom-drop-55.f)/delta[1];
        push_ratio=safe>0?safe:0;
    }
    shake_x=world_view_bound(shake_x,14.f)*clearance;
    shake_y=world_view_bound(shake_y,8.f)*clearance;
    float x=shake_x*delta[2]/horizontal,z=-shake_x*delta[0]/horizontal;
    eye_shift[0]=x-delta[0]*push_ratio;
    eye_shift[1]=shake_y-drop-delta[1]*push_ratio;
    eye_shift[2]=z-delta[2]*push_ratio;
    shift[0]+=x;shift[1]+=shake_y;shift[2]+=z;
    return side!=0||forward!=0||amount!=0||push_ratio!=0||shake_x!=0||shake_y!=0;
}
#endif

static u32 world_view(void *out,void *camera)
{
    if(camera!=world_player_view_camera)return world_original_view(out,camera);
    void *a=(void *)(uptr)word(camera,0x85c);
    if(!local_supported(a)||(int16_t)u16(a,HP)<=0)return world_original_view(out,camera);
    unsigned area=byte(a,AREA);
    unsigned steady=world.actor==(u32)(uptr)a&&world.epoch==area&&u16(a,STATE)==16&&
        u16(a,ACTION)==w_action(world.move)&&
        (world.move==W_FORESIGHT||(world.move==W_SMALL&&world.slot==238));
    float eye[3],look[3],delta[3],distance=0;
    for(unsigned k=0;k<3;++k){
        eye[k]=real(camera,0x40+4*k);
        look[k]=real(camera,0x60+4*k);
        delta[k]=eye[k]-look[k];
        distance+=delta[k]*delta[k];
    }
    if(world_view_baseline.camera!=camera||world_view_baseline.actor!=a||world_view_baseline.area!=area)
        world_view_baseline=(__typeof__(world_view_baseline)){.camera=camera,.actor=a,.area=area};
    float limit=world_view_baseline.distance_squared;
    if(!steady){
        world_view_baseline.source=0;
        if(distance>0)world_view_baseline.distance_squared=distance;
        limit=distance;
    }
    if(!(limit>0))limit=distance;
#if ML_WILDS_MOVES
    if(distance>0){
        float radius=att_sqrt(distance);
        unsigned iai=world_camera_iai_recent(a);
        unsigned round=world_camera_foresight.round&&world_camera_foresight_recent(a);
        unsigned raw_focus=(ml_pad_raw_bits()&PAD_FOCUS)!=0;
        if(raw_focus){
            unsigned state=u16(a,STATE),action=u16(a,ACTION);
            unsigned owned=world.actor==(u32)(uptr)a&&world.epoch==area&&
                state==16&&action==w_action(world.move);
            if((owned&&(world.move==W_CHIJIN||world.move==W_FORESIGHT||world.move==W_SLIDE))||
               (!world.move&&w_native_spirit_one(state,action)))
                world_view_baseline.focus_chain_sample=sample_sequence;
        }
        unsigned chain=raw_focus&&world_view_baseline.focus_chain_sample&&
            sample_sequence-world_view_baseline.focus_chain_sample<=WORLD_FOCUS_CAMERA_TAIL_SAMPLES;
        unsigned focus=chain||(iai&&(world_camera_focus||raw_focus))||
            (round&&(world_camera_foresight.focus||raw_focus));
        unsigned fresh=world_view_baseline.focus_sample!=sample_sequence;
        if(fresh)world_view_baseline.focus_sample=sample_sequence;
        if(focus&&!world_view_baseline.focus_held){
            float previous=world_view_baseline.rendered_distance;
            world_view_baseline.focus_limit=previous>0?previous:radius;
        }
        world_view_baseline.focus_held=focus;
        float cap=world_view_baseline.focus_limit;
        if(cap>0){
            /* Native obstruction can move the eye closer. Suppress repeated
             * outward corrections during the handoff, then ease them back. */
            if(radius<cap)cap=radius;
            else if(!focus&&fresh){
                float frames=real(a,0x1c);
                if(frames>0)cap+=(radius-cap)*(frames<12?frames/12:1);
            }
            world_view_baseline.focus_limit=!focus&&radius-cap<.5f?0:cap;
            if(cap*cap<limit)limit=cap*cap;
            if(fresh)log_event(chain?"world_focus_chain_distance":round?"world_focus_round_distance":"world_focus_iai_distance",(u32)(uptr)a,focus,
                (u32)(radius*10),(u32)(att_sqrt(limit)*10));
        }
        world_view_baseline.rendered_distance=att_sqrt(limit<distance?limit:distance);
    }
#endif
    unsigned clamp_eye=limit>0&&distance>limit;
    float shift[3]={0,0,0};
    float eye_shift[3]={0,0,0};
    float ratio=clamp_eye?att_sqrt(limit/distance):1;
    unsigned frame_action=0;
#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE && ML_WORLD_PRESENTATION
    float headroom=look[1]+delta[1]*ratio-real(a,0x44);
    float ray[3]={delta[0]*ratio,delta[1]*ratio,delta[2]*ratio};
    frame_action=world_view_framing(a,ray,att_sqrt(clamp_eye?limit:distance),headroom,shift,eye_shift);
#endif
    if(!clamp_eye&&!frame_action)return world_original_view(out,camera);
    if(clamp_eye&&steady&&world_view_baseline.source!=world.source){
        world_view_baseline.source=world.source;
        log_event("world_steady_view",(u32)(uptr)a,world.source,(u32)att_sqrt(distance),(u32)att_sqrt(limit));
    }
    /* Only the render matrix uses these offsets. Keep the native camera's
     * obstacle and aim state for the next update. */
    for(unsigned k=0;k<3;++k){
        if(clamp_eye||eye_shift[k]!=0)set_real(camera,0x40+4*k,look[k]+delta[k]*ratio+eye_shift[k]);
        if(frame_action)set_real(camera,0x60+4*k,look[k]+shift[k]);
    }
    u32 result=world_original_view(out,camera);
    for(unsigned k=0;k<3;++k){
        if(clamp_eye||eye_shift[k]!=0)set_real(camera,0x40+4*k,eye[k]);
        if(frame_action)set_real(camera,0x60+4*k,look[k]);
    }
    return result;
}
#endif
#if ML_WILDS_MOVES
static unsigned focus_camera_actor,focus_camera_epoch,focus_camera_yaw;
static unsigned world_camera_heading(void *a,unsigned *heading)
{
    if(focus_camera_actor!=(u32)(uptr)a||
       focus_camera_epoch!=byte(a,AREA)||!local_supported(a)||(int16_t)u16(a,HP)<=0)return 0;
    *heading=focus_camera_yaw;return 1;
}
static struct {
    unsigned actor,epoch,sample,yaw,free_facing;
} focus_facing;
static unsigned world_focus_heading(void *a,unsigned *heading)
{
    unsigned target;
    if(!(ml_pad_raw_bits()&PAD_FOCUS)||!world_camera_heading(a,&target)){
        if(focus_facing.actor==(u32)(uptr)a)focus_facing.actor=0;
        return 0;
    }
    unsigned epoch=byte(a,AREA);
    /* An evade also has common state 1. Reacquire Focus from the
     * actual exit heading once idle or walking resumes. */
    unsigned free_facing=!world.move&&(byte(a,DRAWN)&1)&&u16(a,STATE)<=2&&u16(a,ACTION)<=5;
    if(focus_facing.actor!=(u32)(uptr)a||focus_facing.epoch!=epoch||
       (free_facing&&!focus_facing.free_facing)){
        focus_facing.actor=(u32)(uptr)a;focus_facing.epoch=epoch;
        focus_facing.yaw=u16(a,HEADING);focus_facing.sample=sample_sequence-1;
    }
    focus_facing.free_facing=free_facing;
    if(focus_facing.sample!=sample_sequence){
        focus_facing.sample=sample_sequence;
        float frames=real(a,0x1c);
        if(frames>0){
            // A long update may span several simulation frames. Bound its visible step.
            if(frames>2)frames=2;
            int limit=(int)((65536.f/360.f)*8.f*frames);
            int delta=ml_heading_delta(target,focus_facing.yaw);
            if(delta>limit)delta=limit;
            if(delta< -limit)delta= -limit;
            focus_facing.yaw=(focus_facing.yaw+delta)&65535u;
        }
    }
    *heading=focus_facing.yaw;return 1;
}
#endif

static unsigned world_direction(float x,float z)
{
    float ax=x<0?-x:x,az=z<0?-z:z;
    unsigned lo=0,hi=16384;
    while(hi-lo>1){
        unsigned mid=(lo+hi)/2;
        if(world_sin(mid)*az<world_sin(mid+16384)*ax)lo=mid;else hi=mid;
    }
    unsigned n=(lo+hi)/2;
    if(z<0)n=32768-n;
    return (x<0?65536-n:n)&65535;
}
#if ML_WILDS_MOVES
static void world_camera_reserve_focus(void *a)
{
    if(world_camera_seen!=3||world_camera_active)
        log_event("world_camera_focus_control",(u32)(uptr)a,world.source,world_camera_seen,world_camera_active);
    /* Consume both automatic turns for this Iai. Releasing LT must not
     * replay a key or resume a tween that Focus already canceled. */
    world_camera_seen=3;
    world_camera_active=0;
    world_camera_focus=1;
}
#endif
static void world_camera_begin(void *a)
{
    world_camera_actor=(u32)(uptr)a;world_camera_epoch=byte(a,AREA);
    world_camera_seen=world_camera_active=0;
#if ML_WILDS_MOVES
    world_camera_focus=0;
    world_camera_iai_sample=sample_sequence;
    if(ml_pad_raw_bits()&PAD_FOCUS)world_camera_reserve_focus(a);
#endif
}
static unsigned world_camera_target(void *a,unsigned *yaw)
{
    WFollow *job=world_follow+7;
    if(!job->target||job->area!=byte(a,AREA))return 0;
    void *target=world_monster(job->uid);
    if((u32)(uptr)target!=job->target||byte(target,AREA)!=job->area)return 0;
    const float *hit=job->bone<0?0:world_raw_bone(target,(unsigned)job->bone);
    if(!hit)return 0;
    int bone=world_nearest_bone(target,a);
    const float *near=bone<0?0:world_raw_bone(target,(unsigned)bone);
    float v[3],distance=0;
    for(unsigned k=0;k<3;++k){
        float p=near?near[12+k]:real(target,0x40+4*k);
        v[k]=(p+hit[12+k]+job->point[k])*.5f-real(a,0x40+4*k);
        distance+=v[k]*v[k];
    }
    if(!(distance>2500)||!(v[0]*v[0]+v[2]*v[2]>.000001f))return 0;
    unsigned direction=world_direction(v[0],v[2]);
    if(!ml_heading_delta(direction,word(a,HEADING)))return 0;
    *yaw=(direction+32768)&65535;return 1;
}
static void world_camera(void *camera)
{
    void *a=(void *)(uptr)word(camera,0x85c);
    unsigned local=local_supported(a),alive=local&&(int16_t)u16(a,HP)>0;
#if ML_WILDS_MOVES
    unsigned focus=alive?ml_pad_raw_bits()&PAD_FOCUS:0;
    if(alive)world_camera_track_foresight(a,focus);
    else if(world_camera_foresight.actor==a)world_camera_foresight.actor=0;
#endif
#if !ML_SWITCH_GUARD_DIAGNOSTICS
    if(local)world_player_view_camera=camera;
    else if(world_player_view_camera==camera)world_player_view_camera=0;
#endif
    unsigned valid=alive&&world_camera_actor==(u32)(uptr)a&&world_camera_epoch==byte(a,AREA);
    if(!valid)world_camera_active=0;
    else {
        if(world_camera_object!=(u32)(uptr)camera){
            world_camera_object=(u32)(uptr)camera;world_camera_active=0;
            log_event("world_camera_owner",(u32)(uptr)a,(u32)(uptr)camera,word(camera,0x9e0),word(camera,0x9e4));
        }
#if ML_WILDS_MOVES
        if(world.move==W_BIG&&world.source>=308&&world.source<=311&&
           u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_BIG))
            world_camera_iai_sample=sample_sequence;
        if((world_camera_iai_recent(a)||world_camera_active)&&focus)
            world_camera_reserve_focus(a);
#endif
        unsigned fresh=world_camera_sample!=sample_sequence;float step=0;
        if(fresh){
            world_camera_sample=sample_sequence;
            float dt=real(a,0x1c)/60;
            if(dt>0&&dt<.5f){
                step=dt;
                if(world.move==W_BIG&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_BIG)&&
                   u16(a,MOTION)==0x500+world.slot){
                    float frame=real(a,FRAME);
                    for(unsigned i=0;i<2;++i)if(!(world_camera_seen&(1u<<i))&&frame>=(i?37:20)){
                        world_camera_seen|=1u<<i;
                        unsigned yaw;
                        if(world_camera_target(a,&yaw)){
                            world_camera_origin[0]=word(camera,0x9e0)&65535;
                            world_camera_origin[1]=word(camera,0x9e4)&65535;
                            world_camera_delta[0]=ml_heading_delta((65536-546),world_camera_origin[0]);
                            world_camera_delta[1]=ml_heading_delta(yaw,world_camera_origin[1]);
                            /* Source TIML float keys0:.7 and37:.4, linear.
                             * Sample at the consumed frame, not at nominal20. */
                            world_camera_duration=w_float(world.source,W_FLOAT_CAMERA,frame);
                            world_camera_age=0;world_camera_active=1;
                            log_event("world_camera_request",(u32)(uptr)a,i,yaw,(u32)(world_camera_duration*1000000));
                        }
                    }
                }
            }
        }
        if(world_camera_active){
            float t=world_camera_age/world_camera_duration;if(t>1)t=1;
            float q=1-t;q*=q;float ease=1-q*q;
            for(unsigned k=0;k<2;++k){
                unsigned angle=(world_camera_origin[k]+(int)(world_camera_delta[k]*ease))&65535;
                put(camera,0x9e8+4*k,angle);put(camera,0x9e0+4*k,angle);put(camera,0x9d8+4*k,angle);
            }
            if(t>=1){world_camera_active=0;log_event("world_camera_done",(u32)(uptr)a,word(camera,0x9e0),word(camera,0x9e4),0);}
            /* Original samples the current clock, then advances it. The
             * read-only training trace agrees with a one-frame clock lag. */
            world_camera_age+=step;
        }
    }
    /* Keep GU's floor/obstruction constraints and eye-position calculation. */
    world_original_camera(camera);
#if ML_WILDS_MOVES
    if(local){
        focus_camera_actor=(u32)(uptr)a;focus_camera_epoch=byte(a,AREA);
        focus_camera_yaw=(word(camera,0x9dc)+32768u)&65535u;
#if !ML_SWITCH_NO_OVERLAY
        if((int16_t)u16(a,HP)>0){
            world_wound_markers(a,camera);
#if ML_ASCENDANCE_PROTOTYPE && ML_WORLD_PRESENTATION
            world_radiance_overlay(a,camera);
#endif
        }
#endif
    }
#endif
}
