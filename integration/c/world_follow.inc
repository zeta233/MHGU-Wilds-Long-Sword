/* Delayed contacts retain a monster-local bone point, not a stale world point.
 * GU's native Sakura shell owns allocation, damage dispatch and destruction.
 * World source timing: first helm0.4s, interval0.05s, seven; big Iai0.5s.
 */
typedef struct {
    unsigned live,target,uid,area,tier,kind,emitted,power;
    int bone;float point[3],remaining,age;
    const void *part_table;void *part;
} WFollow;
static WFollow world_follow[56];
#if ML_WILDS_MOVES
static struct {unsigned target,mask;WFollow contact[3];} release_marks[4];
#if ML_ASCENDANCE_PROTOTYPE
static struct {
    WFollow mark;
    const void *status;
    unsigned id,damage,serial;
    float due;
} release_extra[8];
static unsigned release_serial;
static void world_release_credit(void *target,const void *status,unsigned damage,unsigned mv);
#endif
static unsigned crimson_final_strike;
static void world_release_contact(void *target,void *part,const float *contact);
static const void *world_wound_part_table(void *);
#if ML_WILDS_MOVES
static void world_add_valor_meter(void *a,unsigned amount)
{
    unsigned before=word(a,0x2790);
    if(before>=1000)return;
    unsigned after=before+amount;
    /* Fixed rewards bypass native hit multipliers. Use GU's full-meter
     * transition to enter blue and leave an existing blue timer in place. */
    if(after>=1000){
        if(valor_active(a))put(a,0x2790,1000);
        else {
            put(a,0x2790,999);
            ((void (*)(void *,int))(main_base+0x2a33a8))(a,1);
        }
    }else put(a,0x2790,after);
    set_real(a,0x27dc,60.f);
}
#endif

#endif
static unsigned world_helm_tier;
static unsigned world_helm_count,world_helm_hit_sample;
static unsigned world_helm_emitted,world_helm_line_hit,world_helm_valor_paid;
#if ML_WORLD_PRESENTATION
static float world_helm_landed,world_helm_sound_point[3];
static float world_helm_blade[16];
#include "world_helm_stroke.h"
#endif
typedef struct {unsigned target;float next;} WPrimaryCooldown;
static WPrimaryCooldown world_primary_cooldown[10];
static u32 (*world_original_monster_query)(void *,void *,void *);
typedef void (*WMonsterDamage)(void *,void *,void *,void *,void *,void *,void *);
static void (*world_original_monster_damage)(void *,void *,void *,void *,void *,void *,void *,void *);
#if ML_WILDS_MOVES
static void world_monster_wound_damage(void *,void *,void *,void *,void *,void *,void *,void *);
#endif
static unsigned world_primary_helm(void *attack)
{
    return world_attack_owned&&(uptr)attack==(uptr)world_attack_actor+0x3020&&
        u16(attack,0x34)==world_attack_id&&(world.move==W_DOWN||world.move==W_LAND);
}
typedef struct {unsigned shell,request,helm_impact,kind,power;float until,point[3];
    unsigned attached,target,uid,ordinal;int bone;float local[3];const void *part_table;void *part;} WShellPosition;
static WShellPosition world_shell_positions[64];
static const WShellPosition *world_find_shell(void *attack)
{
    for(unsigned i=0;i<64;++i){
        const WShellPosition *p=world_shell_positions+i;
        if(p->kind&&p->shell+0x13a8==(u32)(uptr)attack&&world_elapsed<=p->until&&
           p->request==word((void *)(uptr)p->shell,0x136c))
            return p;
    }
    return 0;
}
static const WShellPosition *world_release_shell(void *attack)
{
    const WShellPosition *p=world_find_shell(attack);
    return p&&p->kind==4?p:0;
}
#if ML_ASCENDANCE_PROTOTYPE
static float world_delayed_author_scale(void *collider)
{
    const WShellPosition *shell=world_find_shell(collider);
    return shell&&shell->power?shell->power*.01f:1.f;
}
#endif

