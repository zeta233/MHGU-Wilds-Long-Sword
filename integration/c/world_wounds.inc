/* GU owns part IDs and damage. Keep the opening contact in the hit
 * bone's space so the effect follows the same spot as the monster poses. */
#include "world_wound_surface.inc"
enum { WOUND_MONSTERS=10, WOUND_PARTS=8, WOUND_MARKERS=12 };
typedef struct {
    unsigned progress;
    float until,recover,flash;
    const void *part_table;
    unsigned part_index;
    int bone;
    float local[3];
    unsigned burst_damage,burst_shown;
    WSurfaceAnchor surface;
} WWound;
typedef struct {
    void *target,*status;
    unsigned id;
    WWound parts[WOUND_PARTS];
} WMonsterWounds;
static WMonsterWounds wilds_wounds[WOUND_MONSTERS];
static unsigned wilds_wound_owner,wilds_wound_zone;
static struct {
    void *target,*status;
    unsigned area,region;
    float until;
} wound_recoil;
static struct {
    void *target,*status;
    unsigned actor,area,id,slot,region,bonus;
} focus_burst;
static int (*world_original_monster_reaction)(void *);
static int (*world_original_monster_action)(void *,unsigned,unsigned,unsigned);
static struct {
    void *target,*status;
    unsigned actor,area,action,resting;
} focus_stagger;

static void world_focus_stagger_release(void)
{
    if(focus_stagger.target)
        log_event("wilds_focus_stagger_end",(u32)(uptr)focus_stagger.target,world.source,
            focus_stagger.resting,(u32)(world_elapsed*1000));
    focus_stagger.target=0;
}

static int world_monster_action(void *target,unsigned group,unsigned action,unsigned mode)
{
    if(target==focus_stagger.target&&world.move==W_FOCUS&&world.wilds_focus_break&&
       focus_stagger.actor==world.actor&&world.epoch==focus_stagger.area&&
       byte(target,AREA)==focus_stagger.area&&word(target,0x1428)==(u32)(uptr)focus_stagger.status&&
       (int)word(focus_stagger.status,0x370)>0&&u16(target,0x73e0)==focus_stagger.action&&group<10&&
       (mode==0||mode==2||mode==5)){
        if(focus_stagger.resting)return 0;
        /* GU's common idle selector (75224) uses 0:2 for species with
         * the fatigue idle capability, otherwise 0:1 or 0:0. Enter the
         * animation without changing the monster's actual stamina. */
        unsigned idle=(byte(focus_stagger.status,0x1a8)&0x40)?2:
            byte(focus_stagger.status,0x2c0)==1?1:0;
        int result=world_original_monster_action(target,0,idle,2);
        unsigned next=u16(target,0x73e0);
        if((next&255)==0){
            focus_stagger.action=next;focus_stagger.resting=1;
            log_event("wilds_focus_stagger_breathe",(u32)(uptr)target,world.source,next,
                (u32)(world_elapsed*1000));
        }
        return result;
    }
    return world_original_monster_action(target,group,action,mode);
}
extern void ml_wilds_wound(unsigned,unsigned,unsigned);

static void world_wound_owner(void *a)
{
    unsigned area=byte(a,AREA);
    if(wilds_wound_owner!=(u32)(uptr)a||(area==0&&wilds_wound_zone!=0)){
        for(unsigned i=0;i<WOUND_MONSTERS;++i)wilds_wounds[i]=(WMonsterWounds){0};
        wound_recoil.target=0;
        wilds_wound_owner=(u32)(uptr)a;
    }
    wilds_wound_zone=area;
}

