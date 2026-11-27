#pragma once
#include "moveset.h"
#include "world_events.h"
#include "world_inputs.h"
#include "world_controls.h"

static inline float w_scale_integer(unsigned value,float scale)
{
    float converted=(float)value;
#if defined(__arm__)
    /* Keep the conversion and scale separate. Ryujinx A32 rejects the
     * fixed-point VCVT encoding GCC can fuse them into. */
    __asm__("" : "+t"(converted));
#endif
    return converted*scale;
}

enum { W_NONE,W_FORESIGHT,W_THRUST,W_RISE,W_AIR,W_DOWN,W_LAND,W_SHEATHE,W_SMALL,W_BIG,W_SHEATHE_END,W_GROUND,
       /* Wilds actions with dedicated animation slots. */
       W_CHIJIN,W_CHARGE,W_SLIDE,W_RELEASE,W_FOCUS };
enum { W_CRIMSON_MOVING=901,W_CRIMSON_STATIONARY=920,W_CRIMSON_RETREAT=921 };
enum { W_SPIRIT_TWO_FORWARD=922,W_SPIRIT_TWO_LEFT,W_SPIRIT_TWO_RIGHT,W_SPIRIT_TWO_BACK };
enum { W_FOCUS_FLURRY=926,W_FOCUS_LEFT,W_FOCUS_RIGHT,W_FOCUS_BACK,W_CRIMSON_SPIN };
static inline unsigned w_focus_finisher_end(unsigned source)
{
    switch(source){
    case 914:return 35;
    case W_FOCUS_LEFT:case W_FOCUS_RIGHT:return 42;
    case W_FOCUS_BACK:return 37;
    default:return 0;
    }
}
enum { W_CRIMSON_DIRECTIONAL=1,W_CRIMSON_LATERAL };
enum { W_KEEP,W_ENTER,W_NATIVE,W_IDLE,W_ROLL,W_ABORT,W_NATIVE_R,W_MOTION };
enum { W_RED_ROUND=1,W_RED_IAI=2 };
enum { W_VALOR_COUNTER_GAIN=200,W_VALOR_HELM_GAIN=300,W_VALOR_RELEASE_END=106 };
enum { W_RELEASE_SEAT_START=118,W_RELEASE_SEAT_ALIGN=132,W_RELEASE_SHEATHE_FRAME=140,W_RELEASE_RECOVERY_CANCEL=205 };
/* The roundslash archive removes 48 frames from the original GU timeline. */
enum { W_ROUND_TAIL_FRAME=62,W_ROUND_CUT_FRAMES=48,W_ROUND_SHEATHE_FRAME=80,W_ROUND_FRAMES=111 };
typedef struct {
    unsigned actor,epoch,sample,held,waiting,ready;
    float remaining[3];
} WKeys;
void w_keys_tick(WKeys *keys,const MLObservation *o);
typedef struct {
    MLObservation base;
    unsigned state,drawn,alive,grounded,rank,contact,counter,resources_ready;
    unsigned previous_weapon_action;
    unsigned focus_strike,valor,valor_style,valor_move,valor_draw_cancel;
    unsigned focus_camera_heading,has_focus_heading,focus_active;
    unsigned ascendance_active,dividing_stick_heading,rising_sprint;
} WObservation;
typedef struct {
    unsigned kind,move,source,slot,native_action,heading;
    int rank_delta,gauge_delta;
    unsigned gauge_clear,gauge_fill,regen_seconds;
    unsigned valor_gain,valor_foresight_second,valor_flash_second_press;
    unsigned red_recovery,counter_resheathe;
    unsigned motion_contract,crimson_direction,sheathed_roll;
    unsigned valor_slide,valor_slide_heading,valor_slide_input;
    unsigned dividing_step_requested,dividing_step_heading;
    float start,blend,speed;
} WDecision;
typedef struct {
    unsigned actor,epoch,move,source,slot,heading,rank,previous_held,sample;
    WKeys keys;
    WControls controls;
    unsigned input_motion,gu_chord_wait;
    unsigned air_sheathe_sample,air_sheathe_held;
    float air_sheathe_buffer;
    unsigned bound,consumed,queued,queued_mask,queued_source,foresight_gauge;
    struct {
        unsigned motion,action,generation,valor;
    } foresight_press;
    struct {
        unsigned motion,generation,held,pending,exit_sample;
    } round_follow;
    unsigned queued_word,queued_shift,queued_heading;
    unsigned counter_success,hit_success,rank_paid,follow_ready;
    unsigned helm_cost_pending,valor_release;
    unsigned ground_node,main_source,spirit_paid;
    unsigned wilds_tier,wilds_sample,wilds_previous;
    float wilds_elapsed,wilds_buffer,wilds_roll_buffer,wilds_thrust_buffer;
    unsigned crimson_buffer_heading,crimson_buffer_direction,crimson_buffer_source;
    unsigned spirit_crimson_pending,spirit_crimson_held,spirit_crimson_input,spirit_crimson_magnitude,spirit_crimson_repeat;
    unsigned spirit_crimson_action,spirit_crimson_motion,spirit_crimson_guard_r;
    float spirit_crimson_confirm;
    float wilds_gauge_fraction;
    unsigned wilds_roll_mod,wilds_roll_heading,wilds_r_pending;
    unsigned wilds_r_direction,wilds_iai_recovered,wilds_focus_break,iai_focus_locked;
    unsigned iai_follow_keys;
    float iai_follow_wait;
    float wilds_r_wait;
    unsigned wilds_stage,wilds_chain_source,wilds_bridge_sample,wilds_bridge_held,wilds_helm_red;
    float wilds_bridge_hold;
    unsigned wilds_native_three_sample;
    float wilds_charge_carry;
    unsigned wilds_charge_carry_source,wilds_charge_carry_action;
    unsigned wilds_charge_open,wilds_charge_link,wilds_three_exit_r,wilds_three_exit_sample,wilds_native_three_held;
    unsigned wilds_slide_exit_sample,wilds_slide_exit_r,wilds_native_r_spent;
    unsigned red_foresight_chain,valor_foresight,valor_foresight_r,foresight_focus;
    unsigned wilds_direction_pending,wilds_direction_heading,wilds_direction_input;
    float wilds_direction_age;
    float entry_start;
    unsigned turn_seen,turn_sample,turn_origin;
    int turn_delta;
    float turn_age,turn_duration,turn_fraction;
    float queue_until,hold_seconds,last_frame,helm_cost_seconds,solo_age;
} WState;
#if ML_WILDS_MOVES
#define W_SPIRIT_ONE_CRIMSON_FRAME 56.f
enum {W_SPIRIT_CRIMSON_INPUT=1,W_SPIRIT_CRIMSON_NEXT};
#define W_CHARGE_READY_FRAME 180.f
#define W_CHARGE_READY_SECONDS 2.9f
#define W_CHARGE_READY_HOLD .30f
#define W_SPIRIT_FULL_HOLD_SECONDS .18f
#define W_CHARGED_THREE_BRANCH_FRAME 110.f
#define W_CHARGED_THREE_POST_SAMPLES 18u
static inline unsigned w_round_spin_recovery(const WState *s,unsigned sample)
{
    return s->round_follow.exit_sample&&sample-s->round_follow.exit_sample<=18u;
}
static inline float w_charge_gesture_time(const WState *s)
{
    /* Keep the longer threshold for R already spent on the current move. */
    return (s->consumed&ML_R)?.60f:W_SPIRIT_FULL_HOLD_SECONDS;
}
static inline unsigned w_native_three_recovery(const WState *s,unsigned sample)
{
    if(!s->wilds_native_three_sample)return 0;
    unsigned age=sample-s->wilds_native_three_sample;
    return age<=W_CHARGED_THREE_POST_SAMPLES||(age<=44&&s->wilds_native_three_held);
}
static inline unsigned w_charged_three_recovery(const WState *s,unsigned sample)
{
    return s->wilds_three_exit_sample&&
        (s->wilds_three_exit_r==902||sample-s->wilds_three_exit_sample<=W_CHARGED_THREE_POST_SAMPLES);
}
static inline unsigned w_moving_spirit_one(unsigned source)
{
    return source==903||(source>=910&&source<=912);
}
static inline unsigned w_moving_spirit_recovery(const WState *s,unsigned sample)
{
    return s->wilds_slide_exit_sample&&
        (s->wilds_slide_exit_r||sample-s->wilds_slide_exit_sample<=18u);
}
static inline float w_charge_rate(const WState *s)
{
    return s->wilds_stage==3?1.3f:1.f;
}
#endif
const WClip *w_clip(unsigned source);
const WEvent *w_event(unsigned source,float frame);
float w_next_hit_frame(unsigned source,float frame);
enum {W_FLOAT_SPEED,W_FLOAT_TURN,W_FLOAT_CAMERA};
float w_float(unsigned source,unsigned channel,float frame);
unsigned w_source(unsigned motion);
unsigned w_foresight_stage(unsigned source,const WEvent *event);
#if ML_WILDS_MOVES && ML_WORLD_INPUT_VM
WDecision w_foresight_press(WState *s,const WObservation *o);
#endif
unsigned w_action(unsigned move);
unsigned w_valor_draw_cancel(unsigned action,unsigned motion);
unsigned w_valor_chord(unsigned state,unsigned action,unsigned motion,float frame,unsigned held,unsigned focus);
/* GU's weak Spirit I remains available. Later paid links use these
 * thresholds both when the controller decides and when GU replays a tap. */