static void world_monster_damage(void *manager,void *attack,void *target,void *point,
                                 void *v1,void *v2,void *part,void *shape)
{
#if ML_ASCENDANCE_PROTOTYPE
    if(target&&target==radiance_pursuit.target&&!radiance_pursuit.confirmed&&
       !ascendance_flash_attack(attack)&&part&&byte(part,8)==radiance_pursuit.region)
        radiance_pursuit.target=0;
#endif
#if ML_WILDS_MOVES
    const WShellPosition *release_shell=world_release_shell(attack);
    if(release_shell){
        if((u32)(uptr)target!=release_shell->target||world_wound_part_table(target)!=release_shell->part_table)return;
        part=release_shell->part;
        log_event("wilds_release_part",release_shell->ordinal,byte(part,8),release_shell->target,(u32)(uptr)part);
        world_original_monster_damage(manager,attack,target,point,v1,v2,part,shape);
        return;
    }
#endif
    /* GU171350's native type42 path records the real intersection, hurtpart
     * and target, but skips damage, hitstop, sharpness and gauge callbacks.
     * World COL82 is a zero-damage sampler; only its delayed shells hurt.
     * Change the type after native collision filtering, then restore it before
     * returning to that pass. All other attacks retain their native path. */
#if ML_WILDS_MOVES
    if(world.move>=W_CHIJIN&&world_attack_owned&&(uptr)attack==(uptr)world_attack_actor+0x3020&&u16(attack,0x34)==world_attack_id){
        if(world.move==W_RELEASE&&target&&point&&part)world_release_contact(target,part,point);
        log_event("wilds_damage_begin",world.actor,world.source,(u32)(uptr)target,(unsigned)world_hit);
        world_monster_wound_damage(manager,attack,target,point,v1,v2,part,shape);
        log_event("wilds_damage_dispatch",world.actor,world.source,(u32)(uptr)target,(unsigned)world_hit);
        return;
    }
#endif
    if(!world_primary_helm(attack)){
#if ML_WILDS_MOVES
        void *actor=attack?(void *)(uptr)word(attack,0xc):0;
        if(actor&&attack==(void *)((uptr)actor+0x3020)&&local_supported(actor)&&u16(actor,STATE)==16){
            unsigned action=u16(actor,ACTION);
            if((action>=16&&action<=17)||(action>=23&&action<=24)||(action>=26&&action<=27)||
               (world_attack_owned&&world.source==109&&world_attack_actor==(u32)(uptr)actor&&
                world_attack_id==u16(attack,0x34))){
                log_event("wilds_spirit_damage_begin",(u32)(uptr)actor,action,u16(attack,0x38),u16(attack,0x34));
                world_monster_wound_damage(manager,attack,target,point,v1,v2,part,shape);
                log_event("wilds_spirit_damage_end",(u32)(uptr)actor,action,u16(attack,0x38),u16(attack,0x34));
                return;
            }
        }
#endif
#if ML_WILDS_MOVES
        world_monster_wound_damage(manager,attack,target,point,v1,v2,part,shape);
#else
        world_original_monster_damage(manager,attack,target,point,v1,v2,part,shape);
#endif
        return;
    }
    unsigned type=byte(attack,0x3a);
    *(unsigned char *)((uptr)attack+0x3a)=0x42;
    ((WMonsterDamage)(main_base+0x171350))(manager,attack,target,point,v1,v2,part);
    *(unsigned char *)((uptr)attack+0x3a)=(unsigned char)type;
}
static void (*world_original_shell_position)(void *);
static void *world_monster(unsigned uid);
static const float *world_bone(void *a,int bone);
#if ML_WILDS_MOVES && ML_WORLD_PRESENTATION
/* 1 forces the round bank for this contact; 2 excludes a delayed shell. */
static unsigned world_contact_audio_scope;
static void (*world_original_impact_sound)(void *,void *,void *,const float *);
static void world_impact_sound(void *manager,void *target,void *attack,const float *point)
{
    unsigned blade=world_attack_owned&&!world_primary_helm(attack)&&
        (uptr)attack==(uptr)world_attack_actor+0x3020&&u16(attack,0x34)==world_attack_id;
    if(blade)
        log_event("wilds_blade_hitstop",world.actor,world.source,byte(attack,0x57),byte(attack,6));
    const WShellPosition *delayed=blade?0:world_find_shell(attack);
    unsigned follow=delayed?delayed->kind:0;
    unsigned shell=follow==4;
    /* Use the contact reported by GU, including delayed release hits which
     * may land after the hunter has already started another move. */
    if(point&&world.actor&&(shell||(blade&&wilds_clip(world.source))))
        world_sfx_emit((void *)(uptr)world.actor,point,0x3fc0);
    if(point&&blade&&world.source==904){
        unsigned uid=world_hit==160?0x3fd5:world_hit==159?0x3fd4:0x3fd3;
        world_sfx_emit((void *)(uptr)world.actor,point,uid);
    }
    if(point&&blade&&world.move==W_CHIJIN&&crimson_final_strike)
        world_sfx_emit((void *)(uptr)world.actor,point,0x3f60);
    enum WAudioImpact impact=shell?W_SOUND_RELEASE_FOLLOW:follow==1?W_SOUND_HELM_FOLLOW:
        follow?W_SOUND_IAI_FOLLOW:blade&&world.move==W_CHIJIN&&crimson_final_strike?
        W_SOUND_CRIMSON_FINISH:world.move==W_RELEASE?W_SOUND_RELEASE_BLADE:
        world.move==W_THRUST?W_SOUND_THRUST_BLADE:W_SOUND_BLADE;
    if(!blade&&!delayed){
        world_original_impact_sound(manager,target,attack,point);
        return;
    }
#if ML_ASCENDANCE_PROTOTYPE
    if(blade){
        if(ascendance_independent_flash()||world.source==W_CRIMSON_SPIN||
           (radiance_pursuit.independent&&world.slot==PURSUIT_SECOND_SLOT)||
           musou.phase==MUSOU_DASH||musou.phase==MUSOU_CUT)
            impact=W_SOUND_LINGYUE_FINISH;
        else if(musou.phase==MUSOU_FLURRY)impact=W_SOUND_LINGYUE_RAPID;
        else if(ascendance.double_cut||dividing_slot(world.slot)||
                (radiance_pursuit.independent&&world.slot==PURSUIT_FIRST_SLOT))
            impact=W_SOUND_LINGYUE_CUT;
    }
#endif
    unsigned wilds_only=shell||follow==1||(blade&&(world.move==W_THRUST||world.move==W_RELEASE));
    if(wilds_only&&world_audio_impact(u16(attack,0x34),impact,delayed?delayed->ordinal:0,point)){
        log_event("wilds_impact_sound",world.actor,shell?1040:follow?1041+follow:world.source,
                  (u32)(uptr)target,u16(attack,0x34));
        return;
    }
    /* Retain the native contact handler for the other weapon routes. */
    unsigned char *collider=(unsigned char *)attack;
    unsigned sound=collider[0x55],effect=collider[0x52];
    unsigned round=blade&&(world.source==109||world.source==917);
    unsigned previous_scope=world_contact_audio_scope;
    world_contact_audio_scope=round?1:2;
    if(round){collider[0x55]=0;collider[0x52]=0x2e;}
    if(sound==0xfe)collider[0x55]=0;
    if(!effect)collider[0x52]=0x2e;
    world_original_impact_sound(manager,target,attack,point);
    log_event("wilds_gu_contact_audio",world.actor,u16(attack,0x34),collider[0x55],collider[0x52]);
    collider[0x52]=(unsigned char)effect;
    collider[0x55]=(unsigned char)sound;
    world_contact_audio_scope=previous_scope;
    if(!round)world_audio_impact(u16(attack,0x34),impact,delayed?delayed->ordinal:0,point);
    log_event("wilds_impact_sound",world.actor,shell?1040:follow?1041+follow:world.source,
              (u32)(uptr)target,u16(attack,0x34));
}
static unsigned world_install_impact_sound(void)
{
    const u32 *entry=(const u32 *)(main_base+0x18015c);
    if(entry[0]!=0xe92d48f0u||entry[1]!=0xe1a05002u){
        log_event("world_entry_refused",0x18015c,entry[0],entry[1],0xe92d48f0u);
        return 0;
    }
    forge_hook_create((void *)entry,(void *)world_impact_sound,(void **)&world_original_impact_sound);
    return world_original_impact_sound!=0;
}
#endif
static void world_shell_position(void *shell)
{
    for(unsigned i=0;i<64;++i){
        WShellPosition *p=world_shell_positions+i;
        if(p->shell==(u32)(uptr)shell&&p->request==word(shell,0x136c)&&world_elapsed<=p->until){
            if(p->attached){
                void *target=world_monster(p->uid);
                const float *m=(u32)(uptr)target==p->target?world_bone(target,p->bone):0;
                if(m)for(unsigned k=0;k<3;++k)
                    p->point[k]=m[12+k]+m[k]*p->local[0]+m[4+k]*p->local[1]+m[8+k]*p->local[2];
            }
            for(unsigned k=0;k<3;++k)set_real(shell,0x40+4*k,p->point[k]);
            return;
        }
    }
    world_original_shell_position(shell);
}
static void *world_monster(unsigned uid)
{
    void *manager=(void *)(uptr)*(u32 *)(main_base+0x1851fe0);
    return manager?((void *(*)(void *,u32))(main_base+0x5ceb4))(manager,uid):0;
}
static const float *world_bone(void *a,int bone)
{
    if(!a)return 0;
    uptr *vt=*(uptr **)a;
    return ((const float *(*)(void *,int))vt[(bone<0?0x54:0xd0)/4])(a,bone);
}
static const float *world_raw_bone(void *a,unsigned bone)
{
    unsigned count=word(a,0x490);uptr records=word(a,0x494);
    /* Native53aad8 resolves function IDs through498 into160-byte records494;
     * the posed matrix begins at record10.490 is the allocated record count. */
    return records&&count<=256&&bone<count?(const float *)(records+160*bone+0x10):0;
}
/* Source equipment helper47 is a child of left-hand function12, rather
 * than the hand origin itself. Samples use the same source local
 * coordinates as the real weapon attachment controller. */