static int world_monster_reaction(void *target)
{
    if(target!=wound_recoil.target){
#if ML_ASCENDANCE_PROTOTYPE
        unsigned pending=ascendance_reaction_before(target);
        unsigned before=pending?u16(target,0x73e0):0;
        int result=world_original_monster_reaction(target);
        if(result&&pending)ascendance_reaction_after(target,pending,before);
        return result;
#else
        return world_original_monster_reaction(target);
#endif
    }
    wound_recoil.target=0;
    void *status=(void *)(uptr)word(target,0x1428);
    if(status!=wound_recoil.status||byte(target,AREA)!=wound_recoil.area||
       world_elapsed>wound_recoil.until||(int)word(status,0x370)<=0)
        return world_original_monster_reaction(target);
    /* Play GU's short, species-aware recoil before the protected idle.
     * An existing stronger reaction, knockdown or capture takes priority. */
    unsigned before=u16(target,0x73e0),pending=u16(status,0x3ae);
    int result;
    if((pending&&pending!=2)||(before&255)==10||(before&255)==11||(before&255)>=14){
        result=world_original_monster_reaction(target);
    }else{
        uint16_t *reaction=(uint16_t *)((uptr)status+0x3ae);
        unsigned char *region=(unsigned char *)((uptr)status+0x3b2),previous=*region;
        *region=(unsigned char)wound_recoil.region;*reaction=2;
        result=world_original_monster_reaction(target);
        *reaction=0;*region=previous;
    }
    unsigned action=u16(target,0x73e0);
    if(world_original_monster_action&&(action&255)==10){
        focus_stagger.target=target;focus_stagger.status=status;focus_stagger.actor=world.actor;
        focus_stagger.area=wound_recoil.area;focus_stagger.action=action;focus_stagger.resting=0;
        log_event("wilds_wound_recoil",(u32)(uptr)target,wound_recoil.region,before,action);
    }
    return result;
}

static unsigned world_wound_slot(const void *registry,const void *target)
{
    if(registry)
        for(unsigned i=0;i<WOUND_MONSTERS;++i)
            if(word(registry,0x1c+4*i)==(u32)(uptr)target)return i;
    return WOUND_MONSTERS;
}

/* GU's monster query walks 44-byte hurt-part rows. Each row indexes the
 * current posed sphere/capsule in the collision manager, relative to status+500. */
static const void *world_wound_part_table(void *target)
{
    return ((const void *(*)(void *))(main_base+0x710b0))(target);
}
/* Evaluate the row itself. The native collision pool omits disabled rows,
 * so an index into the part table is not necessarily its index in that pool. */
static unsigned world_wound_pose(void *target,const void *part,float *posed)
{
    _Alignas(16) unsigned descriptor[16]={0};
    descriptor[0]=word(part,0);
    descriptor[1]=u16(part,4)|(((u16(part,10)>>12)&1u)<<16);
    descriptor[2]=word(part,16);
    for(unsigned k=0;k<3;++k){
        descriptor[4+k]=word(part,20+4*k);
        descriptor[8+k]=word(part,32+4*k);
    }
    descriptor[12]=1;set_real(descriptor,52,1.f);
    return ((unsigned (*)(unsigned,void *,const void *,float *))(main_base+0x16de70))
        (1,target,descriptor,posed);
}

static unsigned world_wound_position(void *target,const WWound *w,float *p)
{
    if(!w->part_table||world_wound_part_table(target)!=w->part_table)return 0;
    const void *part=(const void *)((uptr)w->part_table+44*w->part_index);
    if(((unsigned (*)(void *,unsigned))(*(uptr **)target)[0x370/4])(target,u16(part,12)))return 0;
    const float *m=world_bone(target,w->bone);
    if(!m)return 0;
    for(unsigned k=0;k<3;++k)
        p[k]=m[12+k]+m[k]*w->local[0]+m[4+k]*w->local[1]+m[8+k]*w->local[2];
    return 1;
}

static void world_wound_anchor(WWound *w,void *target,const void *part,const float *point)
{
    w->part_table=0;w->surface.model=0;
    const void *table=world_wound_part_table(target);
    if(!table||(uptr)part<(uptr)table)return;
    unsigned offset=(uptr)part-(uptr)table;
    if(offset%44)return;
    int bone=(int16_t)u16(part,0);
    if(bone==255)bone=-1;
    /* GU type 2 joins separate bones. Choose the end nearest the opening
     * contact once, so subsequent animation cannot move the mark to the
     * other end. Type 1 already uses the first bone for both ends. */
    if(u16(part,4)==2&&bone>=0){
        _Alignas(16) float posed[12]={0};
        int other=(int16_t)u16(part,2);
        if(other>=0&&other!=255&&other!=bone&&world_wound_pose(target,part,posed)==2){
            float first=0,second=0;
            for(unsigned k=0;k<3;++k){
                float a=point[k]-posed[k],b=point[k]-posed[4+k];
                first+=a*a;second+=b*b;
            }
            if(second<first)bone=other;
        }
    }
    const float *m=world_bone(target,bone);
    if(!m||!world_inverse(m,point,w->local))return;
    w->bone=bone;w->part_index=offset/44;w->part_table=table;
    if(surface_bind(target,part,point,&w->surface))
        log_event("wilds_wound_surface",(u32)(uptr)target,w->part_index,w->surface.mesh,bone);
}