static inline unsigned w_spirit_link_cost(unsigned action)
{
    switch(action){
    case 18:case 19:case 23:case 24:return 15;
    case 26:case 27:return 20;
    case 30:return 25;
    default:return 0;
    }
}
static inline unsigned w_spirit_charge_source(unsigned source)
{
    return source==104||source==106||source==108||source==110;
}
static inline unsigned w_ordinary_charge_source(unsigned source)
{
    return source==101||source==102||source==103||source==105||source==107||
           source==127||source==128;
}
unsigned w_guild_attack(unsigned action);
unsigned w_guild_source(unsigned state,unsigned action,unsigned motion);
static inline unsigned w_native_spirit_one(unsigned state,unsigned action)
{
    /* Standing Spirit I (16/17) and the powered/weak draw entries (20/21)
     * all reach the same striking clip after their native preparation. */
    return state==16&&(action==16||action==17||action==20||action==21);
}
static inline unsigned w_valor_spirit_one(unsigned state,unsigned action)
{
    return state==16&&action==130;
}
static inline unsigned w_valor_guard_reaction(unsigned state,unsigned action,unsigned motion)
{
    return state==16&&(action==108||action==109)&&motion==0x583;
}
enum { W_VALOR_GUARD_SECOND_ACTION=135 };
static inline unsigned w_valor_third_action(unsigned state,unsigned action)
{
    return state==16&&action==117;
}
static inline unsigned w_valor_alternate_third_action(unsigned state,unsigned action)
{
    return state==16&&(action==137||action==139||action==141);
}
static inline unsigned w_valor_second_action(unsigned state,unsigned action)
{
    /* These native alternate III requests bind the same cut and recovery as II. */
    return (state==16&&(action==116||(action>=W_VALOR_GUARD_SECOND_ACTION&&action<=141)));
}
enum { W_VALOR_RELEASE_SECOND=1,W_VALOR_RELEASE_THIRD,W_VALOR_RELEASE_TAIL };
static inline unsigned w_spirit_one_crimson_source(unsigned state,unsigned action,unsigned motion)
{
    return w_native_spirit_one(state,action)&&w_guild_source(state,action,motion)==104;
}
static inline unsigned w_ground_step_move(unsigned move)
{
    return move>=W_FORESIGHT&&move<=W_FOCUS&&(move<W_RISE||move>W_DOWN);
}
static inline unsigned w_ground_step_height(float drop)
{
    return drop>=-.5f&&drop<=120.f;
}
unsigned w_guild_spirit_opener(unsigned action,unsigned motion,unsigned previous);
unsigned w_private(unsigned state,unsigned action);
/* Native movement action 13 is the uphill run. */
static inline unsigned w_uphill_run(unsigned state,unsigned action)
{
    return state==2&&action==13;
}
static inline unsigned w_red_sprint(const WObservation *o)
{
    return w_uphill_run(o->state,o->base.native_action)||
        (o->state==2&&o->base.native_action==2&&o->rising_sprint);
}
unsigned w_red_draw(const WObservation *o);
#if ML_WILDS_MOVES
unsigned w_sheathed_slide(const WObservation *o);
void w_reserve_spirit_crimson(WState *s,const MLObservation *input);
#endif
unsigned w_native_roll(unsigned state,unsigned action);
void w_reset(WState *s);
WDecision w_wilds_enter(WState *s,const WObservation *o,unsigned move);
WDecision w_art_follow(WState *s,const WObservation *o,unsigned keys);
WDecision w_valor_chain_release(WState *s,const WObservation *o,unsigned from);
WDecision w_art_spirit(WState *s,const WObservation *o,unsigned moving_two,unsigned charge);
WDecision w_iai_press(WState *s,const WObservation *o);
WDecision w_iai_attack(WState *s,const WObservation *o,unsigned key);
unsigned w_iai_ready(const WState *s,const WObservation *o);
WDecision w_debug_enter(WState *s,const WObservation *o,unsigned move);
#if ML_ASCENDANCE_PROTOTYPE
unsigned w_wilds_fade_action(const WObservation *o);
WDecision w_wilds_round(WState *,const WObservation *);
WDecision w_wilds_round_charge(WState *,const WObservation *);
WDecision w_dividing_round(WState *s,const WObservation *o);
WDecision w_dividing_spirit_attack(WState *s,const WObservation *o,unsigned keys);
WDecision w_dividing_sheathe(WState *s,const WObservation *o);
WDecision w_radiance_enter(WState *s,const WObservation *o,unsigned move);
WDecision w_radiance_pursuit_enter(WState *s,const WObservation *o);
WDecision w_valor_radiance_enter(WState *s,const WObservation *o,unsigned slot);
#endif
WDecision w_step(WState *s,const WObservation *o);
WDecision w_leave(WState *s);
void w_input_finish(WState *s,const WObservation *o,WDecision decision);
unsigned w_rotation(WState *s,const WObservation *o,unsigned *heading);
int w_root_heading(unsigned source,float frame);