#if ML_WORLD_FX_PROBE || ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
static unsigned world_joint47(void *a,float *out)
{
    unsigned motion=u16(a,MOTION);float frame=real(a,FRAME);
    const MLAttachmentClip *clip=0;
    for(unsigned i=0;i<sizeof(ml_attachment_clips)/sizeof(*clip);++i)
        if(motion==0x500+ml_attachment_clips[i].slot){clip=ml_attachment_clips+i;break;}
    if(!clip||!(frame>=0&&frame<10000))return 0;
    unsigned n=(unsigned)frame;if(n>=clip->frames)n=clip->frames-1;
    unsigned next=n+1<clip->frames?n+1:n;float f=frame-(float)n;if(f>1)f=1;
    const float *x=ml_attachment_samples[clip->offset+n]+7,*y=ml_attachment_samples[clip->offset+next]+7;
    const float *m=world_bone(a,12);if(!m)return 0;
    float p[3];for(unsigned k=0;k<3;++k)p[k]=x[k]+(y[k]-x[k])*f;
    for(unsigned k=0;k<3;++k)out[k]=m[12+k]+m[k]*p[0]+m[4+k]*p[1]+m[8+k]*p[2];
    return 1;
}
#endif
static int world_nearest_bone(void *target,const void *a)
{
    float nearest=1.0e30f;int bone=-1;
    unsigned count=word(target,0x490);if(count>256)return -1;
    for(unsigned i=0;i<count;++i){
        const float *m=world_raw_bone(target,i);if(!m)break;
        float d=0;for(unsigned k=0;k<3;++k){float v=m[12+k]-real(a,0x40+4*k);d+=v*v;}
        if(d<nearest){nearest=d;bone=(int)i;}
    }
    return bone;
}
static unsigned world_inverse(const float *m,const float *p,float *out)
{
    float x=p[0]-m[12],y=p[1]-m[13],z=p[2]-m[14];
    float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!(det>0.000001f||det<-.000001f))return 0;
    out[0]=((e*i-f*h)*x+(c*h-b*i)*y+(b*f-c*e)*z)/det;
    out[1]=((f*g-d*i)*x+(a*i-c*g)*y+(c*d-a*f)*z)/det;
    out[2]=((d*h-e*g)*x+(b*g-a*h)*y+(a*e-b*d)*z)/det;
    for(unsigned j=0;j<3;++j)if(!(out[j]>-1000000&&out[j]<1000000))return 0;
    return 1;
}
static void world_follow_clear(void)
{
    for(unsigned i=0;i<56;++i){world_follow[i].live=0;world_follow[i].remaining=0;}
#if ML_WILDS_MOVES
#if ML_ASCENDANCE_PROTOTYPE
    for(unsigned i=0;i<sizeof(release_extra)/sizeof(*release_extra);++i)release_extra[i].mark.target=0;
#endif
    for(unsigned i=0;i<4;++i)release_marks[i].target=0;
#endif
#if ML_WORLD_PRESENTATION
    for(unsigned i=0;i<64;++i)world_shell_positions[i].helm_impact=0;
#endif
    world_helm_tier=0;
    world_helm_count=world_helm_emitted=0;
    for(unsigned i=0;i<10;++i)world_primary_cooldown[i].target=0;
}
#if ML_WILDS_MOVES
static void world_release_begin(void)
{
    for(unsigned i=0;i<4;++i)release_marks[i].target=0;
#if ML_ASCENDANCE_PROTOTYPE
    for(unsigned i=0;i<sizeof(release_extra)/sizeof(*release_extra);++i)
        if(!release_extra[i].due)release_extra[i].mark.target=0;
    if(!++release_serial)++release_serial;
#endif
}
static void world_release_contact(void *target,void *part,const float *contact)
{
    if(!target||world_hit<158||world_hit>160)return;
    unsigned stroke=world_hit-158,slot=4;
    void *data=(void *)(uptr)word(target,0x1428);if(!data)return;
    unsigned uid=byte(data,0x1b5);
    if(world_monster(uid)!=target||byte(target,AREA)!=world.epoch)return;
    for(unsigned i=0;i<4;++i){
        if(release_marks[i].target==(u32)(uptr)target){slot=i;break;}
        if(slot==4&&!release_marks[i].target)slot=i;
    }
    if(slot==4)return;
    if(!release_marks[slot].target)release_marks[slot].mask=0;
    if(release_marks[slot].mask&(1u<<stroke))return;
    const void *table=world_wound_part_table(target);
    if(!table||(uptr)part<(uptr)table||((uptr)part-(uptr)table)%44)return;
    int bone=(int16_t)u16(part,0);if(bone==255)bone=-1;
    int other=(int16_t)u16(part,2);if(other==255)other=-1;
    const float *first=world_bone(target,bone),*second=world_bone(target,other);
    if(first&&second){
        float d1=0,d2=0;
        for(unsigned k=0;k<3;++k){float v=contact[k],x=first[12+k]-v,y=second[12+k]-v;d1+=x*x;d2+=y*y;}
        if(d2<d1)bone=other;
    }
    const float *m=world_bone(target,bone);float point[3];
    if(!m||!world_inverse(m,contact,point))return;
    release_marks[slot].target=(u32)(uptr)target;
    release_marks[slot].mask|=1u<<stroke;
    release_marks[slot].contact[stroke]=(WFollow){.live=1,.target=(u32)(uptr)target,
        .uid=uid,.area=world.epoch,.tier=3,.kind=4,.bone=bone,.point={point[0],point[1],point[2]},
        .part_table=table,.part=part};
    log_event("wilds_release_mark",stroke,byte(part,8),(u32)(uptr)target,(u32)(uptr)part);
}
#if ML_ASCENDANCE_PROTOTYPE
static void world_release_credit(void *target,const void *status,unsigned damage,unsigned mv)
{
    if(!mv||world_hit<158||world_hit>160)return;
    const WFollow *mark=0;
    for(unsigned i=0;i<4;++i)if(release_marks[i].target==(u32)(uptr)target&&
       (release_marks[i].mask&(1u<<(world_hit-158))))mark=&release_marks[i].contact[world_hit-158];
    if(!mark)return;
    unsigned slot=sizeof(release_extra)/sizeof(*release_extra);
    for(unsigned i=0;i<sizeof(release_extra)/sizeof(*release_extra);++i){
        if(release_extra[i].mark.target==mark->target&&release_extra[i].serial==release_serial){slot=i;break;}
        if(slot==sizeof(release_extra)/sizeof(*release_extra)&&!release_extra[i].mark.target)slot=i;
    }
    if(slot==sizeof(release_extra)/sizeof(*release_extra))return;
    /* Estimate the independent 65-MV explosion from the native hit on this
     * part, removing the Release-specific 170% multiplier. */
    unsigned denominator=mv*170,burst=(damage*6500+denominator/2)/denominator;
    if(!burst)burst=1;
    if(release_extra[slot].mark.target&&release_extra[slot].damage>=burst)return;
    release_extra[slot]=(__typeof__(release_extra[0])){
        .mark=*mark,.status=status,.id=u16(status,0x1b6),.damage=burst,.serial=release_serial};
    log_event("ascendance_release_credit",mark->target,damage,mv,burst);
}

