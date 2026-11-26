#include "moveset.h"
#if ML_ASCENDANCE_PROTOTYPE
#include "radiance_walk_data.h"
#include "enhanced_sheathe_data.h"
#include "radiance_flash_data.h"
#include "radiance_pursuit_data.h"
#include "radiance_pursuit_rates.h"
#include "double_draw_data.h"
#include "double_draw_grips.h"
#include "dividing_stand_data.h"
#include "dividing_step_data.h"
#include "musou_flurry_data.h"
#if ML_ASCENDANCE_PROTOTYPE
#include "musou_entry_data.h"
#include "musou_path_data.h"
#include "musou_second_data.h"
#include "musou_third_data.h"
#include "musou_horizontal_data.h"
#include "musou_spin_data.h"
#include "musou_out_data.h"
#include "musou_return_data.h"
#include "musou_seat_data.h"
#include "musou_rest_data.h"
#include "musou_hold_data.h"
#include "musou_relax_data.h"
enum { MUSOU_RELAX_SLOT=30, MUSOU_HOLD_SLOT=31 };
static unsigned musou_stance_motion(unsigned motion){return motion==0x500+MUSOU_RELAX_SLOT||motion==0x500+MUSOU_HOLD_SLOT;}
#include "musou_group_two_data.h"
#include "musou_late_data.h"
#endif
#endif
#if ML_WORLD_COMBAT
#include "world_core.h"
#endif
typedef uint32_t u32;
typedef uintptr_t uptr;
typedef u32 (*NativeCall)(void *,u32,u32,u32);
typedef uptr (__attribute__((pcs("aapcs-vfp"))) *NativeLayer)(void *,u32,u32,u32,float,float,float);
typedef struct {void *address;u32 length;const void *bytes,*original;unsigned char enabled,pad[3];} PublicPatch;
typedef struct {void *target,*detour,*original,*context;void **cell;unsigned char has_context;} PublicHook;
_Static_assert(sizeof(PublicPatch)==20,"Forge ARM32 Patch ABI");
_Static_assert(sizeof(PublicHook)==24,"Forge ARM32 Hook ABI");
extern PublicPatch forge_patch_create(void *,const void *,u32,u32);
extern void forge_patch_enable(PublicPatch *);
extern void forge_patch_disable(PublicPatch *);
extern void forge_patch_destroy(PublicPatch *);
extern PublicHook forge_hook_create(void *,void *,void **);

enum {VT=0x174cf64,STATE=0x2506,ACTION=0x250a,HEADING=0xfec,TARGET=0x14a0,
      FRAME=0x500,MOTION=0x4b4,AREA=0x1054,HP=0x13d6,DRAWN=0x2749,AIR=0x2514};
static uptr main_base;
static NativeCall original_weapon,original_common1,original_common2,original_request,virtual_request;
static NativeLayer original_layer;
#if ML_ANIMATION_PREVIEW || ML_WORLD_COMBAT
static PublicPatch patches[6];
static NativeCall original_equipment;
#else
static PublicPatch patches[4];
#endif
static u32 patch_count,installed,submitting,update_depth,held,sample_sequence,pad_valid;
#if ML_WILDS_MOVES
static unsigned hunter_art_x_held;
enum { HUNTER_ART_CONTROL_TYPE=2, HUNTER_ART_INPUT_MASK=0xff00u };
#endif
static u32 bind_generation;static void *bind_actor;
static MLState controller;
static u32 seen_actor;
static u32 seen_native=0xffffffffu,seen_motion=0xffffffffu;
static uint16_t u16(const void *p,u32 off){return *(const uint16_t *)((uptr)p+off);}
static u32 word(const void *p,u32 off){return *(const u32 *)((uptr)p+off);}
static float real(const void *p,u32 off){return *(const float *)((uptr)p+off);}
static void put(void *p,u32 off,u32 v){*(u32 *)((uptr)p+off)=v;}
static unsigned byte(const void *p,u32 off){return *(const unsigned char *)((uptr)p+off);}

static unsigned world_resource_path(const char *a,const char *b)
{
    for(unsigned i=0;i<64;++i){char c=a[i]=='\\'?'/':a[i];if(c!=b[i])return 0;if(!c)return 1;}
    return 0;
}
#if ML_TRACE_EVENTS
static u32 logs;
static void log_event(const char *name,u32 a,u32 x,u32 y,u32 z)
{
    if(logs>=12000)return;
    ++logs;
    char b[160];u32 n=0;const char *prefix="ML_REBUILD: ",*hex="0123456789ABCDEF";
    for(u32 i=0;prefix[i];++i)b[n++]=prefix[i];
    for(u32 i=0;name[i]&&n<100;++i)b[n++]=name[i];
    u32 values[]={a,x,y,z};
    for(u32 j=0;j<4;++j){b[n++]=' ';for(int bit=28;bit>=0;bit-=4)b[n++]=hex[(values[j]>>bit)&15];}
    b[n++]='\n';
    register const char *r0 __asm__("r0")=b;register u32 r1 __asm__("r1")=n;
    __asm__ volatile("svc #0x27":"+r"(r0),"+r"(r1)::"memory","cc");
}
#else
#define log_event(...) ((void)0)
#endif
#if ML_WILDS_MOVES
static unsigned hunter_art_x_chord(u32 raw)
{
    unsigned buttons=raw&HUNTER_ART_INPUT_MASK;
    if(buttons!=0x1400u&&buttons!=0x1100u)return 0;
    const void *options=(const void *)(uptr)word((const void *)main_base,0x1897f78);
    if(!options||byte(options,0x446c)!=HUNTER_ART_CONTROL_TYPE)return 0;
    /* Control Type 2 uses ZL+X, or L+X with shoulder roles swapped.
     * The other types leave Focus (ZL) and the attack button independent. */
    return buttons==(byte(options,0x446e)?0x1100u:0x1400u);
}
#endif
static void ml_pad_read(void)
{
    if(!installed)return;
    held=0;pad_valid=0;
    uptr pad=*(uptr *)(main_base+0x2126888);if(!pad||(pad&3))return;
    int index=*(int *)(pad+0x6b60);if(index<0||index>8)return;
    u32 raw=*(u32 *)(pad+(u32)index*0xb50+0x1d0);
    held=((raw&0x1000)?ML_X:0)|((raw&0x2000)?ML_A:0)|((raw&0x200)?ML_R:0)|
         ((raw&0x4000)?ML_B:0)|((raw&0x8000)?ML_Y:0)|((raw&0x800)?ML_ZR:0);
#if ML_WILDS_MOVES
    /* Keep the native shortcut's X out of the combo controller until release. */
    if(!(raw&0x1000u))hunter_art_x_held=0;
    else if(hunter_art_x_chord(raw))hunter_art_x_held=1;
    if(hunter_art_x_held)held&=~ML_X;
#endif
    pad_valid=1;
}
#if ML_WORLD_COMBAT && ML_WILDS_MOVES
static void world_art_poll(void);
#endif
void ml_pad_refresh(void)
{
    ++sample_sequence;
    ml_pad_read();
#if ML_TRACE_EVENTS
    static u32 last_trace;
    if(held!=last_trace){
        log_event("android_combat_pad",sample_sequence,last_trace,held,0);
        last_trace=held;
    }
#endif
#if ML_WORLD_COMBAT && ML_WILDS_MOVES
    world_art_poll();
#endif
}
u32 ml_pad_raw_bits(void)
{
    uptr pad=*(uptr *)(main_base+0x2126888);
    if(!pad||(pad&3))return 0;
    int index=*(int *)(pad+0x6b60);
    if(index<0||index>8)return 0;
    return *(u32 *)(pad+(u32)index*0xb50+0x1d0);
}
static unsigned hunter_style(void *a)
{
    return ((unsigned (*)(void *))(main_base+0x268818))(a);
}
static unsigned valor_active(void *a)
{
    return hunter_style(a)==5&&(word(a,10056)&0x400000u);
}
static unsigned local_supported(void *a)
{
    if(!a||word(a,0)!=main_base+VT||!(word(a,0x13cc)&1))return 0;
#if ML_TEST_ANY_STYLE
    /* test builds: the automation save is not a Guild-style hunter */
    return 1;
#else
    unsigned style=hunter_style(a);
    return style==0 || (ML_WILDS_MOVES && style==5);
#endif
}
#if ML_ASCENDANCE_PROTOTYPE
static unsigned solo_ascendance_rt(void *a)
{
    if(!local_supported(a)||hunter_style(a)!=0||!byte(a,AREA)||byte(a,AREA)==255)return 0;
    const void *quest=(const void *)(uptr)word((const void *)main_base,0x188ad90);
    const void *players=(const void *)(uptr)word((const void *)main_base,0x1885b58);
    if(!quest||!word(quest,0xa4)||!players)return 0;
    /* sPlayer stores four hunters here. Palicoes use separate slots. */
    unsigned found=0;
    for(unsigned i=0;i<4;++i){
        u32 player=word(players,0x1c+4*i);
        if(!player)continue;
        if(player!=(u32)(uptr)a)return 0;
        found=1;
    }
    return found;
}

static unsigned ascendance_pad_bits(void *a)
{
    unsigned raw=ml_pad_raw_bits();
    if(solo_ascendance_rt(a))raw=(raw&~2u)|((raw&0x800u)?2u:0u);
    return raw;
}
#endif
#if ML_ANIMATION_PREVIEW || ML_WORLD_COMBAT
#if ML_ATTACHMENT_PREVIEW || ML_WORLD_COMBAT
#include "attachment_data.h"
#if ML_WILDS_MOVES
#include "wilds_root_data.h"
#if ML_ANDROID_RECOMP
#include "charge_pose_grips_android.h"
#else
#include "charge_pose_grips.h"
#endif
#include "focus_walk_data.h"
static const WildsClip *focus_walk_clip(unsigned motion)
{
    for(unsigned i=0;i<sizeof(focus_walk_clips)/sizeof(*focus_walk_clips);++i)
        if(motion==0x500+focus_walk_clips[i].slot)return focus_walk_clips+i;
    return 0;
}
#endif
static NativeCall original_placement,original_equipment_motion;
/* Native3078b8 only rewrites the local pose when its placement-table pointer
 * changes, and writes nothing when that pointer is null. Preserve its input
 * state across our temporary source attachment, including the cache pointer.
 * Otherwise source312 frame34 reparents the last hand offset onto the back. */
