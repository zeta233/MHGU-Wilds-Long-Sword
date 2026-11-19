/* Keep GU's part IDs and damage accounting. A wound is attached to the
 * current hurt volume of the part that filled its wound threshold. */
enum { WOUND_MONSTERS=10, WOUND_PARTS=8, WOUND_MARKERS=12 };
typedef struct {
    unsigned progress;
    float until,recover,flash;
    const void *part_table;
    unsigned part_index;
    float fraction;
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
extern void ml_wilds_wound(unsigned,unsigned);

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
    if(target!=wound_recoil.target)return world_original_monster_reaction(target);
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
    if((action&255)==10){
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
static const void *world_wound_volume(const void *manager,const void *status,unsigned part)
{
    unsigned first=word(status,0x500);
    if(!manager||first>=256||part>=256-first)return 0;
    const void *v=(const void *)((uptr)manager+0x1c+(first+part)*32);
    unsigned type=byte(v,0);
    return type>=1&&type<=3&&real(v,4)>0?v:0;
}
/* Mirror GU's 16ae40 hurt-part descriptor, but evaluate it against the
 * current pose when the camera draws. The collision pool may be a tick old. */
static unsigned world_wound_position(void *target,const WWound *w,float *p)
{
    if(!w->part_table||world_wound_part_table(target)!=w->part_table)return 0;
    const void *part=(const void *)((uptr)w->part_table+44*w->part_index);
    if(((unsigned (*)(void *,unsigned))(*(uptr **)target)[0x370/4])(target,u16(part,12)))return 0;
    _Alignas(16) unsigned descriptor[16]={0};
    _Alignas(16) float posed[12]={0};
    descriptor[0]=word(part,0);
    descriptor[1]=u16(part,4)|(((u16(part,10)>>12)&1u)<<16);
    descriptor[2]=word(part,16);
    for(unsigned k=0;k<3;++k){
        descriptor[4+k]=word(part,20+4*k);
        descriptor[8+k]=word(part,32+4*k);
    }
    descriptor[12]=1;set_real(descriptor,52,1.f);
    unsigned type=((unsigned (*)(unsigned,void *,const void *,float *))(main_base+0x16de70))
        (1,target,descriptor,posed);
    if(type<1||type>3||!(posed[8]>0))return 0;
    for(unsigned k=0;k<3;++k)
        p[k]=type==1?posed[k]:posed[k]+(posed[4+k]-posed[k])*w->fraction;
    return 1;
}
static void world_wound_anchor(WWound *w,void *manager,void *target,void *status,
                               const void *part,const float *point)
{
    w->part_table=0;
    const void *table=world_wound_part_table(target);
    if(!table||(uptr)part<(uptr)table)return;
    unsigned offset=(uptr)part-(uptr)table,index=offset/44;
    if(offset%44)return;
    const void *v=world_wound_volume(manager,status,index);
    if(!v)return;
    float fraction=0;
    if(byte(v,0)!=1){
        float dot=0,length2=0;
        for(unsigned k=0;k<3;++k){
            float start=real(v,8+4*k),axis=real(v,20+4*k)-start;
            dot+=(point[k]-start)*axis;length2+=axis*axis;
        }
        if(length2>.0001f)fraction=dot/length2;
        if(fraction<0)fraction=0;
        if(fraction>1)fraction=1;
    }
    w->part_index=index;w->fraction=fraction;w->part_table=table;
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
        float best=68.f*68.f;
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
        w->progress=0;w->until=0;w->recover=world_elapsed+12;w->flash=world_elapsed+.45f;
        world.wilds_focus_break=1;
        unsigned bonus=25+threshold/4;if(bonus>120)bonus=120;
        ((void (*)(void *,unsigned,int,unsigned))(main_base+0x9d6f8))(target,region,(int)bonus,0);
        wound_recoil.target=target;wound_recoil.status=status;wound_recoil.area=byte(target,AREA);
        wound_recoil.region=region;wound_recoil.until=world_elapsed+.25f;
#if ML_WORLD_PRESENTATION
        world_helm_impact(actor,(const float *)point);
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
        world_wound_anchor(w,manager,target,status,part,(const float *)point);
        w->until=world_elapsed+90;
        log_event("wilds_wound_open",world.actor,(u32)(uptr)target,region,w->progress);
    }
}

static void world_wound_markers(void *a,void *camera)
{
    unsigned aiming=(ml_pad_raw_bits()&PAD_FOCUS)&&(byte(a,DRAWN)&1);
    const void *registry=(const void *)(uptr)word((void *)main_base,0x1851fe0);
    if(!registry||!(real(a,0x1c)>0))return;
    const void *hud=*(void **)(main_base+0x212668c),*render=*(void **)(main_base+0x184d750);
    if(!hud||!render)return;
    unsigned view=byte(hud,236);
    if(view>=4)return;
    const void *viewport=(const void *)((uptr)render+432*view);
    if(word(viewport,84)!=(u32)(uptr)camera||word(viewport,112)<=word(viewport,104)||
       word(viewport,116)<=word(viewport,108))return;
    world_wound_owner(a);
    unsigned marker=0;
    for(unsigned i=0;i<WOUND_MONSTERS&&marker<WOUND_MARKERS;++i){
        WMonsterWounds *m=wilds_wounds+i;
        if(!m->target||word(registry,0x1c+4*i)!=(u32)(uptr)m->target||
           word(m->target,0x1428)!=(u32)(uptr)m->status||u16(m->status,0x1b6)!=m->id||
           (int)word(m->status,0x370)<=0||byte(m->target,AREA)!=byte(a,AREA))continue;
        for(unsigned j=0;j<WOUND_PARTS&&marker<WOUND_MARKERS;++j){
            WWound *w=m->parts+j;
            unsigned kind=w->flash>world_elapsed?2:aiming&&w->until>world_elapsed?1:0;
            if(!kind)continue;
            float p[3],screen[4];
            if(!world_wound_position(m->target,w,p))continue;
            /* Use the same active viewport projection as the native HUD.
             * It returns a 1280x720 logical position and clip visibility. */
            ((void (*)(float *,const float *,unsigned))(main_base+0x551340))(screen,p,view);
            if(screen[3]>0&&screen[2]>0&&screen[0]>0&&screen[0]<1280&&screen[1]>0&&screen[1]<720){
                unsigned sx=(unsigned)(screen[0]*(4094.0f/1280));
                unsigned sy=(unsigned)((720-screen[1])*(4094.0f/720));
                ml_wilds_wound(marker++,kind<<24|sy<<12|sx);
            }
        }
    }
    for(;marker<WOUND_MARKERS;++marker)ml_wilds_wound(marker,0);
}