static void world_release_extra_tick(void *a)
{
    for(unsigned i=0;i<sizeof(release_extra)/sizeof(*release_extra);++i){
        __typeof__(release_extra[0]) *burst=release_extra+i;
        const WFollow *mark=&burst->mark;
        if(!mark->target||!burst->due||world_elapsed<burst->due)continue;
        void *target=world_monster(mark->uid);
        unsigned valid=(u32)(uptr)target==mark->target&&mark->area==byte(a,AREA)&&byte(target,AREA)==mark->area&&
            word(target,0x1428)==(u32)(uptr)burst->status&&u16(burst->status,0x1b6)==burst->id&&
            (int)word(burst->status,0x370)>0&&world_wound_part_table(target)==mark->part_table;
        burst->mark.target=0;
        if(!valid)continue;
        unsigned region=byte(mark->part,8);
        int applied=((int (*)(void *,unsigned,int,unsigned))(main_base+0x9d6f8))(target,region,(int)burst->damage,0);
#if ML_WORLD_PRESENTATION
        const float *m=world_bone(target,mark->bone);
        if(applied>0&&m){
            float p[3];for(unsigned k=0;k<3;++k)p[k]=m[k]*mark->point[0]+m[k+4]*mark->point[1]+m[k+8]*mark->point[2]+m[k+12];
            world_helm_impact(a,p);
        }
#endif
        log_event("ascendance_release_extra",(u32)(uptr)target,burst->serial,burst->damage,(u32)applied);
    }
}
#endif
static void world_schedule_release(void *a)
{
    if(world.move==W_RELEASE&&real(a,FRAME)<94)return;
#if ML_ASCENDANCE_PROTOTYPE
    for(unsigned i=0;i<sizeof(release_extra)/sizeof(*release_extra);++i){
        if(!release_extra[i].mark.target||release_extra[i].due||release_extra[i].serial!=release_serial)continue;
        if(world.move==W_RELEASE)release_extra[i].due=world_elapsed+.001f+11*.035f+.10f+.12f;
        else release_extra[i].mark.target=0;
    }
#endif
    for(unsigned i=0;i<4;++i){
        if(!release_marks[i].target)continue;
        unsigned count=0,strokes[3];
        for(unsigned n=0;n<3;++n)if(release_marks[i].mask&(1u<<n))strokes[count++]=n;
        for(unsigned j=0;j<12;++j){
            WFollow *job=world_follow+8+12*i+j;
            *job=release_marks[i].contact[strokes[j*count/12]];
#if ML_ASCENDANCE_PROTOTYPE
            job->power=ascendance.release_power;
#endif
            job->age=world_elapsed;job->emitted=j;job->remaining=.001f+j*.035f;
        }
        release_marks[i].target=0;
    }
}
#endif
static void world_follow_capture(void *a)
{
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance_independent_flash())return;
#endif
    unsigned helm=world.move==W_DOWN||world.move==W_LAND;
    if((helm&&world_helm_count>=7)||(!helm&&world.move!=W_BIG)||!world_contact(a))return;
    unsigned counter=world.counter_success||world_counter_seen;
    unsigned tier=helm?world_helm_tier:world.rank;
    if((helm&&!tier)||tier>3)return;