static struct {u32 object,owner,active,pose[12],previous,joint,kind,cache;} attachment_native[2];
static unsigned attachment_pose(void *a,u32 role,float *out,u32 *joint,u32 *closed)
{
    u32 motion=u16(a,MOTION);float frame=real(a,FRAME);
#if ML_ASCENDANCE_PROTOTYPE
    unsigned index=motion-(0x500+DOUBLE_DRAW_BASE);
    unsigned entry=musou_entry_slot(motion-0x500)&&role==9;
    unsigned path=musou_path_slot(motion-0x500)&&role==9;
    unsigned spin=musou_spin_slot(motion-0x500)&&role==9;
    unsigned out_dash=motion>=0x500+33&&motion<=0x500+40&&role==9;
    unsigned rest=motion==0x500+32;
    unsigned hold=motion==0x500+MUSOU_HOLD_SLOT,relax=motion==0x500+MUSOU_RELAX_SLOT;
    unsigned seat=(motion==0x500+41||rest||hold||relax)&&(role==9||role==10);
    unsigned dividing=dividing_slot(motion-0x500)&&role==9;
    if(dividing||index<5||entry||path||spin||out_dash||seat){
        if(!(frame>=0&&frame<10000))return 0;
        unsigned count=dividing?(motion==0x500+DIVIDING_STEP_SLOT?DIVIDING_STEP_FRAMES:DIVIDING_STAND_FRAMES):entry?MUSOU_ENTRY_SAMPLES:path?18:spin?26:out_dash?35:seat?(hold?225:relax?119:rest?179:62):double_draw_spans[index][1],n=(unsigned)frame;
        if(n>=count)n=count-1;
        unsigned next=n+(n+1<count);
        const float *x,*y;
        if(dividing){
            const float (*grip)[7]=motion==0x500+DIVIDING_STEP_SLOT?dividing_step_grip:dividing_grip;
            x=grip[n];y=grip[next];
        }else if(entry){
            unsigned side=motion-0x500-MUSOU_ENTRY_LEFT;
            x=musou_entry_grips[side][n];y=musou_entry_grips[side][next];
        }else if(seat){
            const float (*grips)[2][7]=hold?musou_hold_grips[0]:relax?musou_relax_grips[0]:rest?musou_rest_grips[0]:musou_seat_grips[0];
            x=grips[n][role-9];y=grips[next][role-9];
        }else if(out_dash){
            const float (*grip)[7]=motion==0x500+40?musou_return_grips[0]:musou_out_grips[motion-0x500-33];
            x=grip[n];y=grip[next];
        }else if(spin){
            const float (*grip)[7]=musou_spin_grips[motion-0x500-70];
            x=grip[n];y=grip[next];
        }else if(path&&motion-0x500<=49){
            unsigned slot=motion-0x500;
            const float (*grip)[7]=slot<46?musou_late_grips[slot-42]:musou_group_two_grips[slot-46];
            x=grip[n];y=grip[next];
        }else if(path){
            unsigned i=motion-0x500-53;
            const float (*grip)[7]=i<4?musou_path_grips[i]:i<8?musou_second_grips[i-4]:i<11?musou_third_grips[i-9]:musou_horizontal_grips[i-11];
            x=grip[n];y=grip[next];
        }else{
            unsigned first=double_draw_spans[index][0];
            x=double_draw_grips[first+n][role-9];y=double_draw_grips[first+next][role-9];
        }
        float t=frame-n;if(t>1)t=1;
        float dot=0,length=0;
        for(unsigned k=3;k<7;++k)dot+=x[k]*y[k];
        for(unsigned k=0;k<3;++k)out[k]=x[k]+t*(y[k]-x[k]);
        for(unsigned k=0;k<4;++k){
            out[4+k]=(1-t)*x[3+k]+(dot<0?-t:t)*y[3+k];
            length+=out[4+k]*out[4+k];
        }
        float norm;__asm__("vsqrt.f32 %0, %1":"=t"(norm):"t"(length));
        if(!(norm>.00001f))return 0;
        for(unsigned k=4;k<8;++k)out[k]/=norm;
        out[3]=out[11]=0;out[8]=out[9]=out[10]=1;
        *joint=role==9?12:8;
        *closed=role==9&&((index==4&&frame>=18)||(seat&&frame>=58));
        return 1;
    }
    if(motion==0x500+RADIANCE_WALK_SLOT){motion=0x500+RADIANCE_HOLD_SLOT;frame=RADIANCE_HOLD_FRAME;}
    else if(motion==0x500+ENHANCED_SHEATHE_SLOT)motion=0x500+RADIANCE_HOLD_SLOT;
    else if(motion==0x500+RADIANCE_FLASH_SLOT){motion=0x500+RADIANCE_FLASH_DONOR;frame=radiance_flash_sample(frame);}
#endif
    if((motion>>8)!=5||!(frame>=0&&frame<10000))return 0;
#if ML_WILDS_MOVES
#if ML_ASCENDANCE_PROTOTYPE
    if(radiance_pursuit_pose(motion)){
        unsigned slot=motion-0x500;
        motion=0x500+PURSUIT_WEAPON_SLOT;
        if(slot==PURSUIT_FIRST_SLOT)frame+=PURSUIT_FIRST_OFFSET;
        else if(slot==PURSUIT_SECOND_SLOT){
            /* The first cut still evaluates around donor F40-F41 when the
             * second binds. Ease out of that grip and rejoin its track at F5. */
            float start=PURSUIT_FIRST_OFFSET+PURSUIT_FIRST_BRANCH+1.5f;
            float end=PURSUIT_SECOND_OFFSET+5.f;
            float t=frame/5.f;
            frame=t<1.f?start+(end-start)*t*t:frame+PURSUIT_SECOND_OFFSET;
        }
        else frame=PURSUIT_WEAPON_FRAME;
    }
    const MusouStroke *stroke=musou_clip(motion);
    if(stroke){motion=0x500+stroke->donor;frame+=stroke->begin;}
#endif
    const WildsClip *wc=0;
    for(unsigned i=0;i<sizeof(wilds_clips)/sizeof(*wilds_clips);++i)
        if(wilds_clips[i].slot==motion-0x500){wc=wilds_clips+i;break;}
    const float (*weapon_samples)[7]=wilds_weapon_samples;
    if(!wc){wc=focus_walk_clip(motion);weapon_samples=focus_walk_weapon_samples;}
    if(role==9&&wc){
        /* Once HunterBasic turns the weapon OFF, native placement owns the
         * closed blade on its scabbard. Do not reattach it to the hand. */
        if(wc->source==904&&frame>=W_RELEASE_SHEATHE_FRAME)return 0;
        unsigned n=(unsigned)frame;if(n>=wc->frames)n=wc->frames-1;
        unsigned next=n+1<wc->frames?n+1:n;float t=frame-(float)n;if(t>1)t=1;
        unsigned offset=wc->offset;
        if(wc->source==908||wc->source==915||wc->source==909){
            weapon_samples=charge_pose_grips;
            offset=wc->source==908?CHARGE_GRIP_STANDING:
                wc->source==915?CHARGE_GRIP_MOVING:CHARGE_GRIP_FULL;
        }
        const float *x=weapon_samples[offset+n],*y=weapon_samples[offset+next];
        for(unsigned i=0;i<3;++i)out[i]=x[i]+(y[i]-x[i])*t;out[3]=0;
        float dot=0,length=0;for(unsigned i=3;i<7;++i)dot+=x[i]*y[i];
        for(unsigned i=0;i<4;++i){out[4+i]=x[3+i]+((dot<0?-1:1)*y[3+i]-x[3+i])*t;length+=out[4+i]*out[4+i];}
        float norm;__asm__("vsqrt.f32 %0, %1":"=t"(norm):"t"(length));
        if(!(norm>.00001f))return 0;for(unsigned i=0;i<4;++i)out[4+i]/=norm;
        out[8]=out[9]=out[10]=1;out[11]=0;*joint=12;*closed=0;return 1;
    }
#endif
    const MLAttachmentClip *clip=0;
    for(u32 i=0;i<sizeof(ml_attachment_clips)/sizeof(*clip);++i)
        if(ml_attachment_clips[i].slot==motion-0x500){clip=&ml_attachment_clips[i];break;}
    if(!clip)return 0;
    u32 n=(u32)frame;if(n>=clip->frames)n=clip->frames-1;
    *closed=ml_attachment_weapon_motion[clip->offset+n]==0;
    u32 selected=ml_attachment_selection[clip->offset+n][role-9];
    if(!selected)return 0;
    u32 next=n+1<clip->frames?n+1:n;
    const float *x=ml_attachment_samples[clip->offset+n]+(selected-1)*7;
    const float *y=ml_attachment_samples[clip->offset+next]+(selected-1)*7;
    float frac=frame-(float)n;if(frac>1)frac=1;
    for(u32 i=0;i<3;++i)out[i]=x[i]+(y[i]-x[i])*frac;
    out[3]=0;float dot=0;
    for(u32 i=3;i<7;++i)dot+=x[i]*y[i];
    float sign=dot<0?-1:1,len2=0,len;
    for(u32 i=3;i<7;++i){out[i+1]=x[i]+(sign*y[i]-x[i])*frac;len2+=out[i+1]*out[i+1];}
    __asm__("vsqrt.f32 %0, %1":"=t"(len):"t"(len2));
    if(!(len>0.00001f))return 0;
    for(u32 i=4;i<8;++i)out[i]/=len;
    out[8]=out[9]=out[10]=1;out[11]=0;*joint=selected==1?8:12;
    return 1;
}
/* During 312 the authored hand selector disappears at frame 34. Preserve
 * the evaluated equipment transform when crossing to the native back joint.
 * Work in the new joint's local space so turning cannot leave the sword behind. */
static struct {void *object,*owner;u32 epoch;float pose[7],last;} holster[2];
static float att_sqrt(float v){float x;__asm__("vsqrt.f32 %0, %1":"=t"(x):"t"(v));return x;}
/* The scabbard retains the weapon model's actual back placement while
 * drawn. The private timeout has no GU placement-table row of its own. */
