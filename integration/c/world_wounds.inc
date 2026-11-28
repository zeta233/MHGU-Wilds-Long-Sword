/* GU owns part IDs and damage. Keep the opening contact in the hit
 * bone's space so the effect follows the same spot as the monster poses. */
#include "world_wound_surface.inc"
enum { WOUND_MONSTERS=10, WOUND_PARTS=8, WOUND_MARKERS=12 };
enum { WOUND_REACTION_DEFEAT=1, WOUND_REACTION_FLINCH=2, WOUND_REACTION_HEAVY=3 };
static const float wound_break_flash_time=.85f;
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
typedef struct {
    void *target,*status;
    unsigned actor,area,id,slot,eligible,regions;
    unsigned blocked,pending,deflected_region,settled,count;
    unsigned char order[WOUND_PARTS];
} WWoundExecution;
static WWoundExecution wound_execution[WOUND_MONSTERS];
static int (*world_original_monster_reaction)(void *);
static int (*world_original_monster_action)(void *,unsigned,unsigned,unsigned);

extern void ml_wilds_wound(unsigned,unsigned,unsigned);

static void world_wound_owner(void *a)
{
    unsigned area=byte(a,AREA);
    if(wilds_wound_owner!=(u32)(uptr)a||(area==0&&wilds_wound_zone!=0)){
        for(unsigned i=0;i<WOUND_MONSTERS;++i)wilds_wounds[i]=(WMonsterWounds){0};
        for(unsigned i=0;i<WOUND_MONSTERS;++i)wound_execution[i]=(WWoundExecution){0};
        wilds_wound_owner=(u32)(uptr)a;
    }
    wilds_wound_zone=area;
}

static WWoundExecution *world_wound_execution_for(void *target)
{
    for(unsigned i=0;i<WOUND_MONSTERS;++i)
        if(wound_execution[i].target==target&&wound_execution[i].regions)return wound_execution+i;
    return 0;
}

static unsigned world_release_wound_target(WWoundExecution *execution,void *actor);

static unsigned world_wound_holds_action(void *target,void *status,unsigned group,unsigned action)
{
    WWoundExecution *execution=world_wound_execution_for(target);
    if(!execution)return 0;
    void *actor=(void *)(uptr)execution->actor;
    unsigned area=execution->area;
    if(status!=execution->status||!status||byte(target,AREA)!=area||
       world.actor!=execution->actor||byte(actor,AREA)!=area||
       world.move!=W_FOCUS||!world.wilds_focus_break||
       u16(actor,STATE)!=16||u16(actor,ACTION)!=w_action(W_FOCUS)||
       (int)word(status,0x370)<=0){
        return world_release_wound_target(execution,actor)&&group<10;
    }
    /* A stronger native reaction can still take priority. */
    if(!group||group>=10)return 0;
    if(!execution->blocked){
        execution->blocked=1;
        log_event("wilds_wound_hold",(u32)(uptr)target,world.source,
            (group<<8)|action,(u32)real(actor,FRAME));
    }
    return 1;
}

static int world_monster_action(void *target,unsigned group,unsigned action,unsigned mode)
{
    void *status=(void *)(uptr)word(target,0x1428);
    if(world_wound_holds_action(target,status,group,action))return 0;
    return world_original_monster_action(target,group,action,mode);
}

/* Ask GU's damage dispatcher for a species- and part-specific heavy
 * reaction. Its action state machine owns recovery and stronger reactions. */
static int world_request_part_reaction(void *target,void *status,unsigned region,unsigned kind)
{
    /* GU reaction 1 dispatches defeat even with positive HP. This adapter
     * may request only the two native part-hit reactions we defer. */
    if(kind!=WOUND_REACTION_FLINCH&&kind!=WOUND_REACTION_HEAVY)return 0;
    uint16_t *reaction=(uint16_t *)((uptr)status+0x3ae);
    unsigned char *part=(unsigned char *)((uptr)status+0x3b2),previous=*part;
    *part=(unsigned char)region;
    *reaction=(uint16_t)kind;
    int result=world_original_monster_reaction(target);
    *reaction=0;
    *part=previous;
    return result;
}

static int world_request_heavy_reaction(void *target,void *status,unsigned region)
{
    return world_request_part_reaction(target,status,region,WOUND_REACTION_HEAVY);
}