#if ML_WILDS_MOVES
    /* Wilds grants its single delayed hit only on a successful counter
     * whose blade actually lands. The colorless counter earns the first tier. */
    if(!helm){
        if(!counter)return;
        if(!tier)tier=1;
    }
#endif
    void *target=(void *)(uptr)word(a,0x30d0);void *part=(void *)(uptr)word(a,0x30cc);
    void *data=(void *)(uptr)word(target,0x1428);unsigned uid=byte(data,0x1b5);
    if(world_monster(uid)!=target||byte(target,AREA)!=world.epoch)return;
    int bone=(int16_t)u16(part,0);if(bone==255)bone=-1;
    if(helm){
        int other=(int16_t)u16(part,2);if(other==255)other=-1;
        const float *m1=world_bone(target,bone),*m2=world_bone(target,other);
        if(m1&&m2){
            float d1=0,d2=0;
            for(unsigned k=0;k<3;++k){float p=real(a,0x3040+4*k),x=m1[12+k]-p,y=m2[12+k]-p;d1+=x*x;d2+=y*y;}
            if(d2<d1)bone=other;
        }
    }
    if(!helm)bone=world_nearest_bone(target,a);
    const float *matrix=helm?world_bone(target,bone):bone<0?0:world_raw_bone(target,(unsigned)bone);
    float point[3];if(!matrix)return;
    if(helm){if(!world_inverse(matrix,(const float *)((uptr)a+0x3040),point))return;}
    else for(unsigned k=0;k<3;++k)point[k]=real(a,0x3040+4*k)-matrix[12+k];
    /* Allow only one delayed Iai impact per slash. The first contact sets
     * Wilds' half-second interval, even if the blade crosses another part. */
    WFollow *job=world_follow+(helm?world_helm_count:7);
    /* Source2122682 rejects a still-positive slot.21226fc/212272a copies
     * the previous slot's remaining timer, which freezes once it crosses0.
     * An absolute deadline would continue ageing after emission and therefore
     * mis-time a later blade contact following fallback shells. */
    if(helm&&job->remaining>0)return;
#if ML_WILDS_MOVES
    if(!helm&&job->kind)return;
#endif
    float remaining=helm?(world_helm_count?world_follow[world_helm_count-1].remaining+.05f:.4f):.5f;
    *job=(WFollow){.live=1,.target=(u32)(uptr)target,.uid=uid,.area=world.epoch,.tier=tier,
        .kind=helm?1:counter?3:2,.bone=bone,
        .point={point[0],point[1],point[2]},.remaining=remaining,.age=world_elapsed,.emitted=helm?world_helm_count:0};
#if ML_ASCENDANCE_PROTOTYPE
    if(helm)job->power=ascendance.helm_power;