static struct {void *owner;unsigned epoch,model;float pose[12];} back_rest[2];
#if ML_WILDS_MOVES
static unsigned ported_round_motion(unsigned motion)
{
    return motion==0x500+148;
}
static unsigned shortened_round(void *a,unsigned motion)
{
    void *resource=(void *)(uptr)word(a,0xf18);
    const void *bank=resource?(void *)(uptr)word(resource,0x64):0;
    unsigned slot=motion-0x500;
    if(!bank||word(bank,0)!=0x00544d4c||slot>=u16(bank,6))return 0;
    const void *descriptor=(const void *)(uptr)word(bank,8+slot*4);
    return descriptor&&word(descriptor,8)==W_ROUND_FRAMES;
}
typedef u32 (__attribute__((pcs("aapcs-vfp"))) *NativeFrameCrossing)(void *,float);
static NativeFrameCrossing original_frame_crossing;
static u32 __attribute__((pcs("aapcs-vfp"))) round_frame_crossing(void *a,float frame)
{
    /* GU's action 30 uses this call site to enter its sheathing phase. */
    if((uptr)__builtin_return_address(0)==main_base+0x119b7f4&&u16(a,MOTION)==0x500+127&&
       shortened_round(a,0x500+127))frame=W_ROUND_SHEATHE_FRAME;
    return original_frame_crossing(a,frame);
}
static void attachment_multiply(const float *a,const float *b,float *out)
{
    for(unsigned i=0;i<3;++i){
        unsigned j=(i+1)%3,k=(i+2)%3;
        out[i]=a[3]*b[i]+b[3]*a[i]+a[j]*b[k]-a[k]*b[j];
    }
    out[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];
}
#if ML_ASCENDANCE_PROTOTYPE
static unsigned enhanced_scabbard_pose(void *a,float *pose,u32 *joint,u32 *closed);
#endif
static unsigned seat_special_sheathe(void *a,float *pose,u32 *joint)
{
    for(unsigned i=0;i<2;++i)
        if(back_rest[i].owner!=a||back_rest[i].epoch!=byte(a,AREA)||
           back_rest[i].model!=word(a,0x23a0+4*i))return 0;
    float sheath[12];u32 parent,closed;
#if ML_ASCENDANCE_PROTOTYPE
    if(u16(a,MOTION)==0x500+ENHANCED_SHEATHE_SLOT){
        if(!enhanced_scabbard_pose(a,sheath,&parent,&closed))return 0;
    }else
#endif
    if(!attachment_pose(a,10,sheath,&parent,&closed))return 0;
    const float *blade=back_rest[0].pose,*rest=back_rest[1].pose;
    float inv[4]={-rest[4],-rest[5],-rest[6],rest[7]};
    if(!(rest[8]>.01f&&rest[9]>.01f&&rest[10]>.01f))return 0;
    float scale=sheath[8]/rest[8];
    float rotation[4],inverse[4],delta[4],step[4],placed[4];
    attachment_multiply(sheath+4,inv,rotation);
    for(unsigned i=0;i<3;++i){delta[i]=(blade[i]-rest[i])*scale;inverse[i]=-rotation[i];}
    delta[3]=0;inverse[3]=rotation[3];
    attachment_multiply(rotation,delta,step);
    attachment_multiply(step,inverse,placed);
    for(unsigned i=0;i<3;++i)pose[i]=sheath[i]+placed[i];
    pose[3]=0.f;
    attachment_multiply(rotation,blade+4,pose+4);
    for(unsigned i=8;i<11;++i)pose[i]=blade[i]*sheath[i]/rest[i];
    pose[11]=blade[11];
    *joint=parent;
    return 1;
}
#endif
static unsigned attachment_relative_pose(const float *p,const float *m,float *v)
{
    if(!p||!m)return 0;
    float rot[9];
    for(unsigned i=0;i<3;++i){
        float x=p[i*4],y=p[i*4+1],z=p[i*4+2],nn=x*x+y*y+z*z;
        if(!(nn>.01f&&nn<100))return 0;
        v[i]=(x*(m[12]-p[12])+y*(m[13]-p[13])+z*(m[14]-p[14]))/nn;
        for(unsigned j=0;j<3;++j)rot[i*3+j]=(x*m[j*4]+y*m[j*4+1]+z*m[j*4+2])/nn;
    }
    float *q=v+3,diag=rot[0]+rot[4]+rot[8];
    if(diag>0){float t=2*att_sqrt(1+diag);q[3]=t*.25f;q[0]=(rot[7]-rot[5])/t;q[1]=(rot[2]-rot[6])/t;q[2]=(rot[3]-rot[1])/t;}
    else {unsigned i=rot[0]>rot[4]?0:1;if(rot[8]>rot[i*3+i])i=2;unsigned j=(i+1)%3,k=(i+2)%3;
        float t=2*att_sqrt(1+rot[3*i+i]-rot[3*j+j]-rot[3*k+k]);if(!(t>.01f))return 0;
        q[i]=t*.25f;q[j]=(rot[3*i+j]+rot[3*j+i])/t;q[k]=(rot[3*i+k]+rot[3*k+i])/t;q[3]=(rot[3*k+j]-rot[3*j+k])/t;}
    return 1;
}
static unsigned equipment_joint_pose(void *a,void *o,unsigned joint,float *v)
{
    const float *parent=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,joint);
    return attachment_relative_pose(parent,(const float *)((uptr)o+0xb0),v);
}
#if ML_WILDS_MOVES
static void attachment_rotate(const float *q,const float *v,float *out)
{
    float point[4]={v[0],v[1],v[2],0},step[4],result[4],inverse[4]={-q[0],-q[1],-q[2],q[3]};
    attachment_multiply(q,point,step);
    attachment_multiply(step,inverse,result);
    for(unsigned i=0;i<3;++i)out[i]=result[i];
}
#if ML_ASCENDANCE_PROTOTYPE
/* Take the scabbard from this weapon's native back mount. Both mounts
 * move with the body while the hand takes over the grip. */
static unsigned enhanced_scabbard_pose(void *a,float *pose,u32 *joint,u32 *closed)
{
    if(!attachment_pose(a,10,pose,joint,closed))return 0;
    if(back_rest[1].owner!=a||back_rest[1].epoch!=byte(a,AREA)||
       back_rest[1].model!=word(a,0x23a4))return 1;
    const float *back=back_rest[1].pose;
    if(!(back[8]>.01f&&back[9]>.01f&&back[10]>.01f))return 1;
    for(unsigned i=8;i<11;++i)pose[i]=back[i];
    float frame=real(a,FRAME);
    if(frame<=5.f){
        for(unsigned i=0;i<12;++i)pose[i]=back[i];
        *joint=2;
    }else if(frame<23.f){
        const float *(*matrix)(void *,u32)=(const float *(*)(void *,u32))(*(uptr **)a)[0x54/4];
        float rel[7],origin[3],rotation[4];
        if(!attachment_relative_pose(matrix(a,*joint),matrix(a,2),rel))return 1;
        attachment_rotate(rel+3,back,origin);
        attachment_multiply(rel+3,back+4,rotation);
        float t=(frame-5.f)/18.f;
        t=t*t*(3.f-2.f*t);
        for(unsigned i=0;i<3;++i){
            origin[i]+=rel[i];
            pose[i]=origin[i]+(pose[i]-origin[i])*t;
        }
        float dot=0,norm=0;
        for(unsigned i=0;i<4;++i)dot+=rotation[i]*pose[4+i];
        for(unsigned i=0;i<4;++i){
            pose[4+i]=rotation[i]*(1.f-t)+pose[4+i]*(dot<0?-t:t);
            norm+=pose[4+i]*pose[4+i];
        }
        if(norm>.00001f){
            float inverse=1.f/att_sqrt(norm);
            for(unsigned i=4;i<8;++i)pose[i]*=inverse;
        }
    }
    return 1;
}

static float enhanced_sheathe_ease(float frame,float start,float end)
{
    if(frame<=start)return 0.f;
    if(frame>=end)return 1.f;
    float t=(frame-start)/(end-start);
    return t*t*(3.f-2.f*t);
}

/* Turn the blade while the hands approach. Once aligned, guide the tip
 * onto the sheath axis and keep that direction through insertion. */
static void enhanced_sheathe_insert(void *a,float *pose,u32 *joint,unsigned closed)
{
    float frame=real(a,FRAME);
    if(frame<52.f)return;
    float seated[12];u32 parent;
    if(!seat_special_sheathe(a,seated,&parent))return;
    if(closed||frame>=80.f){
        for(unsigned i=0;i<12;++i)pose[i]=seated[i];
        *joint=parent;
        return;
    }
    const float *(*matrix)(void *,u32)=(const float *(*)(void *,u32))(*(uptr **)a)[0x54/4];
    float rel[7],origin[3],grip[4],axis[3],forward[]={0,0,1};
    if(!attachment_relative_pose(matrix(a,parent),matrix(a,*joint),rel))return;
    attachment_rotate(rel+3,pose,origin);
    attachment_multiply(rel+3,pose+4,grip);
    attachment_rotate(seated+4,forward,axis);
    float axial=0,dot=0,norm=0;
    for(unsigned i=0;i<3;++i){
        origin[i]+=rel[i];
        axial+=(origin[i]-seated[i])*axis[i];
    }
    for(unsigned i=0;i<4;++i)dot+=grip[i]*seated[4+i];
    float align=enhanced_sheathe_ease(frame,52.f,70.f);
    float guide=enhanced_sheathe_ease(frame,61.f,74.f);
    float slide=1.f-enhanced_sheathe_ease(frame,74.f,80.f);
    for(unsigned i=0;i<3;++i)
        pose[i]=origin[i]+guide*(seated[i]+axis[i]*axial*slide-origin[i]);
    for(unsigned i=0;i<4;++i){
        pose[4+i]=grip[i]+align*((dot<0?-1.f:1.f)*seated[4+i]-grip[i]);
        norm+=pose[4+i]*pose[4+i];
    }
    if(norm>.00001f){
        float inv=1.f/att_sqrt(norm);
        for(unsigned i=4;i<8;++i)pose[i]*=inv;
    }
    *joint=parent;
}
#endif
static void release_wrist_offset(void *a,float *pose,const float *local,float amount)
{
    const float *hand=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,12);
    if(!hand)return;
    const float *root=(const float *)((uptr)a+0xb0);
    float direction[3],axis[3];
    for(unsigned i=0;i<3;++i)
        direction[i]=local[0]*root[i]+local[1]*root[4+i]+local[2]*root[8+i];
    for(unsigned i=0;i<3;++i){
        const float *basis=hand+4*i;
        float length=att_sqrt(basis[0]*basis[0]+basis[1]*basis[1]+basis[2]*basis[2]);
        if(!(length>.01f))return;
        axis[i]=(basis[0]*direction[0]+basis[1]*direction[1]+basis[2]*direction[2])/length;
    }
    float length=att_sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
    if(!(length>.01f))return;
    float scale=amount/length;
    float correction[4]={axis[0]*scale,axis[1]*scale,axis[2]*scale,1},rotation[4];
    attachment_multiply(correction,pose+4,rotation);
    float norm=att_sqrt(1.f+amount*amount);
    for(unsigned i=0;i<4;++i)pose[4+i]=rotation[i]/norm;
}
/* Retain the hand position. Redistribute wrist rotation between the
 * authored hit windows and return to the authored grip before each strike. */
static void release_wrist_pose(void *a,float *pose)
{
    float frame=real(a,FRAME);
    if(frame>33.f&&frame<37.f){
        static const float axis[]={-.36f,.89f,.28f};
        float t=frame<34.7f?(frame-33.f)/1.7f:(37.f-frame)/2.3f;
        release_wrist_offset(a,pose,axis,.04366f*t*t*(3.f-2.f*t));
    }else if(frame>73.5f&&frame<80.5f){
        static const float axis[]={.48f,-.86f,.15f};
        /* Tangent of half the angle, keyed once per frame from F73.5. */
        static const float correction[]={0,.02968f,.06554f,.03055f,-.01746f,.00873f,.00873f,0};
        float position=frame-73.5f;
        unsigned i=(unsigned)position;
        float t=position-i;
        float amount=correction[i]+(correction[i+1]-correction[i])*t*t*(3.f-2.f*t);
        release_wrist_offset(a,pose,axis,amount);
    }
}
/* MOT295 releases the hand constraint at F120 and closes at F140.
 * Begin the visual alignment at F118, then seat along the blade axis. */