static int world_request_ground_reaction(void *target,void *status,unsigned region)
{
    enum { GLAVENUS_VTABLE=0x17f1d44, GROUND_RECOIL=0x70a };
    if(word(target,0)==main_base+GLAVENUS_VTABLE&&world_original_monster_action){
        /* Its limb-specific dispatch can pick the hop. Use the native
         * grounded heavy recoil, including its normal recovery animation. */
        world_original_monster_action(target,GROUND_RECOIL&255,GROUND_RECOIL>>8,5);
        if(u16(target,0x73e0)==GROUND_RECOIL){
            *(uint16_t *)((uptr)status+0x3ae)=0;
            return 1;
        }
    }
    return world_request_heavy_reaction(target,status,region);
}
static unsigned world_finish_wound_recoil(void *target,void *status,unsigned region,unsigned deferred,unsigned deferred_region);

static unsigned world_release_wound_target(WWoundExecution *execution,void *actor)
{
    WWoundExecution saved=*execution;
    *execution=(WWoundExecution){0};
    void *target=saved.target,*status=saved.status;
    if(!saved.regions)return 0;
    log_event("wilds_wound_hold_end",(u32)(uptr)target,saved.actor,(u32)(world_elapsed*1000),0);
    if(saved.actor!=(u32)(uptr)actor||byte(actor,AREA)!=saved.area||
       world_monster(saved.slot)!=target||word(target,0x1428)!=(u32)(uptr)status||
       byte(target,AREA)!=saved.area||u16(status,0x1b6)!=saved.id||(int)word(status,0x370)<=0)return 0;
    if(saved.settled)
        return world_finish_wound_recoil(target,status,saved.order[0],saved.pending,saved.deflected_region);
    if(saved.pending&&!u16(status,0x3ae)&&(u16(target,0x73e0)&255)<10)
        return world_request_part_reaction(target,status,saved.deflected_region,saved.pending)!=0;
    return 0;
}

static void world_release_wound_execution(void *actor)
{
    for(unsigned i=0;i<WOUND_MONSTERS;++i)
        if(wound_execution[i].target)world_release_wound_target(wound_execution+i,actor);
}

static int world_monster_reaction(void *target)
{
    WWoundExecution *execution=world_wound_execution_for(target);
    if(execution&&world.move==W_FOCUS&&world.wilds_focus_break){
        void *status=(void *)(uptr)word(target,0x1428);
        if(status&&status==execution->status&&byte(target,AREA)==execution->area&&
           (int)word(status,0x370)>0){
            unsigned pending=u16(status,0x3ae);
            /* These native flinches displace the target between blade windows. */
            if(pending==WOUND_REACTION_FLINCH||pending==WOUND_REACTION_HEAVY){
                if(pending>=execution->pending){
                    execution->pending=pending;
                    execution->deflected_region=byte(status,0x3b2);
                }
                *(uint16_t *)((uptr)status+0x3ae)=0;
                return 0;
            }
        }
    }
#if ML_ASCENDANCE_PROTOTYPE
        unsigned pending=ascendance_reaction_before(target);
        unsigned before=pending?u16(target,0x73e0):0;
        if(ascendance_independent_flash()){
            if(pending!=2)return world_original_monster_reaction(target);
#if ML_OFFSET_ALWAYS_STAGGER
            /* An ongoing stagger or a native knockdown already meets the
             * test condition. Death and capture still belong to GU. */
            unsigned group=before&255;
            if(group==10||group==11||u16(radiance_pursuit.status,0x3ae)){
                int result=world_original_monster_reaction(target);
                ascendance_reaction_after(target,pending,before);
                return result;
            }
#endif
            if((before&255)>=10)return world_original_monster_reaction(target);
            int result=world_request_ground_reaction(target,radiance_pursuit.status,radiance_pursuit.region);
            if(result)
                ascendance_reaction_after(target,pending,before);
            return result;
        }
        int result=world_original_monster_reaction(target);
        if(result&&pending)ascendance_reaction_after(target,pending,before);
        return result;
#else
        return world_original_monster_reaction(target);
#endif
}