#endif
    if(helm){
#if ML_WORLD_PRESENTATION
        if(!world_helm_count){
            for(unsigned k=0;k<3;++k)world_helm_sound_point[k]=real(a,0x3040+4*k);
            world_audio_blade(a,world_helm_sound_point);
#if !ML_WILDS_MOVES
            if(world.move==W_LAND)world_audio_tail(a,1-(world_elapsed-world_helm_landed),world_helm_sound_point);
#endif
        }
#endif
        ++world_helm_count;
        for(unsigned i=0;i<10;++i)if(!world_primary_cooldown[i].target||world_primary_cooldown[i].target==(u32)(uptr)target){
            world_primary_cooldown[i]=(WPrimaryCooldown){(u32)(uptr)target,world_elapsed+1.5f/60};break;
        }
        log_event("world_helm_primary",(u32)(uptr)a,world_helm_count,(u32)(uptr)target,(u32)real(a,FRAME));
        log_event("world_helm_clock",(u32)(uptr)a,world_helm_count,(u32)(world_elapsed*1000),(u32)real(a,FRAME));
    }
    log_event("world_follow_capture",(u32)(uptr)a,job->kind,tier,uid);
}
#if ML_WILDS_MOVES
/* Raise the posed blade envelope without moving its lower edge.
 * Restore exact floats before GU visits the next target. */
static u32 wilds_high_blade_query(void *manager,void *attack,void *target,float lift)
{
    void *resource=(void *)(uptr)word(attack,0x84);
    void *group=resource?((void *(*)(void *,unsigned))(*(uptr **)resource)[0x44/4])
        (resource,word(attack,0x88)):0;
    unsigned count=group?word(group,4):0,first=word(attack,0x1c);
    if(first>=64||count>64-first)return world_original_monster_query(manager,attack,target);
    float saved[64][3];
    for(unsigned i=0;i<count;++i){
        void *v=(void *)((uptr)manager+0x2030+32*(first+i));
        saved[i][0]=real(v,4);saved[i][1]=real(v,12);saved[i][2]=real(v,24);
        unsigned type=byte(v,0);
        if(type<1||type>3)continue;
        set_real(v,4,saved[i][0]+lift);set_real(v,12,saved[i][1]+lift);
        if(type!=1)set_real(v,24,saved[i][2]+lift);
    }
    u32 result=world_original_monster_query(manager,attack,target);
    for(unsigned i=0;i<count;++i){
        void *v=(void *)((uptr)manager+0x2030+32*(first+i));
        set_real(v,4,saved[i][0]);set_real(v,12,saved[i][1]);set_real(v,24,saved[i][2]);
    }
    return result;
}
#endif
#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE
/* Small monsters apply collision damage during their next actor update.
 * Keep the pre-contact HP for a few pad samples, keyed by the live enemy. */
enum { SMALL_HIT_SLOTS=10, SMALL_HIT_SAMPLES=12 };
static struct {
    void *target;
    const void *status;
    unsigned id, hp, sample;
} small_hits[SMALL_HIT_SLOTS];