static unsigned blend_blade_to_back(void *a,unsigned hand,float *pose,u32 *joint,u32 *closed,
                                    const float *rest,float start,float align,float finish)
{
    float frame=real(a,FRAME);
    if(!(frame>=start&&frame<finish))return 0;
    if(!(rest[8]>.01f&&rest[9]>.01f&&rest[10]>.01f))return 0;
    float aligned[3];
    const float axis[3]={0,0,-18.f};
    attachment_rotate(rest+4,axis,aligned);
    float t=1,depth=0;
    if(frame<align){
        if(!hand)return 0;
        const float *(*matrix)(void *,u32)=(const float *(*)(void *,u32))(*(uptr **)a)[0x54/4];
        float relative[7],point[3],rotation[4];
        if(!attachment_relative_pose(matrix(a,2),matrix(a,12),relative))return 0;
        attachment_rotate(relative+3,pose,point);
        attachment_multiply(relative+3,pose+4,rotation);
        for(unsigned i=0;i<3;++i)pose[i]=relative[i]+point[i];
        for(unsigned i=0;i<4;++i)pose[4+i]=rotation[i];
        t=(frame-start)/(align-start);
        t=t*t*(3-2*t);
        depth=1;
    }else{
        for(unsigned i=0;i<12;++i)pose[i]=rest[i];
        float insert=(frame-align)/(finish-align);
        depth=1-insert*insert*(3-2*insert);
    }
    for(unsigned i=0;i<3;++i)pose[i]+=t*(rest[i]+depth*aligned[i]-pose[i]);
    if(t<1){
        float dot=0,length=0;
        for(unsigned i=0;i<4;++i)dot+=pose[4+i]*rest[4+i];
        for(unsigned i=0;i<4;++i){
            pose[4+i]=pose[4+i]*(1-t)+(dot<0?-t:t)*rest[4+i];
            length+=pose[4+i]*pose[4+i];
        }
        if(!(length>.00001f))return 0;
        float norm=att_sqrt(length);
        for(unsigned i=0;i<4;++i)pose[4+i]/=norm;
        for(unsigned i=8;i<11;++i)pose[i]+=t*(rest[i]-pose[i]);
    }
    *joint=2;*closed=0;
    return 1;
}

static unsigned release_blade_seating(void *a,unsigned hand,float *pose,u32 *joint,u32 *closed)
{
    const void *scabbard=(const void *)(uptr)word(a,0x23a4);
    if(!scabbard||word(scabbard,0x13cc)!=2)return 0;
    return blend_blade_to_back(a,hand,pose,joint,closed,(const float *)((uptr)scabbard+0x40),
                               W_RELEASE_SEAT_START,W_RELEASE_SEAT_ALIGN,W_RELEASE_SHEATHE_FRAME);
}
#endif
#if ML_ASCENDANCE_PROTOTYPE
/* The pursuit donor places the blade on joint 12. Let the native idle
 * socket carry the outgoing world-space blade before it takes full weight. */
static struct {
    void *owner,*blade;
    unsigned area,started,socket_motion,start_heading;
    float grip_progress,yaw_lag;
    unsigned last_heading,rear_turn;
    float initial[7],last,release_blade[16];
    unsigned release;
} pursuit_idle_grip;
static unsigned step_round_grip_offsets(void *a,float *yaw,float *pitch);
/* Preserve the corrected blade pose in joint 12 space at the end of the
 * joint 8 phase. Native placement selects joint 12 at roundslash F53. */
static struct {
    void *actor,*blade;
    unsigned area;
    float frame,position[3],rotation[4];
} step_round_transfer;