/* Settle the reserved wound once the directional finisher closes its last hitbox. */
static void world_finish_focus_burst(void *actor,float frame)
{
    if(!focus_burst.target)return;
    unsigned end=w_focus_finisher_end(world.source);
    if(!end||frame<end)return;
    void *target=focus_burst.target,*status=focus_burst.status;
    focus_burst.target=0;
    const void *registry=(const void *)(uptr)word((void *)main_base,0x1851fe0);
    if(world.move!=W_FOCUS||!world.wilds_focus_break||world.actor!=focus_burst.actor||
       byte(actor,AREA)!=focus_burst.area||world_wound_slot(registry,target)!=focus_burst.slot||
       word(target,0x1428)!=(u32)(uptr)status||byte(target,AREA)!=focus_burst.area||
       u16(status,0x1b6)!=focus_burst.id||(int)word(status,0x370)<=0)return;
    unsigned region=focus_burst.region,bonus=focus_burst.bonus;
    WWound *w=wilds_wounds[focus_burst.slot].parts+region;
    int applied=((int (*)(void *,unsigned,int,unsigned))(main_base+0x9d6f8))(target,region,(int)bonus,0);
    w->burst_damage=applied>0?(unsigned)applied:0;
    w->flash=world_elapsed+.85f;w->burst_shown=0;
#if ML_WORLD_PRESENTATION
    WSurfaceModel model;WSurfaceVec pos,normal;
    if(w->surface.model&&surface_model(&model,target)&&surface_sample(&model,target,&w->surface,&pos,&normal))
        world_helm_impact(actor,&pos.x);
#endif
    log_event("wilds_wound_burst",region,world.source,bonus,w->burst_damage);
}

static void world_monster_wound_damage(void *manager,void *attack,void *target,void *point,
                                       void *v1,void *v2,void *part,void *shape)
{
    void *actor=(void *)(uptr)world.actor;
    void *status=0;
    unsigned slot=WOUND_MONSTERS,region=0,threshold=0,before=0;
    /* Direct attacks by the local Long Sword only. The native path remains
     * authoritative for collisions, bounce, invulnerability and damage. */
    if(attack&&target&&part&&point&&actor&&world.actor==word(attack,0xc)&&
       byte(attack,0x10)==0&&local_supported(actor)&&world.epoch==byte(actor,AREA)&&
       byte(actor,AREA)==byte(target,AREA)&&byte(actor,AREA)!=0){
        const void *registry=(const void *)(uptr)word((void *)main_base,0x1851fe0);
        slot=world_wound_slot(registry,target);
        region=byte(part,8);
        if(slot<WOUND_MONSTERS&&region<WOUND_PARTS){
            status=(void *)(uptr)word(target,0x1428);
            if(status&&u16(status,0x1b6)<10000&&(int)word(status,0x370)>0){
                threshold=u16(status,0x3be + 12*region);
                before=u16(status,0x488+2*region);
            }
        }
    }
    world_original_monster_damage(manager,attack,target,point,v1,v2,part,shape);
    if(!threshold||threshold>20000||word(target,0x1428)!=(u32)(uptr)status)return;
    unsigned after=u16(status,0x488+2*region);
    unsigned damage=(uint16_t)(after-before);
    if(!damage||damage>2000)return;
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance_flash_attack(attack)){
#if ML_WORLD_PRESENTATION
        if(!ascendance.radiance_hit&&ascendance_independent_flash())
            world_radiance_contact_fx(actor,point);
#endif
        ascendance_flash_target(actor,attack,target,status,region,point);
    }
    ascendance_hit(actor,attack,target,0,damage);
    if(ascendance_owner(actor)&&ascendance_owned_blade(attack))
        ascendance_musou_mark(target,status,region,(const float *)point,damage);
