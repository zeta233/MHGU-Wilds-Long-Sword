#pragma once
#include "moveset.h"
#include "world_events.h"
#include "world_inputs.h"
#include "world_controls.h"

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
enum { W_RELEASE_RECOVERY_CANCEL=205 };
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
    unsigned focus_strike,valor,valor_move,valor_draw_cancel;
    unsigned focus_camera_heading,has_focus_heading;
} WObservation;
typedef struct {
    unsigned kind,move,source,slot,native_action,heading;
    int rank_delta,gauge_delta;
    unsigned gauge_clear,gauge_fill,regen_seconds;
    unsigned red_recovery;
    unsigned motion_contract,crimson_direction,sheathed_roll;
    unsigned valor_slide,valor_slide_heading,valor_slide_input;
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
    unsigned queued_word,queued_shift,queued_heading;
    unsigned counter_success,hit_success,rank_paid,follow_ready;
    unsigned helm_cost_pending;
    unsigned ground_node,main_source,spirit_paid;
    unsigned wilds_tier,wilds_sample,wilds_previous;
    float wilds_elapsed,wilds_buffer,wilds_roll_buffer;
    unsigned crimson_buffer_heading,crimson_buffer_direction,crimson_buffer_source;
    unsigned spirit_crimson_pending,spirit_crimson_held,spirit_crimson_input,spirit_crimson_magnitude,spirit_crimson_repeat;
    unsigned spirit_crimson_action,spirit_crimson_motion;
    float spirit_crimson_confirm;
    float wilds_gauge_fraction;
    unsigned wilds_roll_mod,wilds_roll_heading,wilds_r_pending;
    unsigned wilds_r_direction,wilds_iai_recovered,wilds_focus_break,iai_focus_locked;
    unsigned iai_follow_keys;
    float iai_follow_wait;
    float wilds_r_wait;
    unsigned wilds_stage,wilds_chain_source,wilds_bridge_sample,wilds_bridge_held,wilds_helm_red;
    float wilds_bridge_hold;
    unsigned wilds_charge_open,wilds_charge_link;
    unsigned red_foresight_chain,valor_foresight;
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
static inline float w_charge_rate(const WState *s)
{
    return s->wilds_stage==3?1.3f:1.f;
}
#endif
const WClip *w_clip(unsigned source);
const WEvent *w_event(unsigned source,float frame);
enum {W_FLOAT_SPEED,W_FLOAT_TURN,W_FLOAT_CAMERA};
float w_float(unsigned source,unsigned channel,float frame);
unsigned w_source(unsigned motion);
unsigned w_foresight_stage(unsigned source,const WEvent *event);
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
static inline unsigned w_spirit_one_crimson_source(unsigned state,unsigned action,unsigned motion)
{
    return (action==16||action==17)&&w_guild_source(state,action,motion)==104;
}
unsigned w_guild_spirit_opener(unsigned action,unsigned motion,unsigned previous);
unsigned w_private(unsigned state,unsigned action);
unsigned w_red_draw(const WObservation *o);
#if ML_WILDS_MOVES
unsigned w_sheathed_slide(const WObservation *o);
void w_reserve_spirit_crimson(WState *s,const MLObservation *input);
#endif
unsigned w_native_roll(unsigned state,unsigned action);
void w_reset(WState *s);
WDecision w_wilds_enter(WState *s,const WObservation *o,unsigned move);
WDecision w_iai_press(WState *s,const WObservation *o);
unsigned w_iai_ready(const WState *s,const WObservation *o);
WDecision w_debug_enter(WState *s,const WObservation *o,unsigned move);
WDecision w_step(WState *s,const WObservation *o);
WDecision w_leave(WState *s);
void w_input_finish(WState *s,const WObservation *o,WDecision decision);
unsigned w_rotation(WState *s,const WObservation *o,unsigned *heading);
int w_root_heading(unsigned source,float frame);
