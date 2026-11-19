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
enum { W_CRIMSON_DIRECTIONAL=1,W_CRIMSON_LATERAL };
enum { W_KEEP,W_ENTER,W_NATIVE,W_IDLE,W_ROLL,W_ABORT,W_NATIVE_R,W_MOTION };
enum { W_RED_ROUND=1,W_RED_IAI=2 };
typedef struct {
    unsigned actor,epoch,sample,held,waiting,ready;
    float remaining[3];
} WKeys;
void w_keys_tick(WKeys *keys,const MLObservation *o);
typedef struct {
    MLObservation base;
    unsigned state,drawn,alive,grounded,rank,contact,counter,resources_ready;
    unsigned previous_weapon_action;
    unsigned focus_strike,valor,valor_move;
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
    float wilds_gauge_fraction;
    unsigned wilds_roll_mod,wilds_roll_heading,wilds_r_pending;
    unsigned wilds_r_direction,wilds_iai_recovered,wilds_focus_break,focus_finisher,iai_focus_locked;
    unsigned iai_follow_keys;
    float iai_follow_wait;
    float wilds_r_wait;
    unsigned wilds_stage,wilds_chain_source,wilds_bridge_sample,wilds_bridge_held,wilds_helm_red;
    float wilds_bridge_hold;
    unsigned red_foresight_chain,valor_foresight;
    unsigned wilds_direction_pending,wilds_direction_heading,wilds_direction_input;
    float wilds_direction_age;
    float entry_start;
    unsigned turn_seen,turn_sample,turn_origin;
    int turn_delta;
    float turn_age,turn_duration,turn_fraction;
    float queue_until,hold_seconds,last_frame,helm_cost_seconds,solo_age;
} WState;
const WClip *w_clip(unsigned source);
const WEvent *w_event(unsigned source,float frame);
enum {W_FLOAT_SPEED,W_FLOAT_TURN,W_FLOAT_CAMERA};
float w_float(unsigned source,unsigned channel,float frame);
unsigned w_source(unsigned motion);
unsigned w_foresight_stage(unsigned source,const WEvent *event);
unsigned w_action(unsigned move);
unsigned w_valor_draw_cancel(unsigned action,unsigned motion);
unsigned w_valor_chord(unsigned state,unsigned action,unsigned motion,float frame,unsigned held,unsigned focus);
unsigned w_guild_attack(unsigned action);
unsigned w_guild_source(unsigned state,unsigned action,unsigned motion);
unsigned w_guild_spirit_opener(unsigned action,unsigned motion,unsigned previous);
unsigned w_private(unsigned state,unsigned action);
unsigned w_red_draw(const WObservation *o);
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