#endif
    world_wound_owner(actor);
    WMonsterWounds *monster=wilds_wounds+slot;
    unsigned id=u16(status,0x1b6);
    if(monster->target!=target||monster->status!=status||monster->id!=id)
        *monster=(WMonsterWounds){.target=target,.status=status,.id=id};
    WWound *w=monster->parts+region;
    unsigned focus=world.move==W_FOCUS&&world.source==905&&world_attack_owned&&
        world_attack_actor==world.actor&&u16(attack,0x34)==world_attack_id;
    if(focus&&!world.wilds_focus_break&&w->until<=world_elapsed){
        /* Adjacent GU hurt parts can win the same blade contact. Resolve
         * only a nearby, currently posed wound on the monster actually hit. */
        float best=90.f*90.f;
        for(unsigned i=0;i<WOUND_PARTS;++i){
            WWound *candidate=monster->parts+i;
            float pos[3],d2=0;
            if(candidate->until<=world_elapsed||!world_wound_position(target,candidate,pos))continue;
            for(unsigned k=0;k<3;++k){float d=pos[k]-real(point,4*k);d2+=d*d;}
            if(d2<best){
                unsigned limit=u16(status,0x3be + 12*i);
                if(!limit||limit>20000)continue;
                best=d2;region=i;threshold=limit;w=candidate;
            }
        }
    }
    if(w->until>world_elapsed){
        if(!focus||world.wilds_focus_break)return;
        w->progress=0;w->until=0;w->recover=world_elapsed+12;w->flash=0;
        world.wilds_focus_break=1;
        unsigned bonus=25+threshold/4;if(bonus>120)bonus=120;
        focus_burst.target=target;focus_burst.status=status;focus_burst.slot=slot;
        focus_burst.actor=world.actor;focus_burst.area=byte(target,AREA);
        focus_burst.id=id;focus_burst.region=region;focus_burst.bonus=bonus;
        wound_recoil.target=target;wound_recoil.status=status;wound_recoil.area=byte(target,AREA);
        wound_recoil.region=region;wound_recoil.until=world_elapsed+.25f;
#if ML_WORLD_PRESENTATION
        world_helm_impact(actor,(const float *)point);
#endif
#if ML_ASCENDANCE_PROTOTYPE
        ascendance_hit(actor,0,target,1,0);
#endif
        log_event("wilds_wound_break",world.actor,(u32)(uptr)target,region,bonus);
        return;
    }
    if(w->until){w->until=0;w->progress=0;}
    /* A Focus Strike on a healthy part contributes damage for future wounds,
     * but can't create and destroy the same wound on that hit. */
    if(world_elapsed<w->recover)return;
    unsigned opening=(threshold*65+99)/100;if(opening<24)opening=24;
    w->progress+=damage;
    if(w->progress>=opening){
        world_wound_anchor(w,target,part,(const float *)point);
        w->until=world_elapsed+90;
        log_event("wilds_wound_open",world.actor,(u32)(uptr)target,region,w->progress);
    }
}

typedef struct {
    WSurfaceVec start,end;
    float radius2;
    unsigned row;
} WWoundOccluder;

static unsigned wound_occluders(void *target,const void *table,WWoundOccluder *out,unsigned capacity)
{
    unsigned count=0;
    for(unsigned row=0;row<64&&count<capacity;++row){
        const void *part=(const void *)((uptr)table+44*row);
        if(u16(part,0)==65535)break;
        if(!(real(part,16)>12.f)||((unsigned (*)(void *,unsigned))(*(uptr **)target)[0x370/4])(target,u16(part,12)))continue;
        _Alignas(16) float pose[12]={0};
        unsigned kind=world_wound_pose(target,part,pose);
        if((kind!=1&&kind!=2)||!(pose[8]>0))continue;
        WSurfaceVec start={pose[0],pose[1],pose[2]};
        WSurfaceVec end=kind==2?(WSurfaceVec){pose[4],pose[5],pose[6]}:start;
        /* Hurt volumes extend beyond the skin. Only use the inner part
         * as a conservative occluder for another wound's marker. */
        float radius=pose[8]*.85f;
        out[count++]=(WWoundOccluder){start,end,radius*radius,row};
    }
    return count;
}

static unsigned wound_hidden(WSurfaceVec point,WSurfaceVec view_dir,const WWoundOccluder *volumes,
                             unsigned count,unsigned wound_row)
{
    float view2=surface_dot(view_dir,view_dir);
    if(!(view2>4.f))return 1;
    for(unsigned i=0;i<count;++i){
        const WWoundOccluder *v=volumes+i;
        if(v->row==wound_row)continue;
        WSurfaceVec line=surface_sub(v->end,v->start),delta=surface_sub(v->start,point);
        float axis2=surface_dot(line,line),cross=surface_dot(view_dir,line),projection=surface_dot(delta,view_dir);
        float det=view2*axis2-cross*cross;
        float along=det>.001f?(projection*cross-view2*surface_dot(delta,line))/det:0;
        if(along<0)along=0;
        if(along>1)along=1;
        delta=surface_add(delta,line,along);
        float t=surface_dot(delta,view_dir)/view2;
        if(t<=.035f||t>=1)continue;
        WSurfaceVec separation=surface_add(delta,view_dir,-t);
        if(surface_dot(separation,separation)<v->radius2)return 1;
    }
    return 0;
}