static unsigned world_finish_wound_recoil(void *target,void *status,unsigned region,unsigned deferred,unsigned deferred_region)
{
    if((int)word(status,0x370)<=0)return 0;
    unsigned before=u16(target,0x73e0),pending=u16(status,0x3ae);
    unsigned group=before&255;
    if(group==10||group==11||group>=14||pending==WOUND_REACTION_DEFEAT||pending>WOUND_REACTION_HEAVY)return 0;
    if(deferred>=3&&pending<3)region=deferred_region;
    if(pending==3)region=byte(status,0x3b2);
    world_request_ground_reaction(target,status,region);
    unsigned action=u16(target,0x73e0);
    if(before==action||((action&255)!=10&&(action&255)!=11))return 0;
    log_event("wilds_wound_recoil",(u32)(uptr)target,region,before,action);
    return 1;
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

static void world_consume_wound(void *actor,WMonsterWounds *monster,unsigned region,unsigned bonus)
{
    WWound *w=monster->parts+region;
    void *target=monster->target,*status=monster->status;
    w->progress=0;w->until=0;w->recover=world_elapsed+12;
    w->flash=world_elapsed+wound_break_flash_time;w->burst_shown=0;
#if ML_ASCENDANCE_PROTOTYPE
    ascendance_hit(actor,0,target,1,0);
#endif
    int applied=0;
    if(bonus&&(int)word(status,0x370)>0)
        applied=((int (*)(void *,unsigned,int,unsigned))(main_base+0x9d6f8))(target,region,(int)bonus,0);
    w->burst_damage=applied>0?(unsigned)applied:0;
#if ML_WORLD_PRESENTATION
    float point[3];
    if(world_wound_position(target,w,point))world_helm_impact(actor,point);
#endif
    log_event("wilds_wound_burst",region,world.source,bonus,w->burst_damage);
}

#if ML_WILDS_MOVES
static int world_wound_execution_valid(const WWoundExecution *execution,void *actor)
{
    void *target=execution->target,*status=execution->status;
    return target&&execution->actor==(u32)(uptr)actor&&byte(actor,AREA)==execution->area&&
        world_monster(execution->slot)==target&&byte(target,AREA)==execution->area&&
        word(target,0x1428)==(u32)(uptr)status&&u16(status,0x1b6)==execution->id;
}

/* GU has no Wilds scar damage table. Calibrate the first three ordinary
 * wounds from measured 0.6%, 1.0% and 1.4% of total health, and reuse the
 * last measured rate for subsequent wounds. */
static unsigned world_focus_wound_damage(const void *status,unsigned ordinal)
{
    unsigned rate=ordinal==0?6:ordinal==1?10:14;
    int maximum=(int)word(status,0x374);
    return maximum>0?(unsigned)(((uint64_t)(unsigned)maximum*rate+500)/1000):0;
}

static void world_collect_focus_wounds(void *manager,const void *attack,WWoundExecution *execution,unsigned direct_region,const float *contact_point)
{
    WMonsterWounds *monster=wilds_wounds+execution->slot;
    unsigned candidates=execution->eligible&~execution->regions;
    if(!candidates)return;
    void *resource=(void *)(uptr)word(attack,0x84);
    const void *group=resource?((const void *(*)(void *,unsigned))(*(uptr **)resource)[0x44/4])
        (resource,word(attack,0x88)):0;
    unsigned first=word(attack,0x1c),count=group?word(group,4):0;
    if(first>=64||count>64-first)count=0;
    /* The native contact wins simultaneous overlaps. Later samples append
     * new regions to the same target's order. */
    for(unsigned i=0;i<=WOUND_PARTS;++i){
        if(!i&&direct_region==WOUND_PARTS)continue;
        unsigned region=i?i-1:direct_region;
        if(i&&region==direct_region)continue;
        if(!(candidates&(1u<<region)))continue;
        WWound *w=monster->parts+region;
        if(w->until<=world_elapsed)continue;
        float position[3];
        unsigned posed=world_wound_position(execution->target,w,position);
        /* GU reports only its winning hurt part. Test the exact thrust
         * spheres against other wound anchors on that same native target. */
        unsigned hit=!w->part_table&&region==direct_region;
        _Alignas(16) float wound[12]={0};
        if(posed){
            for(unsigned k=0;k<3;++k)wound[k]=wound[4+k]=position[k];
            wound[8]=36.f;
            if(contact_point){
                float distance=0;
                for(unsigned k=0;k<3;++k){float d=position[k]-contact_point[k];distance+=d*d;}
                hit=distance<=36.f*36.f;
            }
        }
        unsigned first_type=0;
        for(unsigned i=0;i<count&&posed&&!hit;++i){
            const void *record=(const void *)((uptr)manager+0x2030+32*(first+i));
            unsigned type=byte(record,0);
            if(!i)first_type=type;
            if(type<1||type>3)continue;
            _Alignas(16) float shape[12]={0},contact[4]={0};
            for(unsigned k=0;k<3;++k)shape[k]=real(record,8+4*k);
            if(type==1)shape[3]=real(record,4);
            else{
                for(unsigned k=0;k<3;++k)shape[4+k]=real(record,20+4*k);
                shape[8]=real(record,4);
            }
            unsigned fn=type==1?0x168198:type==2?0x167b5c:0x1683dc;
            hit=((unsigned (*)(const void *,const void *,void *))(main_base+fn))(shape,wound,contact);
        }
        if(contact_point){
            log_event("wilds_wound_overlap",region,direct_region,(count<<8)|posed,(first_type<<8)|hit);
            if(posed)log_event("wilds_wound_overlap_distance",region,(int)(position[0]-contact_point[0]),
                (int)(position[1]-contact_point[1]),(int)(position[2]-contact_point[2]));
        }
        if(!hit)continue;
        unsigned threshold=u16(execution->status,0x3be + 12*region);
        if(!threshold||threshold>20000)continue;
        unsigned ordinal=execution->count++;
        execution->regions|=1u<<region;
        execution->order[ordinal]=region;
        world.wilds_focus_break=1;
        log_event("wilds_wound_break",world.actor,(u32)(uptr)execution->target,region,
            world_focus_wound_damage(execution->status,ordinal));
    }
}

static void world_sweep_focus_wounds(void *actor,void *manager,const void *attack)
{
    for(unsigned i=0;i<WOUND_MONSTERS;++i){
        WWoundExecution *execution=wound_execution+i;
        if(!execution->settled&&execution->eligible!=execution->regions&&
           world_wound_execution_valid(execution,actor))
            world_collect_focus_wounds(manager,attack,execution,WOUND_PARTS,0);
    }
}
#endif

/* Commit every wound from this thrust on the same closing frame. Each
 * monster keeps one recovery action regardless of the number of wounds. */
static void world_finish_focus_burst(void *actor,float frame)
{
#if ML_WILDS_MOVES
    if(world.move!=W_FOCUS||!world.wilds_focus_break)return;
    unsigned end=w_focus_finisher_end(world.source);
    if(!end||frame<end)return;
    unsigned count=0;
    for(unsigned i=0;i<WOUND_MONSTERS;++i){
        WWoundExecution *execution=wound_execution+i;
        if(!execution->regions||execution->settled||!world_wound_execution_valid(execution,actor))continue;
        WMonsterWounds *monster=wilds_wounds+i;
        execution->settled=1;
        for(unsigned ordinal=0;ordinal<execution->count;++ordinal){
            unsigned region=execution->order[ordinal];
            world_consume_wound(actor,monster,region,world_focus_wound_damage(execution->status,ordinal));
            ++count;
            if(hunter_style(actor)==5){
                unsigned before=word(actor,0x2790);
                world_add_valor_meter(actor,W_VALOR_WOUND_GAIN);
                log_event("valor_wound_reward",(u32)(uptr)actor,before,word(actor,0x2790),W_VALOR_WOUND_GAIN);
            }
        }
    }
    if(count){
        world.wilds_focus_wounds=count;
        log_event("wilds_wound_settle",(u32)(uptr)actor,world.source,count,0);
    }
#endif
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
    const void *previous_scale=world_wound_scaled_attack;
    world_wound_scaled_attack=0;
    if(threshold&&threshold<=20000&&world.move!=W_FOCUS){
        WMonsterWounds *monster=wilds_wounds+slot;
        if(monster->target==target&&monster->status==status&&monster->id==u16(status,0x1b6)&&
           monster->parts[region].until>world_elapsed)
            world_wound_scaled_attack=attack;
    }
    world_original_monster_damage(manager,attack,target,point,v1,v2,part,shape);
    world_wound_scaled_attack=previous_scale;
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
    if(world.move==W_RELEASE&&ascendance.release_power==170&&ascendance_owned_blade(attack))
        world_release_credit(target,status,damage,u16(attack,0x38));
    if(ascendance_owner(actor)&&ascendance_owned_blade(attack))
        ascendance_musou_mark(target,status,region,(const float *)point,damage);
#endif
    world_wound_owner(actor);
    WMonsterWounds *monster=wilds_wounds+slot;
    unsigned id=u16(status,0x1b6);
    if(monster->target!=target||monster->status!=status||monster->id!=id)
        *monster=(WMonsterWounds){.target=target,.status=status,.id=id};
    WWound *w=monster->parts+region;
#if ML_WILDS_MOVES
    unsigned focus=world.move==W_FOCUS&&world.source==905&&world_attack_owned&&
        world_attack_actor==world.actor&&u16(attack,0x34)==world_attack_id;
    WWoundExecution *execution=wound_execution+slot;
    if(focus){
        if(execution->target!=target||execution->status!=status||execution->id!=id){
            *execution=(WWoundExecution){
                .target=target,.status=status,.actor=world.actor,.area=byte(target,AREA),.id=id,.slot=slot
            };
            for(unsigned i=0;i<WOUND_PARTS;++i)
                if(monster->parts[i].until>world_elapsed)execution->eligible|=1u<<i;
        }
#if ML_WORLD_PRESENTATION
        unsigned previous=execution->regions;
#endif
        world_collect_focus_wounds(manager,attack,execution,region,point);
#if ML_WORLD_PRESENTATION
        if(!previous&&execution->regions)world_helm_impact(actor,(const float *)point);
#endif
    }
    if(execution->target==target&&execution->status==status&&(execution->regions&(1u<<region)))return;
    /* A thrust only reserves existing wounds. The subsequent cuts are
     * ordinary damage on any unrelated part. */
    if(world.move==W_FOCUS&&w->until>world_elapsed)return;
#endif
    if(w->until>world_elapsed){
        /* An open wound starts a fresh damage budget based on this GU part. */
        unsigned limit=threshold<36?36:threshold;
        w->progress+=damage;
        if(w->progress>=limit){
            log_event("wilds_wound_damage_break",world.actor,(u32)(uptr)target,region,w->progress);
            /* The hit already dealt its damage. This path only closes the wound. */
            world_consume_wound(actor,monster,region,0);
        }
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
        w->progress=0;
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

static float wound_occlusion(WSurfaceVec point,WSurfaceVec view_dir,const WWoundOccluder *volumes,
                             unsigned count,unsigned wound_row)
{
    float view2=surface_dot(view_dir,view_dir);
    if(!(view2>4.f))return 0;
    float visibility=1.f;
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
        float clearance=surface_dot(separation,separation)-v->radius2;
        if(clearance<=0)return 0;
        float fade=3.2f*clearance/v->radius2;
        if(fade<visibility)visibility=fade;
    }
    return visibility;
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

static void wound_project(float *screen,WSurfaceVec point,unsigned view)
{
    _Alignas(16) float position[4]={point.x,point.y,point.z,1.f};
    ((void (*)(float *,const float *,unsigned))(main_base+0x551340))(screen,position,view);
}

static unsigned wound_axis_component(float delta)
{
    /* Signed quarter pixels at the 640x360 reference size. */
    float units=2.f*delta;
    if(units>=127.f)return 127u;
    if(units<=-127.f)return 129u;
    return (unsigned)(int)(units+(units<0?-.5f:.5f))&255u;
}

static unsigned wound_project_axes(WSurfaceVec point,WSurfaceVec edge,WSurfaceVec normal,
                                   const float *center,unsigned view)
{
    WSurfaceVec across=surface_cross(normal,edge);
    float along_length=att_sqrt(surface_dot(edge,edge));
    float across_length=att_sqrt(surface_dot(across,across));
    if(!(along_length>.001f&&across_length>.001f))return 0;
    _Alignas(16) float u[4],v[4];
    wound_project(u,surface_add(point,edge,42.f/along_length),view);
    wound_project(v,surface_add(point,across,17.f/across_length),view);
    if(!(u[3]>0&&v[3]>0))return 0;
    return wound_axis_component(u[0]-center[0])|(wound_axis_component(u[1]-center[1])<<8)|
        (wound_axis_component(v[0]-center[0])<<16)|(wound_axis_component(v[1]-center[1])<<24);
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
#if ML_TRACE_EVENTS
    static unsigned previous_style;
    unsigned visible_style=0;
#endif
    for(unsigned i=0;i<WOUND_MONSTERS&&marker<WOUND_MARKERS;++i){
        WMonsterWounds *m=wilds_wounds+i;
        if(!m->target||word(registry,0x1c+4*i)!=(u32)(uptr)m->target||
           word(m->target,0x1428)!=(u32)(uptr)m->status||u16(m->status,0x1b6)!=m->id||
           byte(m->target,AREA)!=byte(a,AREA))continue;
        const void *table=world_wound_part_table(m->target);
        const WSurfaceVec eye={real(camera,0x40),real(camera,0x44),real(camera,0x48)};
        WSurfaceModel model;
        WWoundOccluder volumes[48];
        unsigned loaded=0,volume_count=0,volumes_ready=0;
        for(unsigned j=0;j<WOUND_PARTS&&marker<WOUND_MARKERS;++j){
            WWound *w=m->parts+j;
            unsigned broken=w->flash>world_elapsed;
            unsigned open=w->until>world_elapsed&&(int)word(m->status,0x370)>0;
            if((!broken&&!open)||!w->surface.model||!table||w->part_table!=table)continue;
            const void *part=(const void *)((uptr)table+44*w->part_index);
            if(((unsigned (*)(void *,unsigned))(*(uptr **)m->target)[0x370/4])(m->target,u16(part,12)))continue;
            if(!loaded){if(!surface_model(&model,m->target))break;loaded=1;}
            WSurfaceVec point,normal,edge;
            if(!surface_sample(&model,m->target,&w->surface,&point,&normal,&edge))continue;
            /* The strip winding tells us which wounds face away from the camera. */
            WSurfaceVec view_dir=surface_sub(eye,point);
            float facing=surface_dot(normal,view_dir);
            float metric=surface_dot(normal,normal)*surface_dot(view_dir,view_dir);
            if(!(facing>0&&facing*facing>.04f*metric))continue;
            float incidence=(facing/att_sqrt(metric)-.20f)*3.2f;
            if(incidence>1.f)incidence=1.f;
            if(!volumes_ready){
                volume_count=wound_occluders(m->target,table,volumes,sizeof(volumes)/sizeof(*volumes));
                volumes_ready=1;
            }
            float visibility=incidence*wound_occlusion(point,view_dir,volumes,volume_count,w->part_index);
            unsigned opacity=(unsigned)(7.f*visibility+.5f);
            if(!opacity)continue;
            /* The high bit distinguishes break flashes from posed wound decals. */
            unsigned kind=broken?0x80u+(unsigned)(127.f*(1.f-(w->flash-world_elapsed)/wound_break_flash_time)+.5f):
                (opacity<<4)|(((i*3u+j)&3u)<<2)|(1u+aiming);
            _Alignas(16) float screen[4];
            wound_project(screen,point,view);
            if(screen[3]>0&&screen[2]>0&&screen[0]>0&&screen[0]<1280&&screen[1]>0&&screen[1]<720){
                unsigned sx=(unsigned)(screen[0]*(4094.0f/1280));
                unsigned sy=(unsigned)((720-screen[1])*(4094.0f/720));
                unsigned detail=broken?w->burst_damage:wound_project_axes(point,edge,normal,screen,view);
                if(!broken&&!detail)continue;
                ml_wilds_wound(marker++,kind<<24|sy<<12|sx,detail);
#if ML_TRACE_EVENTS
                unsigned style=broken?3:kind&3;
                if(!visible_style&&style!=previous_style)
                    log_event("wilds_wound_visible",style,sx,sy,detail);
                if(!visible_style)visible_style=style;
#endif
                if(broken&&!w->burst_shown){
                    w->burst_shown=1;log_event("wilds_wound_popup",w->burst_damage,sx,sy,j);
                }
            }
        }
    }
    for(;marker<WOUND_MARKERS;++marker)ml_wilds_wound(marker,0,0);
#if ML_TRACE_EVENTS
    if(previous_style&&!visible_style)log_event("wilds_wound_visible",0,0,0,0);
    previous_style=visible_style;
#endif
}