static void world_small_hits_tick(void *hunter)
{
    const void *registry=(const void *)(uptr)word((const void *)main_base,0x1851fe0);
    for(unsigned i=0;i<SMALL_HIT_SLOTS;++i){
        if(!small_hits[i].target)continue;
        void *target=small_hits[i].target;
        const void *status=small_hits[i].status;
        if(!registry||word(registry,28+4*i)!=(u32)(uptr)target||
           word(target,0x1428)!=(u32)(uptr)status||u16(status,0x1b6)!=small_hits[i].id||
           byte(target,AREA)!=byte(hunter,AREA)||sample_sequence-small_hits[i].sample>SMALL_HIT_SAMPLES||
           !ascendance_owner(hunter)){
            small_hits[i].target=0;
            continue;
        }
        unsigned hp=word(status,0x370);
        if(hp<small_hits[i].hp){
            unsigned damage=small_hits[i].hp-hp;
            small_hits[i].target=0;
            ascendance_hit(hunter,0,target,0,damage);
        }
    }
}
#endif
static u32 world_monster_query(void *manager,void *attack,void *target)
{
    unsigned own=world_attack_owned&&(uptr)attack==(uptr)world_attack_actor+0x3020&&u16(attack,0x34)==world_attack_id;
#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE
    void *hunter=(void *)(uptr)world.actor;
    const void *small_status=0;
    unsigned small_hp=0, small_slot=SMALL_HIT_SLOTS, small_id=0;
    unsigned local_blade=hunter&&attack==(void *)((uptr)hunter+0x3020)&&
        word(attack,0xc)==world.actor&&byte(attack,0x10)==0;
    if(hunter&&target&&ascendance_owner(hunter)&&byte(hunter,AREA)==byte(target,AREA)){
        const void *status=(const void *)(uptr)word(target,0x1428);
        unsigned id=status?u16(status,0x1b6):0;
        if(id>=10000&&id<65535){
            const void *registry=(const void *)(uptr)word((const void *)main_base,0x1851fe0);
            for(unsigned i=0;registry&&i<SMALL_HIT_SLOTS;++i){
                if(word(registry,28+4*i)!=(u32)(uptr)target)continue;
                small_status=status;
                small_hp=word(status,0x370);
                small_slot=i;
                small_id=id;
                break;
            }
        }
    }
    if(small_slot<SMALL_HIT_SLOTS&&small_hits[small_slot].target)
        world_small_hits_tick(hunter);
#endif
#if ML_WILDS_MOVES
    const WShellPosition *release=own?0:world_release_shell(attack);
    if(release&&release->target!=(u32)(uptr)target)return 0;
#endif
    unsigned helm=own&&(world.move==W_DOWN||world.move==W_LAND);
    if(helm){
        if(world_helm_count>=7)return 0;
        for(unsigned i=0;i<10;++i)if(world_primary_cooldown[i].target==(u32)(uptr)target&&world_elapsed<world_primary_cooldown[i].next)return 0;
    }
#if ML_WILDS_MOVES
    float lift=0;
    if(own){
        if(world.move==W_THRUST||world.move==W_SMALL)lift=10.f;
        if(world.move==W_RELEASE||world.move==W_CHIJIN||
           world.source==109||world.source==917||world.source==W_CRIMSON_SPIN)lift=48.f;
    }else{
        void *hunter=(void *)(uptr)word(attack,0xc);
        if(hunter&&attack==(void *)((uptr)hunter+0x3020)&&local_supported(hunter)&&
           u16(hunter,MOTION)==0x500+127)lift=48.f;
    }
    u32 result=lift?wilds_high_blade_query(manager,attack,target,lift):world_original_monster_query(manager,attack,target);
#else
    u32 result=world_original_monster_query(manager,attack,target);
#endif
#if ML_WORLD_PRESENTATION
    if(!own && byte(attack,0xb4)==1 && word(attack,0xb0)==(u32)(uptr)target){
        for(unsigned i=0;i<64;++i){
            WShellPosition *p=world_shell_positions+i;
            if(!p->helm_impact||p->shell+0x13a8!=(u32)(uptr)attack||p->until<world_elapsed)continue;
            void *shell=(void *)(uptr)p->shell;
            if(p->request!=word(shell,0x136c)||!world.actor)continue;
            if(p->helm_impact==1){
                if(!world_helm_line_hit){
                    world_helm_line_hit=1;
                    /* The red stroke starts at the first real damage contact. */
                    world_helm_line((void *)(uptr)world.actor,(const float *)((uptr)attack+0x20));
                }
            }else if(p->helm_impact==3){
                const float *contact=(const float *)((uptr)attack+0x20);
                if(p->ordinal==11){
                    world_sfx_emit((void *)(uptr)world.actor,contact,0x3fd5);
                    world_sfx_emit((void *)(uptr)world.actor,contact,0x3f60);
                }else if(p->ordinal%3==0)
                    world_sfx_emit((void *)(uptr)world.actor,contact,(p->ordinal&1)?0x3fd4:0x3fd3);
            }else world_helm_impact((void *)(uptr)world.actor,(const float *)((uptr)attack+0x20));
            p->helm_impact=0;
            log_event("world_helm_fx_contact",world.actor,p->shell,(u32)(uptr)target,p->request);
            break;
        }
    }
#endif
#if ML_WILDS_MOVES && ML_ASCENDANCE_PROTOTYPE
    if(small_status&&word(target,0x1428)==(u32)(uptr)small_status&&
       byte(attack,0xb4)==1&&word(attack,0xb0)==(u32)(uptr)target){
        if(local_blade&&(int)small_hp>0)
            small_hits[small_slot]=(__typeof__(small_hits[0])){target,small_status,small_id,small_hp,sample_sequence};
        else small_hits[small_slot].target=0;
    }
#endif
    /* Query returns once per actual target, unlike polling the last-contact
     * field next update. The native pass owns every intersection and callback. */
    if((helm||world.move==W_BIG)&&own&&byte(attack,0xb4)==1&&
       word(attack,0xb0)==(u32)(uptr)target){
        void *a=(void *)(uptr)world.actor;
#if ML_WILDS_MOVES
        if(helm&&!world_helm_valor_paid&&hunter_style(a)==5){
            world_helm_valor_paid=1;
            unsigned before=word(a,0x2790);
            world_add_valor_meter(a,W_VALOR_HELM_GAIN);
            log_event("valor_helm_reward",(u32)(uptr)a,before,word(a,0x2790),W_VALOR_HELM_GAIN);
        }
#endif
        world_follow_capture(a);
    }
    return result;
}
static unsigned world_burst(void *a,WFollow *job,const float *point)
{
    /* GU can deliver another blade contact after a tail fallback has already
     * emitted on a long frame. Remaining<=0 is not an unused slot: retain
     * a per-Helm ordinal ledger so that contact cannot emit the same segment
     * again. Reset only for a new Helm or ownership teardown. */
    if(job->kind==1&&(world_helm_emitted&(1u<<job->emitted))){
        log_event("world_helm_duplicate_suppressed",(u32)(uptr)a,job->emitted,world_helm_count,world_helm_emitted);
        return 0;
    }
    /*2122234 and21255f5 select the last shell for ordinal6, for genuine
     * and fallback contacts alike. Its source COL part multiplier is3.0,
     * versus0.9 for earlier shells; physical/elemental values stay equal. */
    unsigned source=(job->kind==1?(job->emitted==6?1020:1000):1010)+job->tier,hit=0,shape=0;
    for(unsigned i=0;i<sizeof(world_hits)/sizeof(*world_hits);++i)
        if(world_hits[i].source==source){hit=world_hits[i].hit;shape=world_hits[i].shape;break;}
#if ML_WILDS_MOVES
    if(job->kind==4){source=1040+job->emitted/3;hit=WILDS_RELEASE_ROW+job->emitted/3;shape=WILDS_RELEASE_SHAPE;}
#endif
    if(!hit)return 0;
    void *manager=(void *)(uptr)*(u32 *)(main_base+0x1892550);if(!manager)return 0;
    unsigned char *r=((void *(*)(u32,u32))(main_base+0x474204))(0x30,0x10);if(!r)return 0;
    for(unsigned i=0;i<0x30;++i)r[i]=0;
    put(r,0,main_base+0x1760e2c);put(r,4,45);put(r,12,(u32)(uptr)a);
    for(unsigned i=0;i<3;++i)set_real(r,16+4*i,point[i]);
    *(uint16_t *)(r+32)=job->uid;*(uint16_t *)(r+34)=255;
    void *shell=((void *(*)(void *,void *,u32))(main_base+0x48c4f0))(manager,r,0);
    if(!shell)return 0;
    if(job->kind==1)world_helm_emitted|=1u<<job->emitted;
    /* World21253a6 starts regeneration with the first emitted shell, rather
     * than the earlier zero-damage blade contact. */
    if(job->kind==1&&job->emitted==0)world_helm_regen=job->tier==3?15:10;
    /* World resolves the delayed contact point at emission. Sakura normally
     * follows its target bone every frame (474674), so intercept that updater
     * only for these owned shells and retain the emitted World position. */
    /* Native shell and request pools recycle addresses independently. Remove
     * an earlier incarnation before matching the new one by either address. */
    for(unsigned i=0;i<64;++i)if(world_shell_positions[i].shell==(u32)(uptr)shell)world_shell_positions[i]=(WShellPosition){0};
    for(unsigned i=0;i<64;++i)if(!world_shell_positions[i].shell||world_shell_positions[i].until<world_elapsed){
        world_shell_positions[i]=(WShellPosition){.shell=(u32)(uptr)shell,.request=word(shell,0x136c),.helm_impact=job->kind==1?1:job->kind==4?3:2,.kind=job->kind,.power=job->power,
            .until=world_elapsed+(job->kind==4?.2f:.6f),.point={point[0],point[1],point[2]},
            .part=job->part,.part_table=job->part_table,.attached=job->kind==4,.ordinal=job->emitted,.target=job->target,.uid=job->uid,.bone=job->bone,
            .local={job->point[0],job->point[1],job->point[2]}};break;
    }
    /* Native674404 uses byte7 as started and byte6 as burst count. 255
     * suppresses the Sakura repeat branch; each World job owns one contact.
     * 1378 is elapsed age, not remaining life: advancing it kills the shell. */
    *(unsigned char *)((uptr)shell+6)=255;*(unsigned char *)((uptr)shell+7)=1;
    void *collider=(void *)((uptr)shell+0x13a8);
    ((void (*)(void *,void *,void *,u32,u32,u32))(main_base+0x2b7520))(a,shell,collider,1,hit,0);
    put(collider,0x88,shape);
    /* Native3798cc skips hit visuals for effect ID0. Both source Helm and
     * delayed Iai SHLPs bind EPV2006/0; Sakura45 would duplicate that flash. */
#if ML_WORLD_PRESENTATION
    if(job->kind==4)*(uint16_t *)((uptr)collider+0x52)=0x2e; /* GU sword slash contact. */
    else if(job->kind!=1)*(uint16_t *)((uptr)collider+0x52)=0;
#endif
    /* Both source delayed shell SHLPs live for0.5s. A3-frame shortcut could
     * expire before the collision pass on a slow frame; native attack IDs
     * already prevent repeat damage to the same target. */
    float active_frames=job->kind==4?6:30;
    set_real(collider,0x5c,0);set_real(collider,0x60,active_frames);
    put(collider,0x30,word(collider,0x30)|0x08000000u);
    if(job->kind==3)set_real(collider,0x98,real(collider,0x98)*1.3f);
    typedef float (__attribute__((pcs("aapcs-vfp"))) *ShellFloat)(void *,u32);
    float lifetime=((ShellFloat)(main_base+0x4a2ed0))(shell,0);
    set_real(shell,0x1378,lifetime>active_frames?lifetime-active_frames:0);
    log_event("world_burst",(u32)(uptr)a,source,job->emitted,(u32)(uptr)shell);
    if(job->kind==4)log_event("wilds_release_burst",(u32)(uptr)a,job->target,job->emitted,hit);
    return 1;
}
static void world_follow_tick(void *a,float dt)
{
    world_elapsed+=dt;
#if ML_WILDS_MOVES
    world_schedule_release(a);
#if ML_ASCENDANCE_PROTOTYPE
    world_release_extra_tick(a);
#endif
#endif
    for(unsigned i=0;i<56;++i){
        WFollow *job=world_follow+i;if(!job->live)continue;
#if !ML_WILDS_MOVES
        /* The World action owns the pending Iai shell until it emits. */
        if((job->kind==2||job->kind==3)&&(world.move!=W_BIG||u16(a,STATE)!=16||u16(a,ACTION)!=w_action(W_BIG))){job->live=0;continue;}
#endif
        void *target=world_monster(job->uid);
        if(job->area!=byte(a,AREA)||(u32)(uptr)target!=job->target||
           byte(target,AREA)!=job->area||world_elapsed-job->age>3){job->live=0;continue;}
        if(!(job->remaining>0)){job->live=0;continue;}
        job->remaining-=dt;
        if(job->remaining<=0){
            unsigned local=job->kind==1||job->kind==4;
            const float *m=local?world_bone(target,job->bone):world_raw_bone(target,(unsigned)job->bone);
            if(!m){job->live=0;continue;}
            float point[3];for(unsigned k=0;k<3;++k)
                point[k]=local?m[k]*job->point[0]+m[4+k]*job->point[1]+m[8+k]*job->point[2]+m[12+k]:m[12+k]+job->point[k];
            /* Source2125425..563c fills unrecorded tail slots only when the
             * last genuine contact emits. All missing slots share0.05s;
             * existing real contacts retain their own bone/point/due time. */
            if(job->kind==1&&i+1==world_helm_count&&world_helm_count<7){
                for(unsigned j=world_helm_count;j<7;++j)if(!world_follow[j].live){
                    world_follow[j]=*job;world_follow[j].emitted=j;world_follow[j].remaining=.05f;
                }
            }
            if(job->tier)world_burst(a,job,point);
            job->live=0;
        }
    }
}