static int world_screen_view(void *camera)
{
    const void *hud=*(void **)(main_base+0x212668c),*render=*(void **)(main_base+0x184d750);
    if(!hud||!render)return -1;
    unsigned view=byte(hud,236);
    if(view>=4)return -1;
    const void *viewport=(const void *)((uptr)render+432*view);
    if(word(viewport,84)!=(u32)(uptr)camera||word(viewport,112)<=word(viewport,104)||
       word(viewport,116)<=word(viewport,108))return -1;
    return (int)view;
}
static void world_wound_markers(void *a,void *camera)
{
    unsigned aiming=(ml_pad_raw_bits()&PAD_FOCUS)&&(byte(a,DRAWN)&1);
    const void *registry=(const void *)(uptr)word((void *)main_base,0x1851fe0);
    if(!registry||!(real(a,0x1c)>0))return;
    int view=world_screen_view(camera);
    if(view<0)return;
    world_wound_owner(a);
    unsigned marker=0;
    for(unsigned i=0;i<WOUND_MONSTERS&&marker<WOUND_MARKERS;++i){
        WMonsterWounds *m=wilds_wounds+i;
        if(!m->target||word(registry,0x1c+4*i)!=(u32)(uptr)m->target||
           word(m->target,0x1428)!=(u32)(uptr)m->status||u16(m->status,0x1b6)!=m->id||
           byte(m->target,AREA)!=byte(a,AREA))continue;
        if(!aiming){
            unsigned j=0;
            while(j<WOUND_PARTS&&m->parts[j].flash<=world_elapsed)++j;
            if(j==WOUND_PARTS)continue;
        }
        const void *table=world_wound_part_table(m->target);
        const WSurfaceVec eye={real(camera,0x40),real(camera,0x44),real(camera,0x48)};
        WSurfaceModel model;
        WWoundOccluder volumes[48];
        unsigned loaded=0,volume_count=0,volumes_ready=0;
        for(unsigned j=0;j<WOUND_PARTS&&marker<WOUND_MARKERS;++j){
            WWound *w=m->parts+j;
            unsigned kind=w->flash>world_elapsed?2:
                aiming&&w->until>world_elapsed&&(int)word(m->status,0x370)>0?1:0;
            if(!kind||!w->surface.model||!table||w->part_table!=table)continue;
            const void *part=(const void *)((uptr)table+44*w->part_index);
            if(((unsigned (*)(void *,unsigned))(*(uptr **)m->target)[0x370/4])(m->target,u16(part,12)))continue;
            if(!loaded){if(!surface_model(&model,m->target))break;loaded=1;}
            WSurfaceVec point,normal;
            if(!surface_sample(&model,m->target,&w->surface,&point,&normal))continue;
            /* The strip winding gives the outward normal. Hide the dot
             * on the far side without scanning the monster mesh each frame. */
            WSurfaceVec view_dir=surface_sub(eye,point);
            float facing=surface_dot(normal,view_dir);
            if(facing<=0||facing*facing<.1296f*surface_dot(normal,normal)*surface_dot(view_dir,view_dir))continue;
            if(!volumes_ready){
                volume_count=wound_occluders(m->target,table,volumes,sizeof(volumes)/sizeof(*volumes));
                volumes_ready=1;
            }
            if(wound_hidden(point,view_dir,volumes,volume_count,w->part_index))continue;
            float screen[4];
            ((void (*)(float *,const float *,unsigned))(main_base+0x551340))(screen,&point.x,view);
            if(screen[3]>0&&screen[2]>0&&screen[0]>0&&screen[0]<1280&&screen[1]>0&&screen[1]<720){
                unsigned sx=(unsigned)(screen[0]*(4094.0f/1280));
                unsigned sy=(unsigned)((720-screen[1])*(4094.0f/720));
                ml_wilds_wound(marker++,kind<<24|sy<<12|sx,kind==2?w->burst_damage:0);
                if(kind==2&&!w->burst_shown){
                    w->burst_shown=1;log_event("wilds_wound_popup",w->burst_damage,sx,sy,j);
                }
            }
        }
    }
    for(;marker<WOUND_MARKERS;++marker)ml_wilds_wound(marker,0,0);
}