static unsigned step_round_receive(void *a,void *blade,float f,unsigned posed,
                                   float *pose,u32 *joint,u32 *closed)
{
    if(step_round_transfer.actor!=a||step_round_transfer.blade!=blade||
       step_round_transfer.area!=byte(a,AREA)||f<53||f>=step_round_transfer.frame+13||
       word(blade,0x13cc)!=12||word(blade,0x13d4)!=0)return posed;
    if(posed&&(*joint!=12||*closed))return posed;
    if(!posed)for(unsigned i=0;i<12;++i)pose[i]=real(blade,0x40+i*4);
    float t=(f-step_round_transfer.frame)/10;
    if(t>1)t=1;
    t=t*t*(3-2*t);
    /* Resolve the larger angular offset over the remaining recovery. */
    float r=(f-step_round_transfer.frame)/13;
    float rotation_weight=r*r*(3-2*r);
    float dot=0,n=0,q[4];
    for(unsigned i=0;i<4;++i)dot+=step_round_transfer.rotation[i]*pose[4+i];
    for(unsigned i=0;i<4;++i){
        q[i]=(1-rotation_weight)*step_round_transfer.rotation[i]+(dot<0?-rotation_weight:rotation_weight)*pose[4+i];
        n+=q[i]*q[i];
    }
    if(!(n>.00001f))return posed;
    n=att_sqrt(n);
    for(unsigned i=0;i<4;++i)pose[4+i]=q[i]/n;
    for(unsigned i=0;i<3;++i)pose[i]=(1-t)*step_round_transfer.position[i]+t*pose[i];
    *joint=12;*closed=0;
    return 1;
}
/* Apply yaw and elevation around the native grip origin. */
static unsigned step_round_grip(void *a,void *blade,unsigned posed,float *pose,u32 *joint,u32 *closed)
{
    float yaw,pitch;
    if(!step_round_grip_offsets(a,&yaw,&pitch))return posed;
    float f=real(a,FRAME);
    unsigned round=u16(a,MOTION)==0x57f;
    if(!round||f<50)step_round_transfer.actor=0;
    if(round&&f>=53)return step_round_receive(a,blade,f,posed,pose,joint,closed);
    if(!posed){
        if(word(blade,0x13cc)!=8||word(blade,0x13d4)!=1)return 0;
        *joint=8;*closed=0;
        for(unsigned i=0;i<12;++i)pose[i]=real(blade,0x40+i*4);
    }
    if((*joint!=8 && !(u16(a,MOTION)==0x500+224 && *joint==12))||*closed)return posed;
    const float *m=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,*joint);
    if(!m)return posed;
    float length=att_sqrt(m[1]*m[1]+m[5]*m[5]+m[9]*m[9]);
    if(!(length>.001f))return posed;
    float up[3]={m[1]/length,m[5]/length,m[9]/length};
    float offset[4]={up[0]*yaw,up[1]*yaw,up[2]*yaw,1},q[4];
    attachment_multiply(offset,pose+4,q);
    float n=att_sqrt(1+yaw*yaw);
    for(unsigned i=0;i<4;++i)pose[4+i]=q[i]/n;
    const float blade_axis[3]={0,0,1};
    float forward[3];
    attachment_rotate(pose+4,blade_axis,forward);
    float axis[3]={forward[1]*up[2]-forward[2]*up[1],
                   forward[2]*up[0]-forward[0]*up[2],
                   forward[0]*up[1]-forward[1]*up[0]};
    float horizontal=att_sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
    if(horizontal>.001f){
        for(unsigned i=0;i<3;++i)offset[i]=axis[i]*pitch/horizontal;
        attachment_multiply(offset,pose+4,q);
        n=att_sqrt(1+pitch*pitch);
        for(unsigned i=0;i<4;++i)pose[4+i]=q[i]/n;
    }
    if(round&&f>=50&&*joint==8){
        const float *dest=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,12);
        float relative[7],offset_position[3];
        if(attachment_relative_pose(dest,m,relative)){
            attachment_rotate(relative+3,pose,offset_position);
            for(unsigned i=0;i<3;++i)step_round_transfer.position[i]=relative[i]+offset_position[i];
            attachment_multiply(relative+3,pose+4,step_round_transfer.rotation);
            step_round_transfer.actor=a;step_round_transfer.blade=blade;
            step_round_transfer.area=byte(a,AREA);step_round_transfer.frame=f;
        }
    }
    return 1;
}
static unsigned pursuit_grip_return(void *a,void *o,unsigned motion,unsigned posed,
                                    float *pose,u32 *joint,u32 *closed)
{
    float f=real(a,FRAME);
    unsigned flash=motion==0x500+RADIANCE_FLASH_SLOT&&f>=RADIANCE_FLASH_FRAMES-4;
    unsigned valor_release_tail=motion==0x500+250&&f>=W_VALOR_RELEASE_END-5&&f<W_VALOR_RELEASE_END+2;
    if(valor_release_tail||flash||(motion==0x500+PURSUIT_SECOND_SLOT&&f>=30)||
       (motion==0x500+PURSUIT_FIRST_SLOT&&f>=PURSUIT_FIRST_BRANCH)){
        pursuit_idle_grip.owner=a;pursuit_idle_grip.blade=o;
        pursuit_idle_grip.area=byte(a,AREA);pursuit_idle_grip.started=0;
        pursuit_idle_grip.release=valor_release_tail;
        if(valor_release_tail)
            for(unsigned i=0;i<16;++i)pursuit_idle_grip.release_blade[i]=real(o,0xb0+4*i);
        return posed;
    }
    unsigned walk=motion==0x504;
    if((motion!=0x501&&!walk)||!(f>=0&&f<(walk?(pursuit_idle_grip.rear_turn?29:12):8))||pursuit_idle_grip.owner!=a||
       pursuit_idle_grip.blade!=o||pursuit_idle_grip.area!=byte(a,AREA)||
       (pursuit_idle_grip.started&&pursuit_idle_grip.socket_motion==motion&&f<pursuit_idle_grip.last)){
        pursuit_idle_grip.owner=0;return posed;
    }
    if(!posed){
        *joint=word(o,0x13cc);*closed=word(o,0x13d4)==2;
        for(unsigned i=0;i<12;++i)pose[i]=real(o,0x40+4*i);
    }
    if(!pursuit_idle_grip.started||pursuit_idle_grip.socket_motion!=motion){
        if(f>1||*joint>=255){
            pursuit_idle_grip.owner=0;return posed;
        }
        const float *before=(const float *)((uptr)o+0xb0);
        if(pursuit_idle_grip.release&&!pursuit_idle_grip.started)before=pursuit_idle_grip.release_blade;
        const float *parent=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,*joint);
        if(!attachment_relative_pose(parent,before,pursuit_idle_grip.initial)){
            pursuit_idle_grip.owner=0;return posed;
        }
        pursuit_idle_grip.started=1;pursuit_idle_grip.socket_motion=motion;
        pursuit_idle_grip.start_heading=pursuit_idle_grip.last_heading=u16(a,HEADING);
        pursuit_idle_grip.grip_progress=0;pursuit_idle_grip.yaw_lag=0;pursuit_idle_grip.rear_turn=0;
    }
    float progress=f;
    if(walk){
        /* Native lateral locomotion turns the body while holding pose F0.
         * Count that motion towards the handoff even before the gait advances. */
        int delta=((u16(a,HEADING)-pursuit_idle_grip.start_heading+32768u)&65535u)-32768;
        /* On the left turn the blade crosses the body just before yaw settles.
         * Finish the socket blend earlier to carry it through the first step. */
        unsigned yaw=u16(a,HEADING);
        int step=(int)((yaw-pursuit_idle_grip.last_heading+32768u)&65535u)-32768;
        if(delta>18204||delta< -18204)pursuit_idle_grip.rear_turn=1;
        if(pursuit_idle_grip.rear_turn&&(step||f!=pursuit_idle_grip.last)){
            /* Follow the final turn around the grip and settle as the feet
             * straighten. Ignore teleports and large heading corrections. */
            if(step>3072||step< -3072)pursuit_idle_grip.yaw_lag=0;
            else pursuit_idle_grip.yaw_lag=.60f*(pursuit_idle_grip.yaw_lag-step);
        }
        pursuit_idle_grip.last_heading=yaw;
        unsigned left=delta>0;
        float turn=(float)(left?delta:-delta)/16384.f;if(turn>1)turn=1;
        progress=left?(f+3*turn)/(1+.2f*turn):(f+4*turn)/(1+turn);
        if(progress<pursuit_idle_grip.grip_progress)progress=pursuit_idle_grip.grip_progress;
        pursuit_idle_grip.grip_progress=progress;
    }
    pursuit_idle_grip.last=f;
    float t=progress/8;if(t>1)t=1;t=t*t*(3-2*t);
    const float *src=pursuit_idle_grip.initial;
    for(unsigned i=0;i<3;++i)pose[i]=src[i]+t*(pose[i]-src[i]);
    float dot=0,len=0;
    for(unsigned i=0;i<4;++i)dot+=src[3+i]*pose[4+i];
    for(unsigned i=0;i<4;++i){
        pose[4+i]=(1-t)*src[3+i]+(dot<0?-t:t)*pose[4+i];
        len+=pose[4+i]*pose[4+i];
    }
    len=att_sqrt(len);for(unsigned i=4;i<8;++i)pose[i]/=len;
    if(walk&&pursuit_idle_grip.rear_turn){
        const float *parent=((const float *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,*joint);
        if(parent){
            int traveled=(int)((u16(a,HEADING)-pursuit_idle_grip.start_heading+32768u)&65535u)-32768;
            if(traveled<0)traveled=-traveled;
            float w=((float)traveled-18204.f)/7282.f;
            if(w<0)w=0;if(w>1)w=1;
            float half=pursuit_idle_grip.yaw_lag*.0000479369f*w*w*(3-2*w);
            float axis_len=att_sqrt(parent[1]*parent[1]+parent[5]*parent[5]+parent[9]*parent[9]);
            if(axis_len>.001f){
                float scale=half/axis_len;
                float yaw_q[4]={parent[1]*scale,parent[5]*scale,parent[9]*scale,1};
                float dest[4];attachment_multiply(yaw_q,pose+4,dest);
                float norm=att_sqrt(1+half*half);
                for(unsigned i=0;i<4;++i)pose[4+i]=dest[i]/norm;
            }
        }
    }
    return 1;
}
#endif
static unsigned holster_pose(void *a,void *o,unsigned ix,unsigned prior,float *out,u32 *joint,u32 *closed)
{
    float f=real(a,FRAME);
    const float start=34,finish=68;
    if(u16(a,MOTION)!=0x5f6||!(f>=start&&f<90)){
        holster[ix].object=0;return 0;
    }
    unsigned rest=back_rest[1].owner==a&&back_rest[1].epoch==byte(a,AREA)&&
        back_rest[1].model==word(a,0x23a4);
    *joint=word(o,0x13cc);*closed=1;
    if(*joint>=255)return 0;
    if(!holster[ix].object&&prior&&f<start+5){
        if(!equipment_joint_pose(a,o,*joint,holster[ix].pose))return 0;
        holster[ix].object=o;holster[ix].owner=a;holster[ix].epoch=byte(a,AREA);holster[ix].last=f;
    }
    unsigned active=holster[ix].object==o&&holster[ix].owner==a&&holster[ix].epoch==byte(a,AREA);
    if(!active||holster[ix].last>f)return 0;
    holster[ix].last=f;
    float t=(f-start)/(finish-start);if(t>1)t=1;t=t*t*(3-2*t);float *v=holster[ix].pose;
    for(unsigned i=0;i<12;++i)out[i]=rest?back_rest[1].pose[i]:real(o,0x40+i*4);
    for(unsigned i=0;i<3;++i)out[i]=v[i]+t*(out[i]-v[i]);
    float dot=0,n=0;for(unsigned i=0;i<4;++i)dot+=v[3+i]*out[4+i];
    for(unsigned i=0;i<4;++i){out[4+i]=v[3+i]*(1-t)+(dot<0?-t:t)*out[4+i];n+=out[4+i]*out[4+i];}
    if(!(n>.001f))return 0;n=att_sqrt(n);for(unsigned i=0;i<4;++i)out[4+i]/=n;
    return 1;
}
#if ML_WILDS_MOVES
static struct {void *actor,*object;unsigned motion,epoch;float pose[7],start[7];unsigned blend;} handoff;
static void blend_entry_grip(void *a,void *o,unsigned motion,float *pose)
{
    float frame=real(a,FRAME);
    unsigned valid=handoff.actor==a&&handoff.object==o&&handoff.epoch==byte(a,AREA);
    unsigned duration=motion==0x500+219?10:8,release=motion==0x500+250,dividing=0;
#if ML_ASCENDANCE_PROTOTYPE
    dividing=dividing_slot(motion-0x500);
    if(dividing)duration=10;
#endif
    if(!valid||motion!=handoff.motion){
        /* Keep the outgoing grip when the move changes the hand-local weapon track. */
        unsigned charged_round=valid&&motion==0x500+219&&
            (handoff.motion==0x500+210||handoff.motion==0x500+211||handoff.motion==0x500+217);
        handoff.blend=(release||charged_round||(motion>=0x554&&motion<=0x557)||dividing)&&frame<duration;
        if(handoff.blend){
            if(valid)for(unsigned i=0;i<7;++i)handoff.start[i]=handoff.pose[i];
            else handoff.blend=equipment_joint_pose(a,o,12,handoff.start);
        }
    }
    if(handoff.blend&&frame>=0&&frame<duration){
        float t=frame/duration;t=t*t*(3-2*t);
        for(unsigned i=0;i<3;++i)pose[i]=handoff.start[i]+(pose[i]-handoff.start[i])*t;
        float dot=0,n=0;
        for(unsigned i=0;i<4;++i)dot+=handoff.start[3+i]*pose[4+i];
        for(unsigned i=0;i<4;++i){
            pose[4+i]=handoff.start[3+i]*(1-t)+pose[4+i]*(dot<0?-t:t);
            n+=pose[4+i]*pose[4+i];
        }
        n=att_sqrt(n);if(n>0)for(unsigned i=0;i<4;++i)pose[4+i]/=n;
    }else handoff.blend=0;
    for(unsigned i=0;i<7;++i)handoff.pose[i]=pose[i+(i>=3)];
    handoff.actor=a;handoff.object=o;handoff.epoch=byte(a,AREA);handoff.motion=motion;
}
#endif
static u32 preview_placement(void *object,u32 owner,u32 native_role,u32 flags)
{
    void *a=(void *)(uptr)owner;
    unsigned role=native_role,scabbard=0;
#if ML_WILDS_MOVES
    if(native_role==25&&local_supported(a)&&word(a,0x23a4)==(u32)(uptr)object){
        role=10;
        scabbard=1;
    }
#endif
    unsigned ix=role==10?1:0;
    unsigned prior=attachment_native[ix].active&&attachment_native[ix].object==(u32)(uptr)object&&attachment_native[ix].owner==owner;
    if((role==9||role==10)&&attachment_native[ix].active){
        if(attachment_native[ix].object==(u32)(uptr)object&&attachment_native[ix].owner==owner){
            for(unsigned i=0;i<12;++i)put(object,0x40+4*i,attachment_native[ix].pose[i]);
            put(object,0x13c8,attachment_native[ix].previous);
            put(object,0x13cc,attachment_native[ix].joint);
            put(object,0x13d4,attachment_native[ix].kind);
            put(object,0x1408,attachment_native[ix].cache);
        }
        attachment_native[ix].active=0;
    }
    float pose[12];u32 joint,closed;
    if((role!=9&&role!=10)||!local_supported(a))return original_placement(object,owner,native_role,flags);
    unsigned motion=u16(a,MOTION),private_pose=0,round_seat=0,round_scabbard=0;
#if ML_WILDS_MOVES
    unsigned charged_round=role==9&&motion==0x500+219;
    round_seat=charged_round&&real(a,FRAME)>=W_ROUND_SHEATHE_FRAME;
    round_scabbard=role==10&&motion==0x500+219;
    if(ported_round_motion(motion)){
        *(uint16_t *)((uptr)a+MOTION)=0x500+127;
        u32 result=original_placement(object,owner,native_role,flags);
        *(uint16_t *)((uptr)a+MOTION)=(uint16_t)motion;
        holster[ix].object=0;
        if(role==9)handoff.actor=0;
        return result;
    }
#endif
#if ML_ASCENDANCE_PROTOTYPE
    unsigned enhanced_sheathe=motion==0x500+ENHANCED_SHEATHE_SLOT;
    if(enhanced_sheathe)*(uint16_t *)((uptr)a+MOTION)=0x500+RADIANCE_HOLD_SLOT;
#endif
    /* The round's sheathed tail settles into the model's own idle mount. */
    if(round_seat)*(uint16_t *)((uptr)a+MOTION)=1;
    u32 result=original_placement(object,owner,native_role,flags);
    if(round_seat)*(uint16_t *)((uptr)a+MOTION)=(uint16_t)motion;
#if ML_ASCENDANCE_PROTOTYPE
    if(enhanced_sheathe)*(uint16_t *)((uptr)a+MOTION)=(uint16_t)motion;
#endif
#if ML_WILDS_MOVES
    private_pose=focus_walk_clip(motion)!=0;
#if ML_ASCENDANCE_PROTOTYPE
    private_pose|=dividing_slot(motion-0x500)||musou_clip(motion)!=0||radiance_pursuit_pose(motion)||musou_entry_slot(motion-0x500);
#endif
    for(unsigned i=0;!private_pose&&i<sizeof(wilds_clips)/sizeof(*wilds_clips);++i)
        private_pose=motion==0x500+wilds_clips[i].slot;
#endif
    if(!private_pose&&!round_seat
#if ML_ASCENDANCE_PROTOTYPE
       &&!musou_stance_motion(motion)&&motion!=0x500+32&&motion!=0x500+41&&!enhanced_sheathe&&!double_draw_motion(motion)
#endif
       &&(role==10||!(byte(a,DRAWN)&1))&&
       word(object,0x13cc)==2&&word(object,0x13d4)==2&&word(object,0x1408)&&
       (motion>>8!=5||motion<0x5e0)){
        back_rest[ix].owner=a;back_rest[ix].epoch=byte(a,AREA);back_rest[ix].model=(u32)(uptr)object;
        for(unsigned i=0;i<12;++i)back_rest[ix].pose[i]=real(object,0x40+4*i);
    }
#if ML_ASCENDANCE_PROTOTYPE
    if(scabbard&&!round_scabbard&&!musou_stance_motion(motion)&&motion!=0x500+32&&motion!=0x500+41&&!double_draw_motion(motion)&&motion!=0x500+230&&motion!=0x500+RADIANCE_WALK_SLOT&&motion!=0x500+ENHANCED_SHEATHE_SLOT)return result;
#else
    if(scabbard&&!round_scabbard&&motion!=0x500+230)return result;
#endif
    unsigned wilds_back=private_pose&&role==10&&back_rest[1].owner==a&&
        back_rest[1].epoch==byte(a,AREA)&&back_rest[1].model==(u32)(uptr)object;
    /* Some GU scabbards have separate drawn and stowed back mounts.
     * Reach the stowed mount before the blade starts sliding in at F74. */
    if(round_scabbard&&(!wilds_back||word(object,0x13cc)!=2||
       real(a,FRAME)<=W_ROUND_TAIL_FRAME))return result;
    if(wilds_back){
        for(unsigned j=0;j<12;++j)pose[j]=back_rest[1].pose[j];
        joint=2;closed=1;
        if(round_scabbard&&real(a,FRAME)<74.f){
            float t=(real(a,FRAME)-W_ROUND_TAIL_FRAME)/(74.f-W_ROUND_TAIL_FRAME);
            t=t*t*(3.f-2.f*t);
            float dot=0,length=0;
            for(unsigned j=4;j<8;++j)dot+=real(object,0x40+4*j)*pose[j];
            for(unsigned j=0;j<12;++j){
                float end=pose[j];
                if(j>=4&&j<8&&dot<0)end=-end;
                pose[j]=(1.f-t)*real(object,0x40+4*j)+t*end;
            }
            for(unsigned j=4;j<8;++j)length+=pose[j]*pose[j];
            if(!(length>.00001f))return result;
            float norm=att_sqrt(length);
            for(unsigned j=4;j<8;++j)pose[j]/=norm;
        }
    }
#if ML_WILDS_MOVES
    if(role==9&&!private_pose)handoff.actor=0;
#endif
    unsigned posed;
#if ML_ASCENDANCE_PROTOTYPE
    if(enhanced_sheathe&&role==10)posed=enhanced_scabbard_pose(a,pose,&joint,&closed);
    else
#endif
    posed=!round_seat&&(wilds_back||attachment_pose(a,role,pose,&joint,&closed));
#if ML_WILDS_MOVES
    if(charged_round&&back_rest[0].owner==a&&back_rest[0].epoch==byte(a,AREA)&&
       back_rest[0].model==(u32)(uptr)object){
        const float *rest=back_rest[0].pose;
        if(round_seat){
            for(unsigned i=0;i<12;++i)pose[i]=rest[i];
            joint=2;closed=1;posed=1;
        }else if(blend_blade_to_back(a,posed,pose,&joint,&closed,rest,
                                    W_ROUND_TAIL_FRAME,74.f,W_ROUND_SHEATHE_FRAME))posed=1;
    }
    if(role==9&&motion==0x500+250){
        if(posed&&joint==12&&!closed)release_wrist_pose(a,pose);
        if(release_blade_seating(a,posed,pose,&joint,&closed))posed=1;
    }
#endif
#if ML_ASCENDANCE_PROTOTYPE
    if(role==9&&(motion==0x500+127||motion==0x500+224))posed=step_round_grip(a,object,posed,pose,&joint,&closed);
    if(role==9)posed=pursuit_grip_return(a,object,motion,posed,pose,&joint,&closed);
#endif
    if(posed)holster[ix].object=0;
    else if(round_seat||!holster_pose(a,object,ix,prior,pose,&joint,&closed))return result;
#if ML_WILDS_MOVES
#if ML_ASCENDANCE_PROTOTYPE
    float seat_begin=motion==0x500+41?52:12;
    if(role==9&&(motion==0x500+DOUBLE_DRAW_RETURN||motion==0x500+41)&&real(a,FRAME)>=seat_begin){
        float seated[12];
        for(unsigned i=0;i<12;++i)seated[i]=pose[i];
        u32 parent=joint;
        seat_special_sheathe(a,seated,&parent);
        if(parent==8){
            float f=real(a,FRAME);
            if(f>=seat_begin+6){
                for(unsigned i=0;i<12;++i)pose[i]=seated[i];
                joint=parent;
            }else{
                const float *(*matrix)(void *,u32)=(const float *(*)(void *,u32))(*(uptr **)a)[0x54/4];
                float rel[7],point[3],q[4];
                if(attachment_relative_pose(matrix(a,parent),matrix(a,joint),rel)){
                    attachment_rotate(rel+3,pose,point);attachment_multiply(rel+3,pose+4,q);
                    float t=(f-seat_begin)/6;t=t*t*(3-2*t);
                    float dot=0,len=0;
                    for(unsigned i=0;i<4;++i)dot+=q[i]*seated[i+4];
                    for(unsigned i=0;i<3;++i)pose[i]=(1-t)*(point[i]+rel[i])+t*seated[i];
                    for(unsigned i=0;i<4;++i){
                        pose[4+i]=(1-t)*q[i]+(dot<0?-t:t)*seated[4+i];
                        len+=pose[4+i]*pose[4+i];
                    }
                    if(len>.00001f){len=att_sqrt(len);for(unsigned i=4;i<8;++i)pose[i]/=len;}
                    joint=parent;
                }
            }
        }
    }
    if(role==9&&motion==0x500+ENHANCED_SHEATHE_SLOT)enhanced_sheathe_insert(a,pose,&joint,closed);
    else if(role==9&&closed&&(musou_stance_motion(motion)||motion==0x500+32||motion==0x500+230||motion==0x500+RADIANCE_WALK_SLOT))
        seat_special_sheathe(a,pose,&joint);
#else
    if(role==9&&motion==0x500+230&&closed)seat_special_sheathe(a,pose,&joint);
#endif
    if(role==9&&private_pose&&joint==12&&!closed)blend_entry_grip(a,object,motion,pose);
    else if(role==9)handoff.actor=0;
#endif
    attachment_native[ix].object=(u32)(uptr)object;attachment_native[ix].owner=owner;
    for(unsigned i=0;i<12;++i)attachment_native[ix].pose[i]=word(object,0x40+4*i);
    attachment_native[ix].previous=word(object,0x13c8);
    attachment_native[ix].joint=word(object,0x13cc);
    attachment_native[ix].kind=word(object,0x13d4);
    attachment_native[ix].cache=word(object,0x1408);attachment_native[ix].active=1;
    for(u32 i=0;i<12;++i)*(float *)((uptr)object+0x40+4*i)=pose[i];
    put(object,0x13c8,word(object,0x13cc));put(object,0x13cc,joint);
    u32 kind=role==10||closed?2:1;
#if ML_ASCENDANCE_PROTOTYPE
    if(role==9&&motion==0x57f&&joint==12&&attachment_native[ix].kind==0)kind=0;
#endif
    put(object,0x13d4,kind);put(object,0x1408,0);
    ((void (*)(void *,void *,u32))(*(uptr **)object)[0x48/4])(object,a,joint);
    /* The outer native update still owns weapon animation, visibility,
     * matrix evaluation and its base update. Never run those twice. */
    return result;
}
static u32 preview_equipment_motion(void *object,u32 owner,u32 c,u32 d)
{
    void *a=(void *)(uptr)owner;
    if(word(object,0x13d0)==9&&local_supported(a)){
        unsigned motion=u16(a,MOTION);float frame=real(a,FRAME);
#if ML_WILDS_MOVES
        if(ported_round_motion(motion)||motion==0x500+127){
            /* The native blade layers still use GU's uncut animation. */
            float *clock=(float *)((uptr)a+FRAME);
            if(motion!=0x500+148&&frame>=W_ROUND_TAIL_FRAME&&shortened_round(a,motion))
                *clock=frame+W_ROUND_CUT_FRAMES;
            *(uint16_t *)((uptr)a+MOTION)=0x500+127;
            u32 result=original_equipment_motion(object,owner,c,d);
            *(uint16_t *)((uptr)a+MOTION)=(uint16_t)motion;
            *clock=frame;
            return result;
        }
        unsigned wilds=0;
        for(unsigned i=0;i<sizeof(wilds_clips)/sizeof(*wilds_clips);++i)
            if(motion==0x500+wilds_clips[i].slot){wilds=1;break;}
#if ML_ASCENDANCE_PROTOTYPE
        wilds|=musou_clip(motion)!=0||radiance_pursuit_pose(motion)||double_draw_motion(motion);
#endif
        if(wilds||focus_walk_clip(motion)){
            unsigned closed=(motion==0x500+250&&frame>=W_RELEASE_SHEATHE_FRAME)||
                (motion==0x500+219&&frame>=W_ROUND_SHEATHE_FRAME);
#if ML_ASCENDANCE_PROTOTYPE
            closed|=motion==0x500+DOUBLE_DRAW_RETURN&&frame>=18;
#endif
            if(u16(object,0x5f4)!=0xffffu)
                ((NativeLayer)(main_base+0x954640))(object,1,0xffffu,0,0,0,1);
            if(u16(object,MOTION)!=(closed?0:1))
                ((NativeLayer)(main_base+0x954640))(object,0,closed?0:1,0,0,0,1);
            put(object,0x13c4,5000u+motion-0x500);return 0;
        }
#endif
#if ML_ASCENDANCE_PROTOTYPE
        if(motion==0x500+RADIANCE_WALK_SLOT){motion=0x500+RADIANCE_HOLD_SLOT;frame=RADIANCE_HOLD_FRAME;}
        else if(motion==0x500+ENHANCED_SHEATHE_SLOT)motion=0x500+RADIANCE_HOLD_SLOT;
        else if(motion==0x500+RADIANCE_FLASH_SLOT){motion=0x500+RADIANCE_FLASH_DONOR;frame=radiance_flash_sample(frame);}
#endif
        if((motion>>8)==5&&frame>=0&&frame<10000)
        for(unsigned i=0;i<sizeof(ml_attachment_clips)/sizeof(*ml_attachment_clips);++i){
            const MLAttachmentClip *clip=ml_attachment_clips+i;
            if(clip->slot!=motion-0x500)continue;
            unsigned n=(unsigned)frame;if(n>=clip->frames)n=clip->frames-1;
            int source=ml_attachment_weapon_motion[clip->offset+n];
            if(source<0||source>3)break;
            unsigned target=source<2?(unsigned)source:220u+(unsigned)source-2u;
            if(!((u32 (*)(void *,u32))(main_base+0x953b10))(object,target))break;
            /* Native fallback keeps a second layer101 (drawn pose). Leaving
             * it active overrides our blade scale even when layer0 advances.
             * Match native308d10's missing-secondary handling for private clips;
             * ordinary body motion will restore its own secondary layer. */

            if(u16(object,0x5f4)!=0xffffu)
                ((NativeLayer)(main_base+0x954640))(object,1,0xffffu,0,0,0,1);
            unsigned body=5000u+clip->slot;
            if(source>=2||u16(object,MOTION)!=target||word(object,0x13c4)!=body){
                float age=source<2?0:(float)ml_attachment_weapon_age[clip->offset+n]+frame-(float)n;
                if(age>10)age=10;
                if(u16(object,MOTION)!=target||word(object,0x13c4)!=body)
                    ((NativeLayer)(main_base+0x954640))(object,0,target,0,0,age,1);
                /* Native308bf4 synchronizes a weapon by writing requested
                 * frame4fc and setting4bc bit4, retaining normal rate1.
                 * Source306[73,80) and307..311[0,8) have body/weapon rate1. */
                if(source>=2){
                    *(float *)((uptr)object+0x4fc)=age;
                    put(object,0x4bc,word(object,0x4bc)|0x10u);
                }
            }
            put(object,0x13c4,body);
            return 0;
        }
    }
    return original_equipment_motion(object,owner,c,d);
}
#endif
static u32 preview_equipment(void *object,u32 b,u32 c,u32 d)
{
    void *a=(void *)(uptr)word(object,0x30);u32 role=word(object,0x13d0);
    if((role!=9&&role!=10)||!local_supported(a))return original_equipment(object,b,c,d);
    u32 result=original_equipment(object,b,c,d);
#if ML_WORLD_COMBAT
    return result; /* Combat builds do not need per-bone preview telemetry. */
#else
    static void *objects[2];static u32 motions[2],bins[2];
    u32 ix=role-9,motion=u16(a,MOTION);float frame=real(a,FRAME);
    if((motion>>8)!=5||!(frame>=0&&frame<10000))return result;
    if(motion==0x501&&frame>=1)return result;
    u32 bin=(u32)frame/32;
    if(objects[ix]==object&&motions[ix]==motion&&bins[ix]==bin)return result;
    objects[ix]=object;motions[ix]=motion;bins[ix]=bin;
    log_event("equip_state",(u32)(uptr)object,role,motion,word(object,0x13cc));
    log_event("equip_clock",(u32)(uptr)object,word(a,FRAME),word(object,0x13d4),(u32)(uptr)a);
    log_event("equip_motion",(u32)(uptr)object,word(object,0x4b4),word(object,0x500),word(object,0x110));
    for(u32 i=0;i<4;++i)
        log_event("equip_pose",(u32)(uptr)object,word(object,0x40+i*12),word(object,0x44+i*12),word(object,0x48+i*12));
    if(motion==0x5e6&&bin==3){
        const void *parent=((const void *(*)(void *,u32))(*(uptr **)a)[0x54/4])(a,word(object,0x13cc));
        if(parent)for(u32 i=0;i<4;++i)
            log_event("equip_parent",(u32)(uptr)object,word(parent,i*16),word(parent,i*16+4),word(parent,i*16+8));
        log_event("equip_actor",(u32)(uptr)object,word(a,0x40),word(a,0x44),word(a,0x48));
    }
    return result;
#endif
}
#endif
static unsigned stick_world_heading(void *a)
{
    u32 target=word(a,TARGET);
    ((void (*)(void *))(main_base+0x27f678))(a);
    unsigned heading=u16(a,TARGET);
    put(a,TARGET,target);
    return heading;
}

static MLObservation observe(void *a,unsigned direction)
{
    MLObservation o={0};o.actor=(u32)(uptr)a;
    o.epoch=byte(a,AREA);o.sample_sequence=sample_sequence;
    o.bind_generation=a==bind_actor?bind_generation:0;
    o.native_action=u16(a,ACTION);o.motion=u16(a,MOTION);o.phase=byte(a,5);
    o.held=held;
#if ML_ASCENDANCE_PROTOTYPE
    if((o.held&ML_ZR)&&solo_ascendance_rt(a))o.held&=~ML_ZR;
#endif
    o.gauge=word(a,0x2780);o.heading=u16(a,HEADING);o.input_heading=o.heading;
    o.animation_frame=real(a,FRAME);o.simulation_delta=real(a,0x1c);
    /* Native thrust f_139a5fc calls f_b50bcc(actor+0x4b0): byte6 bit2. */
    o.motion_finished=(byte(a,0x4b6)>>2)&1;
    o.magnitude=(unsigned)(int)(int16_t)u16(a,0x1410);
    if(o.magnitude>128)o.magnitude=0;
    o.eligible=pad_valid&&u16(a,STATE)==16&&(byte(a,DRAWN)&1)&&
        (int16_t)u16(a,HP)>0&&!(word(a,AIR)&0x400);
    if(direction&&o.magnitude>=50)o.input_heading=stick_world_heading(a);
    return o;
}
#if ML_WORLD_COMBAT
#include "world_adapter.inc"
#endif
static u32 request_hook(void *a,u32 state,u32 action,u32 flags)
{
    if(submitting||!local_supported(a))return original_request(a,state,action,flags);
    /* Requests can precede the post-poll hook. Refresh bits without adding
     * another simulation sample or advancing any buffered-key timer. */
    ml_pad_read();
#if ML_WORLD_COMBAT
    if(world_request(a,state,action,flags))return 0;
#if ML_WORLD_INPUT_VM
#if ML_WILDS_MOVES
    if(world_focus_native_input(a,state,action,flags))return 0;
    unsigned aim;
    if(state==16&&w_guild_attack(action)&&(byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&
       world_focus_heading(a,&aim)){
        put(a,HEADING,aim);put(a,TARGET,aim);
    }
#endif
    return original_request(a,state,action,flags);
#endif
#endif
    MLObservation o=observe(a,0);
    if(o.eligible&&o.native_action==4&&o.motion==0x569&&state==16&&
       ((action>=8&&action<=14)||action==28||action==29))
        log_event("native_fade_offer",o.actor,action,(u32)o.animation_frame,word(a,0x2514));
    if(ml_should_filter(&controller,&o,state,action))return 0;
    /* No decisions, input consumption or delayed native requests here. */
    ml_reset(&controller);
    return original_request(a,state,action,flags);
}
#if ML_WORLD_COMBAT && ML_ASCENDANCE_PROTOTYPE
static void *ascendance_native_round_actor;
#endif
static uptr __attribute__((pcs("aapcs-vfp"))) layer_hook(void *a,u32 layer,u32 motion,u32 flags,float blend,float start,float speed)
{
#if ML_ANIMATION_PREVIEW
    /* Disposable visual experiment only. Never enabled in the normal build.
     * The native thrust handler is used as a playback host, not a combat
     * implementation. Only use with the matching event-free preview ARC. */
    static u32 preview_index;
    static const u32 preview_slots[]={224,225,226,227,228,229,230,238,239,240,241,242,243,244,245};
    if(layer==0&&motion!=0xffffu&&local_supported(a)){
        log_event("preview_layer_input",(u32)(uptr)a,motion,u16(a,ACTION),flags);
        if(motion==5105u&&u16(a,ACTION)==4){
            u32 slot=preview_slots[preview_index%15];
            void *resource=(void *)(uptr)word(a,0xf04+5*4);
            u32 bank=resource?word(resource,0x64):0;
            if(bank&&word((void *)(uptr)bank,0)==0x00544d4c&&
               u16((void *)(uptr)bank,6)>slot&&word((void *)(uptr)bank,8+4*slot)){
                motion=5000u+slot;++preview_index;blend=0;
                log_event("preview_bind",(u32)(uptr)a,slot,preview_index,motion);
            }
        }
    }
#endif
#if ML_WORLD_COMBAT
#if ML_WILDS_MOVES
    if(!layer&&motion!=0xffffu&&art_entry_blend.owner==a){
        unsigned valid=art_entry_blend.epoch==byte(a,AREA)&&sample_sequence-art_entry_blend.sample<=8u;
        if(valid&&u16(a,STATE)==art_entry_blend.group&&u16(a,ACTION)==art_entry_blend.action){
            art_entry_blend.owner=0;
            float previous_blend=blend;
            if(blend<6.f){flags|=0x81u;blend=6.f;}
            (void)previous_blend;
            log_event("hunter_art_entry_blend",(u32)(uptr)a,motion,(u32)previous_blend,(u32)blend);
        }else if(!valid)art_entry_blend.owner=0;
    }
#endif
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
    if(!layer&&motion!=0xffffu&&round_walk.actor==a){
        if(round_walk.area==byte(a,AREA)&&sample_sequence-round_walk.sample<=4&&
           u16(a,MOTION)==1u&&motion==3u)round_walk.active=1;
        else round_walk.actor=0;
    }
#endif
    if(!layer && world_idle_actor==a && motion!=0xffffu){
        world_idle_actor=0;
        if(world_idle_epoch==byte(a,AREA)&&u16(a,MOTION)==world_idle_motion){
            log_event("world_idle_bind",(u32)(uptr)a,motion,flags,(u32)blend);
            unsigned release_idle=world_idle_motion==0x500+250;
            unsigned round_idle=world_idle_motion==0x500+219&&motion==1u;
#if ML_ASCENDANCE_PROTOTYPE && ML_WILDS_MOVES
            if(round_idle)
                round_walk=(__typeof__(round_walk)){.actor=a,.area=byte(a,AREA),.sample=sample_sequence};
#endif
            float idle_blend=release_idle?((byte(a,DRAWN)&1)?18.f:12.f):round_idle?6.f:8.f;
            if((motion==5001u||(release_idle&&motion==1u)||round_idle)&&blend<idle_blend){
                flags|=0x81u;blend=idle_blend;
                if(round_idle)log_event("round_idle_blend",(u32)(uptr)a,motion,flags,(u32)blend);
            }
        }
    }
#endif
#if ML_WILDS_MOVES
#if ML_ASCENDANCE_PROTOTYPE
    if(!layer&&motion==42&&release_walk_turn.actor==(u32)(uptr)a&&
       release_walk_turn.epoch==byte(a,AREA)&&!release_walk_turn.source){
        release_run_join=(__typeof__(release_run_join)){.actor=(u32)(uptr)a,.epoch=byte(a,AREA)};
        if(blend<7){
            flags|=0x81u;blend=7;
            log_event("release_walk_blend",(u32)(uptr)a,motion,flags,(u32)blend);
        }
    }
    if(!layer&&motion==4&&u16(a,MOTION)==42&&u16(a,STATE)==2&&u16(a,ACTION)==5&&
       release_run_join.actor==(u32)(uptr)a&&release_run_join.epoch==byte(a,AREA)&&release_run_join.last_sample){
        if(blend<5){flags|=0x81u;blend=5;}
        log_event("release_run_blend",(u32)(uptr)a,motion,flags,(u32)blend);
    }
    if(!layer&&motion!=0xffffu&&step_foresight_round.actor==a){
        if(step_foresight_round.stage==1&&step_foresight_round.epoch==byte(a,AREA)&&
           motion==5127&&start==0&&u16(a,MOTION)==0x500+224&&
           u16(a,STATE)==16&&u16(a,ACTION)==30){
            step_foresight_round.stage=2;step_foresight_round.last_frame=-1;
        }else step_foresight_round.actor=0;
    }
    /* Blend the stepped return pose into the sweep while maintaining hand contact. */
    float round_blend=step_foresight_round.actor==a && step_foresight_round.stage==2 ? 5.f : 6.f;
    if(!layer&&motion==5127&&start==0&&u16(a,MOTION)==0x500+224&&
       u16(a,STATE)==16&&u16(a,ACTION)==30&&local_supported(a)&&blend<round_blend){
        log_event("foresight_round_blend",(u32)(uptr)a,(u32)blend,(u32)round_blend,(u32)real(a,FRAME));
        flags|=0x81;blend=round_blend;
    }
#endif
    /* GU rebinds its roundslash at the sheathing frame in action 31. */
    if(!layer&&motion==5127&&u16(a,STATE)==16&&u16(a,ACTION)==31&&
       start==W_ROUND_SHEATHE_FRAME+W_ROUND_CUT_FRAMES&&shortened_round(a,0x500+127))
        start=W_ROUND_SHEATHE_FRAME;
#if ML_ASCENDANCE_PROTOTYPE
    if(!layer&&motion==5127&&start==0&&u16(a,STATE)==16&&u16(a,ACTION)==30&&local_supported(a)){
        float boost=ascendance_attack_rate(a,109);
        ascendance_native_round_actor=boost>1.f?a:0;
        if(boost>1.f)log_event("ascendance_attack_speed",(u32)(uptr)a,109,127,(u32)(boost*100.f));
    }
#endif
#endif
    uptr result=original_layer(a,layer,motion,flags,blend,start,speed);
    if(layer==0&&motion!=0xffffu&&local_supported(a)){
        if(bind_actor!=a){bind_actor=a;bind_generation=0;}
        ++bind_generation;
    }
    return result;
}
static u32 update(void *a,u32 b,u32 c,u32 d,NativeCall original)
{
    if(update_depth)return original(a,b,c,d);
    if(!local_supported(a)){
#if ML_WORLD_PRESENTATION && ML_WILDS_MOVES && WORLD_AUDIO_WEAPON
        /* Native styles also share the replaced Long Sword weapon banks. */
        if(a&&word(a,0)==main_base+VT&&(word(a,0x13cc)&1)){
            world_audio_live_actor=(u32)(uptr)a;
            world_audio_live_epoch=byte(a,AREA);
            world_audio_load(a);
        }
#endif
        if(controller.actor==(u32)(uptr)a)ml_reset(&controller);
#if ML_WORLD_COMBAT
        if(world.actor==(u32)(uptr)a){
            world_physics_end(a);world_cleanup(a);world_follow_clear();
            world_normal_gauge_tenths=0;
            world_regen=world_helm_regen=world_regen_fraction=world_armor=0;w_reset(&world);
        }
#endif
        return original(a,b,c,d);
    }
    ++update_depth;
    MLObservation o=observe(a,1);
    if(seen_actor!=(u32)(uptr)a){seen_actor=(u32)(uptr)a;
        seen_native=seen_motion=0xffffffffu;
        log_event("local_actor",o.actor,o.native_action,o.motion,o.epoch);}
    if(o.eligible&&(o.native_action!=seen_native||o.motion!=seen_motion)){
        seen_native=o.native_action;seen_motion=o.motion;
        log_event("native_state",o.actor,o.native_action,o.motion,o.bind_generation);
    }
#if ML_WORLD_COMBAT
    if(world_update(a,&o)){--update_depth;return 0;}
#if ML_WORLD_INPUT_VM
    /* The source input VM owns ground requests. Keep unowned native
     * actions updating without also running the legacy GU queue layer. */
    --update_depth;
    u32 native_result=original(a,b,c,d);
#if ML_ASCENDANCE_PROTOTYPE
    if(ascendance_native_round_actor==a){
        if(u16(a,MOTION)==0x500+127&&u16(a,STATE)==16&&u16(a,ACTION)==30&&ascendance_owner(a))
            ascendance_apply_attack_rate(a,1.1f);
        else ascendance_native_round_actor=0;
    }
#endif
#if ML_WILDS_MOVES
    world_release_walk_step(a);
#endif
    return native_result;
#endif
#endif
    MLCommand command=ml_step(&controller,&o);
    if(command.kind==ML_COMMIT){
        log_event("commit",o.actor,command.semantic,command.native_action,command.transaction);
        log_event("direction_snapshot",o.actor,command.heading,command.input_heading,command.magnitude);
        log_event("source_clock",o.actor,o.motion,(u32)o.animation_frame,o.bind_generation);
        put(a,HEADING,command.heading);put(a,TARGET,command.heading);
        ++submitting;virtual_request(a,16,command.native_action,command.native_action>=16?0x8000u:0);--submitting;
    }else if(command.kind==ML_BIND_TIMEOUT)log_event("bind_timeout_released",o.actor,o.native_action,o.motion,o.bind_generation);
    unsigned pending_fade=controller.actor==(u32)(uptr)a&&
        ((controller.queued>=ML_FADE_BACK&&controller.queued<=ML_FADE_RIGHT)||
         (controller.phase==ML_WAIT_BIND&&controller.target>=ML_FADE_BACK&&controller.target<=ML_FADE_RIGHT));
    u32 anchor=command.kind==ML_COMMIT?command.heading:controller.queued_heading;
    if(pending_fade){put(a,HEADING,anchor);put(a,TARGET,anchor);}
    u32 value=original(a,b,c,d);
#if ML_WORLD_COMBAT
    world_loop_heading(a);
#endif
    if(pending_fade&&local_supported(a)&&u16(a,STATE)==16&&
       (u16(a,ACTION)==4||u16(a,ACTION)==8||u16(a,ACTION)==28||u16(a,ACTION)==29)){
        log_event("pending_heading",(u32)(uptr)a,anchor,u16(a,HEADING),u16(a,ACTION));
        put(a,HEADING,anchor);put(a,TARGET,anchor);
    }
    if(local_supported(a)){
        unsigned before=controller.phase;MLObservation after=observe(a,0);
        ml_observe(&controller,&after);
        if(before==ML_WAIT_BIND&&controller.phase==ML_PLAYING)
            log_event("binding_ack",after.actor,controller.current,after.motion,after.bind_generation);
    }
    --update_depth;return value;
}
static u32 weapon(void *a,u32 b,u32 c,u32 d){return update(a,b,c,d,original_weapon);}
static u32 common1(void *a,u32 b,u32 c,u32 d){return update(a,b,c,d,original_common1);}
static u32 common2(void *a,u32 b,u32 c,u32 d){return update(a,b,c,d,original_common2);}
static void restore(void)
{
    for(u32 i=patch_count;i>0;--i){forge_patch_disable(&patches[i-1]);forge_patch_destroy(&patches[i-1]);}
    patch_count=0;
}
u32 ml_install(u32 base)
{
    if(installed)return 1;
    main_base=base;if(!base||(base&0xfff))return 0;
    volatile uptr *vt=(volatile uptr *)(main_base+VT);
    const u32 offsets[]={0x324,0x318,0x31c,0xc4,0x2f8};
    const u32 expected[]={0x11998b0,0x2cbfcc,0x2e5810,0x2a1d8c,0x119f7a0};
    for(u32 i=0;i<5;++i)if(vt[offsets[i]/4]!=main_base+expected[i]){
        log_event("foreign_vtable_refused",0,offsets[i],vt[offsets[i]/4],main_base+expected[i]);return 0;}
    if(*(u32 *)(main_base+0x2ab670)!=0xe92d47f0u||*(u32 *)(main_base+0x2ab674)!=0xe24dd018u)return 0;
#if ML_ATTACHMENT_PREVIEW || ML_WORLD_COMBAT
    if(*(u32 *)(main_base+0x3078b8)!=0xe92d4bf0u||*(u32 *)(main_base+0x3078bc)!=0xe28db018u)return 0;
    if(*(u32 *)(main_base+0x308b3c)!=0xe92d47f0u||*(u32 *)(main_base+0x308b40)!=0xed2d8b02u)return 0;
#endif
    original_weapon=(NativeCall)vt[0x324/4];original_common1=(NativeCall)vt[0x318/4];
    original_common2=(NativeCall)vt[0x31c/4];original_layer=(NativeLayer)vt[0xc4/4];virtual_request=(NativeCall)vt[0x2f8/4];
    uptr detours[]={(uptr)weapon,(uptr)common1,(uptr)common2,(uptr)layer_hook};
    for(u32 i=0;i<4;++i){
        patches[i]=forge_patch_create((void *)(vt+offsets[i]/4),&detours[i],4,0);patch_count=i+1;
        if(patches[i].address)forge_patch_enable(&patches[i]);
        if(!patches[i].enabled||vt[offsets[i]/4]!=detours[i]){restore();return 0;}
    }
#if ML_ANIMATION_PREVIEW || ML_WORLD_COMBAT
    volatile uptr *equipment_vt=(volatile uptr *)(main_base+0x1752088);
    if(equipment_vt[0x28/4]!=main_base+0x3077f0){restore();return 0;}
    original_equipment=(NativeCall)equipment_vt[0x28/4];
    uptr equipment_detour=(uptr)preview_equipment;
    patches[4]=forge_patch_create((void *)(equipment_vt+0x28/4),&equipment_detour,4,0);patch_count=5;
    if(patches[4].address)forge_patch_enable(&patches[4]);
    if(!patches[4].enabled){restore();return 0;}
#endif
#if ML_WORLD_COMBAT && ML_WORLD_PRESENTATION
    volatile uptr *poly=(volatile uptr *)(main_base+0x1790c84+96);
    if(*poly!=main_base+0xab7f2c){restore();return 0;}
    world_original_poly=(int (*)(void *))*poly;
    uptr poly_update=(uptr)world_poly_update;
    patches[5]=forge_patch_create((void *)poly,&poly_update,4,0);patch_count=6;
    if(patches[5].address)forge_patch_enable(&patches[5]);
    if(!patches[5].enabled){restore();return 0;}
#endif
    forge_hook_create((void *)(main_base+0x2ab670),(void *)request_hook,(void **)&original_request);
    if(!original_request){restore();return 0;}
#if ML_ATTACHMENT_PREVIEW || ML_WORLD_COMBAT
    forge_hook_create((void *)(main_base+0x3078b8),(void *)preview_placement,(void **)&original_placement);
    if(!original_placement){restore();return 0;}
    forge_hook_create((void *)(main_base+0x308b3c),(void *)preview_equipment_motion,(void **)&original_equipment_motion);
    if(!original_equipment_motion){restore();return 0;}
#endif
#if ML_WORLD_COMBAT
    if(!world_install()){restore();log_event("world_combat_guard_refused",0,0,0,0);return 0;}
#endif
    installed=1;log_event("clean_backend_installed",0,4,0x2ab670,8);return 1;
}

/* Freestanding C helpers; no allocator and no resource ownership. */
void *memset(void *p,int v,unsigned n){unsigned char *d=p;for(unsigned i=0;i<n;++i)d[i]=(unsigned char)v;return p;}
void *memcpy(void *p,const void *q,unsigned n){unsigned char *d=p;const unsigned char *s=q;for(unsigned i=0;i<n;++i)d[i]=s[i];return p;}
