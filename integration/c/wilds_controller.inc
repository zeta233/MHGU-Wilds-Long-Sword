#if ML_WILDS_MOVES
static WDecision wilds_phase(WState *s,const WObservation *o,unsigned source)
{
    unsigned previous=s->source;
    WDecision d=ground_motion(s,source,previous,o->base.animation_frame,W_MOTION);
    d.blend=4;s->wilds_bridge_hold=0;s->wilds_charge_link=0;s->spirit_crimson_pending=s->spirit_crimson_repeat=0;s->hit_success=0;s->wilds_buffer=s->wilds_roll_buffer=0;s->wilds_r_pending=0;
    s->wilds_thrust_buffer=0;
    s->wilds_charge_carry=0;s->wilds_charge_carry_source=s->wilds_charge_carry_action=0;
    return d;
}
static WDecision wilds_finish(WState *s,unsigned kind,unsigned heading)
{
    WDecision d=w_leave(s);d.kind=kind;d.heading=heading;return d;
}
static WDecision wilds_native(WState *s,const WObservation *o,unsigned action)
{
    WDecision d=wilds_finish(s,W_NATIVE,o->base.heading);d.native_action=action;return d;
}
static WDecision wilds_directional_weak_spirit(WState *s,const WObservation *o)
{
    WDecision d=wilds_native(s,o,spirit_available(o,15)?16:17);
    /* The weak fallback already spent this directional gesture. Holding
     * its R must not reinterpret it as a stationary charge. */
    s->wilds_native_r_spent=o->base.held==ML_R;
    return d;
}
static unsigned wilds_spirit_is_two(unsigned source)
{
    return source>=W_SPIRIT_TWO_FORWARD&&source<=W_SPIRIT_TWO_BACK;
}

static void wilds_spirit_direction(WState *s,const MLObservation *b)
{
    s->wilds_r_direction=b->magnitude>=50;
    if(!s->wilds_r_direction)return;
    s->wilds_direction_heading=b->heading;
    s->wilds_direction_input=b->input_heading;
}

static WDecision wilds_moving_spirit_two(WState *s,const WObservation *o)
{
    unsigned heading=directional_facing(o,s->wilds_direction_input);
    int angle=ml_heading_delta(s->wilds_direction_input,heading);
    unsigned source=angle>24576||angle< -24576?W_SPIRIT_TWO_BACK:
        angle>=8192?W_SPIRIT_TWO_LEFT:angle<=-8192?W_SPIRIT_TWO_RIGHT:W_SPIRIT_TWO_FORWARD;
    WDecision d;
    if(s->move==W_SLIDE)d=wilds_phase(s,o,source);
    else{
        d=enter(s,o,W_SLIDE,0);
        d.source=s->source=source;
        d.slot=s->slot=w_clip(source)->slot;
    }
    d.heading=s->heading=heading;
    s->wilds_elapsed=0;
    if(o->rank!=3)d.gauge_delta-=15;
    return d;
}
/* Guide KIJIN_ONE -> TWO, TWO -> THREE, THREE -> ROUND. A charged
 * release resumes that same stage; focus and moving I retain their own links. */
unsigned w_wilds_fade_action(const WObservation *o)
{
    const MLObservation *b=&o->base;
    int delta=ml_heading_delta(b->input_heading,b->heading);
    if(b->magnitude>=50){
        if(delta>=8192&&delta<=24576)return 28;
        if(delta<=-8192&&delta>=-24576)return 29;
    }
    return o->valor?143:8;
}
static WDecision wilds_valor_spirit_one(WState *s,const WObservation *o)
{
    unsigned moving=s->wilds_r_direction,heading=s->wilds_direction_heading,input=s->wilds_direction_input;
    WDecision d=wilds_native(s,o,130);
    d.valor_slide=moving;d.valor_slide_heading=heading;d.valor_slide_input=input;
    return d;
}

static WDecision wilds_spirit(WState *s,const WObservation *o,unsigned crimson_source)
{
    unsigned crimson=crimson_source!=0;
    if(o->valor&&(crimson||s->move==W_FOCUS))return wilds_valor_spirit_one(s,o);
    if((crimson||s->source==902)&&s->wilds_r_direction){
        WObservation next=*o;
        next.base.heading=s->wilds_direction_heading;next.base.input_heading=s->wilds_direction_input;
        next.base.magnitude=100;
        WDecision d=w_wilds_enter(s,&next,W_SLIDE);
        if(d.kind!=W_KEEP)return d;
    }
    if(crimson_source==907&&o->rank==3&&!s->wilds_r_direction&&!o->base.held&&
       s->spirit_crimson_pending!=W_SPIRIT_CRIMSON_NEXT){
        WDecision d=w_wilds_round(s,o);
        if(d.kind==W_ENTER){
            d.motion_contract=1;
            d.blend=8;
            d.speed=1;
        }
        return d;
    }
    if(s->source==902){
        if(s->wilds_r_direction||!spirit_available(o,25))return (WDecision){0};
        s->wilds_tier=4;
        WDecision d=wilds_phase(s,o,917);
        if(o->rank!=3)d.gauge_delta=-25;
        return d;
    }
    if(s->move==W_SLIDE){
        if(!spirit_available(o,wilds_spirit_is_two(s->source)?20:15))return (WDecision){0};
        if(wilds_spirit_is_two(s->source)){
            /* Moving II occupies the same link as native action 23. */
            unsigned step=s->source==W_SPIRIT_TWO_FORWARD&&s->wilds_r_direction&&o->ascendance_active;
            unsigned heading=s->wilds_direction_input;
            WDecision d=wilds_native(s,o,o->valor?136:26);
            d.dividing_step_requested=step;
            d.dividing_step_heading=heading;
            return d;
        }
        if(s->wilds_r_direction&&spirit_available(o,15))return wilds_moving_spirit_two(s,o);
        return wilds_native(s,o,o->valor?116:23);
    }
    if(s->move==W_FOCUS)return spirit_available(o,20)?wilds_native(s,o,o->valor?117:26):(WDecision){0};
    return wilds_native(s,o,spirit_available(o,15)?16:17);
}
static void wilds_queue_spirit_crimson(WState *s,unsigned stage,unsigned input,unsigned magnitude)
{
    s->spirit_crimson_pending=stage;
    s->spirit_crimson_input=input;
    s->spirit_crimson_magnitude=magnitude;
}
void w_reserve_spirit_crimson(WState *s,const MLObservation *input)
{
    if(s->spirit_crimson_pending)return;
    wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_INPUT,input->input_heading,input->magnitude);
    s->spirit_crimson_action=input->native_action;
    s->spirit_crimson_motion=input->motion;
    s->spirit_crimson_confirm=0;
}
static WDecision wilds_spirit_follow(WState *s,const WObservation *o)
{
    unsigned queued=s->spirit_crimson_pending;
    s->wilds_r_pending=0;
    WDecision d=wilds_spirit(s,o,s->move==W_CHIJIN?s->source:0);
    if(queued==W_SPIRIT_CRIMSON_NEXT&&d.kind==W_NATIVE&&(d.native_action==16||d.native_action==17))
        s->spirit_crimson_pending=queued;
    return d;
}
static WDecision wilds_charge(WState *s,const WObservation *o,unsigned stage)
{
    unsigned inherit=(s->move&&s->source==s->wilds_charge_carry_source)||
        (!s->move&&s->wilds_charge_carry_action&&o->state==16&&
         o->base.native_action==s->wilds_charge_carry_action)||
        (!s->move&&s->wilds_charge_carry_source==902&&
         (w_charged_three_recovery(s,o->base.sample_sequence)||
          (s->wilds_charge_link&&o->base.held==ML_R)));
    float carry=inherit?s->wilds_charge_carry:0;
    WDecision d=w_wilds_enter(s,o,W_CHARGE);
    if(d.kind!=W_ENTER)return d;
    s->wilds_stage=stage;
    s->wilds_elapsed=carry;
    d.motion_contract=1;
    d.blend=6;
    d.start=s->entry_start=carry*(W_CHARGE_READY_FRAME/W_CHARGE_READY_SECONDS);
    d.speed=(W_CHARGE_READY_FRAME/(60.f*W_CHARGE_READY_SECONDS))*w_charge_rate(s);
    return d;
}
WDecision w_wilds_round_charge(WState *s,const WObservation *o)
{
    return wilds_charge(s,o,4);
}
static WDecision wilds_charge_result(WState *s,WDecision d,float elapsed)
{
    if(d.kind!=W_NATIVE&&d.kind!=W_ENTER&&d.kind!=W_MOTION)return d;
    s->wilds_charge_carry=elapsed<W_CHARGE_READY_SECONDS?elapsed:W_CHARGE_READY_SECONDS;
    s->wilds_charge_carry_source=d.kind==W_NATIVE?0:d.source;
    s->wilds_charge_carry_action=d.kind==W_NATIVE?d.native_action:0;
    return d;
}
/* Recovery may end before the hold reaches its threshold. Carry the
 * reserved follow-up across that natural exit while R stays down. */
static WDecision wilds_charge_recover(WState *s,const WObservation *o)
{
    unsigned link=s->wilds_charge_link;
    unsigned charged_three=s->source==902&&!o->valor&&o->grounded;
    unsigned three=charged_three&&o->rank==3;
    float carry=charged_three&&(three||link)&&s->wilds_charge_carry_source==902?s->wilds_charge_carry:0;
    unsigned slide=s->move==W_SLIDE&&w_moving_spirit_one(s->source)&&!o->valor&&o->grounded;
    unsigned slide_r=slide&&s->wilds_r_pending&&s->wilds_r_direction&&!(o->base.held&~ML_R);
    unsigned queued=link&&s->wilds_r_pending&&!o->valor&&o->base.held==ML_R&&
        (s->source==907||s->source==902)?s->source:0;
    if(three&&s->wilds_r_pending&&!(o->base.held&~ML_R))queued=902;
    WDecision d=wilds_finish(s,W_IDLE,o->base.heading);
    s->wilds_charge_link=link;
    s->wilds_three_exit_r=queued;
    s->wilds_three_exit_sample=three?o->base.sample_sequence:0;
    if(carry>0){s->wilds_charge_carry=carry;s->wilds_charge_carry_source=902;}
    s->wilds_slide_exit_sample=slide?o->base.sample_sequence:0;
    s->wilds_slide_exit_r=slide_r;
    if(three||slide)s->wilds_bridge_held=o->base.held;
    return d;
}
WDecision w_wilds_round(WState *s,const WObservation *o)
{
    if(!spirit_available(o,25))return (WDecision){0};
    WDecision d=w_wilds_enter(s,o,W_CHARGE);if(d.kind!=W_ENTER)return d;
    if(o->rank!=3)d.gauge_delta=-25;
    s->wilds_tier=4;s->wilds_stage=4;s->source=917;s->slot=w_clip(917)->slot;
    d.source=s->source;d.slot=s->slot;return d;
}
/* The stock Spirit actions still own their native hit and gauge logic. This
 * bridge adds the source game's hold-R transitions without replacing them. */
static WDecision wilds_bridge(WState *s,const WObservation *o,unsigned source)
{
    const MLObservation *b=&o->base;
    if(s->wilds_bridge_sample==b->sample_sequence)return (WDecision){0};
    unsigned previous=s->wilds_bridge_held;
    unsigned edge=b->held&~previous;
    s->wilds_bridge_sample=b->sample_sequence;s->wilds_bridge_held=b->held;
    float dt=b->simulation_delta>0&&b->simulation_delta<30?b->simulation_delta/60:0;
    unsigned native_carry=s->wilds_charge_carry_action&&o->state==16&&
        b->native_action==s->wilds_charge_carry_action;
    unsigned recovery_carry=source==1&&s->wilds_charge_carry_source==902&&
        (w_charged_three_recovery(s,b->sample_sequence)||(s->wilds_charge_link&&b->held==ML_R));
    if(!s->move&&!native_carry&&!recovery_carry){
        s->wilds_charge_carry=0;
        s->wilds_charge_carry_source=s->wilds_charge_carry_action=0;
    }
    if(b->held!=ML_R)s->wilds_native_r_spent=0;
    if(s->wilds_slide_exit_sample){
        if(source!=1||!o->drawn||!o->grounded||o->valor||!spirit_available(o,15)||
           (b->held&~ML_R)||!w_moving_spirit_recovery(s,b->sample_sequence)){
            s->wilds_slide_exit_sample=s->wilds_slide_exit_r=0;
        }else if(s->wilds_slide_exit_r||((edge&ML_R)&&b->held==ML_R&&b->magnitude>=50)){
            if(!s->wilds_slide_exit_r){
                s->wilds_slide_exit_r=1;s->wilds_r_wait=.10f;
                wilds_spirit_direction(s,b);
            }
            s->wilds_r_wait-=dt;
            if(b->held==ML_R&&s->wilds_r_wait>0)return (WDecision){0};
            return wilds_moving_spirit_two(s,o);
        }
    }
    if(s->wilds_three_exit_sample){
        if(source!=1||!o->drawn||!o->grounded||o->rank!=3||o->valor||
           (b->held&~ML_R)||!w_charged_three_recovery(s,b->sample_sequence)){
            s->wilds_three_exit_sample=s->wilds_three_exit_r=0;
        }else if(s->wilds_three_exit_r==902||(edge&ML_R)){
            if(!s->wilds_three_exit_r){
                s->wilds_three_exit_r=902;s->wilds_bridge_hold=0;
                wilds_spirit_direction(s,b);
            }
            if(b->held==ML_R)s->wilds_bridge_hold+=dt;
            if(s->wilds_bridge_hold>=W_SPIRIT_FULL_HOLD_SECONDS)
                return w_wilds_round_charge(s,o);
            if(b->held==ML_R)return (WDecision){0};
            if(!s->wilds_r_direction)return w_wilds_round(s,o);
            WObservation next=*o;
            next.base.heading=s->wilds_direction_heading;
            next.base.input_heading=s->wilds_direction_input;
            next.base.magnitude=100;
            return w_wilds_enter(s,&next,W_SLIDE);
        }
    }
    unsigned recover=source==1&&o->grounded&&b->held==ML_R&&s->wilds_charge_link;
    unsigned source_changed=source!=s->wilds_chain_source;
    if(source_changed){
        s->wilds_chain_source=source;s->wilds_r_pending=0;s->wilds_charge_open=0;
        s->wilds_red_three_held=0;
        if(!recover){s->wilds_bridge_hold=0;s->wilds_charge_link=0;}
    }
    if(recover){
        s->wilds_bridge_hold+=dt;
        if(s->wilds_three_exit_r&&o->rank==3){
            if(s->wilds_bridge_hold>=W_SPIRIT_FULL_HOLD_SECONDS)return w_wilds_round_charge(s,o);
            return (WDecision){0};
        }
        if(s->wilds_bridge_hold>=.60f)return wilds_charge(s,o,s->wilds_charge_link);
        return (WDecision){0};
    }
    if(b->held!=ML_R||!o->grounded)s->wilds_charge_link=0;
    unsigned ordinary=!s->move&&(source==1||source==101||source==102||source==103||source==105||source==107);
    unsigned opener=ordinary&&(source==1||b->animation_frame>=24);
    if(o->drawn&&opener&&(b->held&(ML_R|ML_B))==(ML_R|ML_B)&&(edge&(ML_R|ML_B))&&
       (source==1||input_stage(w_event(source,b->animation_frame),WI_SHEATHE)==3)){
        s->wilds_direction_pending=0;return enter(s,o,W_SHEATHE,ML_R|ML_B);
    }
    if(!opener||(b->held&(ML_X|ML_A|ML_B|ML_Y|ML_ZR)))s->wilds_direction_pending=0;
    else{
        if((edge&ML_R)&&b->held==ML_R&&b->magnitude>=50){
            s->wilds_direction_pending=1;s->wilds_direction_age=0;
            s->wilds_direction_heading=b->heading;s->wilds_direction_input=b->input_heading;
        }
        if(s->wilds_direction_pending){
            s->wilds_direction_age+=dt;
            if(s->wilds_direction_age>=.10f){
                s->wilds_direction_pending=0;WObservation next=*o;
                next.base.heading=s->wilds_direction_heading;next.base.input_heading=s->wilds_direction_input;
                next.base.magnitude=100;
                WDecision d=w_wilds_enter(s,&next,W_SLIDE);
                if(d.kind==W_KEEP&&o->drawn)return wilds_directional_weak_spirit(s,o);
                return d;
            }
        }
    }
    unsigned red_third=source==108&&!s->move&&!o->valor&&o->rank==3;
    if(!red_third){
        if(b->held!=ML_R)s->wilds_bridge_hold=0;
        else s->wilds_bridge_hold+=dt;
    }
    if(s->move==W_LAND&&s->wilds_helm_red&&b->motion==0x500+s->slot){
        /* Keep a fresh landing tap through the existing F12 release gate. */
        if(b->held&~ML_R)s->wilds_r_pending=0;
        else if((edge&ML_R)&&o->grounded)s->wilds_r_pending=1;
        if(s->wilds_r_pending&&o->grounded&&b->animation_frame>=12){
            s->wilds_r_pending=0;
            return w_wilds_enter(s,o,W_RELEASE);
        }
    }
    if(!o->grounded)return (WDecision){0};
    unsigned spirit=w_spirit_charge_source(source);
    unsigned insertion=source==112||source==307;
    unsigned ordinary_source=!s->move&&w_ordinary_charge_source(source);
    unsigned ordinary_charge=ordinary_source&&b->magnitude<50;
    unsigned allowed=ordinary_charge||(!s->move&&spirit)||insertion;
    unsigned permission=allowed?input_stage(w_event(source,b->animation_frame),WI_R):0;
    if(red_third){
        /* A pure R gesture keeps its hold time until the native cancel opens. */
        if(b->held&~ML_R){
            s->wilds_r_pending=s->wilds_red_three_held=0;
            s->wilds_bridge_hold=0;
        }else if(b->held==ML_R){
            if(!s->wilds_native_r_spent&&((edge&ML_R)||(source_changed&&previous==ML_R))){
                s->wilds_r_pending=0;s->wilds_red_three_held=1;s->wilds_bridge_hold=0;
            }
            if(s->wilds_red_three_held){
                s->wilds_bridge_hold+=dt;
                if(permission)s->wilds_r_pending=1;
            }
        }else s->wilds_red_three_held=0;
        if(permission==3&&s->wilds_r_pending){
            unsigned full=s->wilds_bridge_hold>=W_SPIRIT_FULL_HOLD_SECONDS;
            if(full||!b->held){
                s->wilds_r_pending=s->wilds_red_three_held=0;
                return full?w_wilds_round_charge(s,o):w_wilds_round(s,o);
            }
        }
        return (WDecision){0};
    }
    if(ordinary_source&&!ordinary_charge)s->wilds_charge_link=0;
    if(permission==3)s->wilds_charge_open=1;
    if(allowed&&s->wilds_charge_open&&b->held==ML_R&&!s->wilds_native_r_spent){
        s->wilds_charge_link=source==108?4:(source==106||source==110)?3:2;
        if(s->wilds_bridge_hold>=.60f)return wilds_charge(s,o,s->wilds_charge_link);
    }
    if(ordinary_charge){
        if((edge&ML_R)&&b->held==ML_R&&permission)s->wilds_r_pending=1;
        if(b->held&~ML_R)s->wilds_r_pending=0;
        if(permission==3&&s->wilds_r_pending&&!b->held){
            s->wilds_r_pending=0;
            if(b->native_action==25&&(o->previous_weapon_action==18||o->previous_weapon_action==19||
               o->previous_weapon_action==23||o->previous_weapon_action==24))
                return (WDecision){.kind=W_NATIVE_R,.heading=b->heading};
            return wilds_native(s,o,spirit_available(o,15)?16:17);
        }
    }
    if((!s->move&&spirit)||insertion){
        if((edge&ML_R)&&b->held==ML_R&&permission){
            s->wilds_r_pending=1;s->wilds_r_wait=.28f;
            wilds_spirit_direction(s,b);
        }
        if(b->held&(ML_X|ML_A|ML_B|ML_Y|ML_ZR))s->wilds_r_pending=0;
        if(s->wilds_r_wait>0)s->wilds_r_wait-=dt;
        if(!(b->held&ML_R))s->wilds_r_wait=0;
        if(permission==3){
            if(s->wilds_r_pending&&!(b->held&ML_R)){
                s->wilds_r_pending=0;
                if(source==108)return w_wilds_round(s,o);
                if(source==104&&s->wilds_r_direction&&spirit_available(o,15))
                    return wilds_moving_spirit_two(s,o);
                unsigned next=source==104||source==307?23:26;
                if(!spirit_available(o,w_spirit_link_cost(next)))return (WDecision){0};
                return wilds_native(s,o,next);
            }
        }
    }
    return (WDecision){0};
}
static WDecision wilds_round_spin(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    unsigned cancel=0;
    if(o->state==16&&o->grounded&&o->drawn&&!o->valor_style){
        if(s->source==109&&s->red_foresight_chain)cancel=50;
        else if(s->source==917)cancel=wilds_clip(917)->cancel;
        else if(!s->move&&b->native_action==30&&b->motion==0x500+127)cancel=50;
    }
    if(!cancel){s->round_follow.motion=0;return (WDecision){0};}
    if(s->round_follow.motion!=b->motion||s->round_follow.generation!=b->bind_generation){
        s->round_follow=(__typeof__(s->round_follow)){.motion=b->motion,
            .generation=b->bind_generation,.held=b->held};
    }
    unsigned edge=b->held&~s->round_follow.held;
    s->round_follow.held=b->held;
    if(o->rank!=3||(b->held&~ML_R))s->round_follow.pending=0;
    else if((edge&ML_R)&&b->animation_frame+15>=cancel)s->round_follow.pending=1;
    if(!s->round_follow.pending||b->animation_frame<cancel||b->motion_finished)
        return (WDecision){0};
    WDecision d=enter(s,o,W_CHARGE,ML_R);
    s->red_foresight_chain=1;s->wilds_tier=s->wilds_stage=4;
    d.source=s->source=W_CRIMSON_SPIN;d.slot=s->slot=w_clip(W_CRIMSON_SPIN)->slot;
    return d;
}

static WDecision wilds_red_round_step(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    if(s->wilds_sample==b->sample_sequence)return (WDecision){0};
    s->wilds_sample=b->sample_sequence;
    unsigned edge=b->held&~s->wilds_previous;s->wilds_previous=b->held;
    if(b->animation_frame>=50&&(edge&ML_X)&&b->held==ML_X&&o->rank==3)
        return w_wilds_enter(s,o,W_CHIJIN);
    if(b->animation_frame>=50&&(edge&ML_B)){
        if(b->held&ML_R)return enter(s,o,W_SHEATHE,ML_R|ML_B);
        return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
    }
    if(b->animation_frame>=106||b->motion_finished)return wilds_finish(s,W_IDLE,b->heading);
    return (WDecision){0};
}

/* First KIJIN_CHARGE event of the corresponding WP03 strike. Blue
 * Crimson's R stays with the native Valor guard. Intermediate wound
 * strikes own their forced successor and cannot be interrupted here. */
static unsigned wilds_charge_window(const WState *s,const WObservation *o)
{
    switch(s->source){
    case W_CRIMSON_STATIONARY:return o->valor?0:34;
    case W_CRIMSON_MOVING:return o->valor?0:50;
    case W_CRIMSON_RETREAT:return o->valor?0:90;
    case 906:return o->valor?0:49;
    case 907:return o->valor?0:69;
    case 903:case 910:case 911:return o->valor?0:56;
    case 912:return o->valor?0:60;
    case W_SPIRIT_TWO_FORWARD:return 62;
    case W_SPIRIT_TWO_LEFT:case W_SPIRIT_TWO_RIGHT:return 72;
    case W_SPIRIT_TWO_BACK:return 80;
    case W_CRIMSON_SPIN:return 69;
    case 902:return W_CHARGED_THREE_BRANCH_FRAME;
    case 914:case W_FOCUS_BACK:return o->valor?0:50;
    case W_FOCUS_LEFT:case W_FOCUS_RIGHT:return o->valor?0:60;
    default:return 0;
    }
}

static WDecision wilds_step(WState *s,const WObservation *o)
{
    enum { VALOR_RETURN_GP=4, VALOR_RETURN_COUNTER=8 };
    const MLObservation *b=&o->base;const WildsClip *clip=wilds_clip(s->source);
    if(!clip)return wilds_finish(s,W_ABORT,b->heading);
    if(!o->valor&&(s->wilds_r_pending&(VALOR_RETURN_GP|VALOR_RETURN_COUNTER)))
        s->wilds_r_pending=0;
    float frame=b->animation_frame,dt=0;unsigned pressed=0;
    if(s->wilds_sample!=b->sample_sequence){
        s->wilds_sample=b->sample_sequence;
        pressed=b->held&~s->wilds_previous;s->wilds_previous=b->held;
        if(b->simulation_delta>0&&b->simulation_delta<30)dt=b->simulation_delta/60;
        if(s->wilds_buffer>0)s->wilds_buffer-=dt;
        if(s->wilds_roll_buffer>0)s->wilds_roll_buffer-=dt;
        if(s->wilds_thrust_buffer>0)s->wilds_thrust_buffer-=dt;
        if(s->wilds_r_wait>0)s->wilds_r_wait-=dt;
    }
    unsigned charge_window=wilds_charge_window(s,o);
    unsigned charged_three=s->source==902;
    unsigned fast_full=(charged_three||(s->move==W_CHIJIN&&s->source==907))&&o->rank==3&&!o->valor;
    unsigned repeat=s->spirit_crimson_repeat;
    unsigned valor_moving_one=o->valor&&s->move==W_SLIDE&&w_moving_spirit_one(s->source);
    /* An R carried into the moving cut is spent. Only a new, centered
     * R can charge; a directional R belongs to the next moving cut. */
    unsigned slide_charge=s->move!=W_SLIDE||
        (s->wilds_r_pending&&!s->wilds_r_direction&&b->magnitude<50);
    unsigned holding_charge=charge_window&&b->held==ML_R&&s->spirit_crimson_pending!=W_SPIRIT_CRIMSON_NEXT&&
        !(s->move==W_CHIJIN&&(s->consumed&ML_R))&&slide_charge;
    s->wilds_charge_link=0;
    if(charge_window&&!fast_full){
        if(holding_charge)s->wilds_bridge_hold+=dt;else s->wilds_bridge_hold=0;
        if(holding_charge&&frame>=charge_window){
            s->wilds_charge_link=s->source==902?4:wilds_spirit_is_two(s->source)?3:2;
            if(s->wilds_bridge_hold>=.60f)return wilds_charge(s,o,s->wilds_charge_link);
        }
    }
    if(s->source==908||s->source==909||s->source==915){
        /* MOT279 accepts Iai from frame 0 and branches on [20,50).
         * MOT275 permits Iai while moving. The port also uses the full
         * cancel of stationary MOT277 on its shared standing loop. */
        unsigned start=s->source==908;
        unsigned sheath_keys=b->held==(ML_R|ML_B)&&!o->valor;
        if(sheath_keys&&(pressed&(ML_R|ML_B))&&(!start||frame<50))
            s->queued=W_SHEATHE;
        if(s->queued==W_SHEATHE&&(o->valor||(start&&frame>=50)))s->queued=0;
        if(s->queued==W_SHEATHE){
            if(!start||frame>=20)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            return (WDecision){0};
        }
        if((pressed&ML_B)&&!sheath_keys)return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        /* MOT279 prebuffers a release from frame 0; its slash opens at 30.
         * A second press cannot extend a charge that was already released. */
        if(!s->wilds_r_pending){
            if(s->wilds_elapsed>=W_CHARGE_READY_SECONDS)s->wilds_elapsed+=dt;
            else if(start||s->source==915){
                /* Use the same pose clock for progress and release. Include
                 * the pose reached on the update when R is released. */
                float elapsed=frame>=W_CHARGE_READY_FRAME?W_CHARGE_READY_SECONDS:
                    frame*(W_CHARGE_READY_SECONDS/W_CHARGE_READY_FRAME);
                if(elapsed>s->wilds_elapsed)s->wilds_elapsed=elapsed;
            }else s->wilds_elapsed+=dt*w_charge_rate(s);
        }
        unsigned ready=s->wilds_elapsed>=W_CHARGE_READY_SECONDS;
        if(!(b->held&ML_R))s->wilds_r_pending=1;
        if(s->wilds_r_pending&&start&&frame<30)return (WDecision){0};
        if(s->wilds_r_pending||s->wilds_elapsed+1.e-5f>=W_CHARGE_READY_SECONDS+W_CHARGE_READY_HOLD){
            float elapsed=s->wilds_elapsed;
            s->wilds_tier=ready?4:s->wilds_elapsed<.8f?1:s->wilds_elapsed<1.6f?2:3;
            unsigned stage=s->wilds_stage;
            if(ready)stage=4;
            else if(s->wilds_elapsed>=1.6f&&stage<3)stage=3;
            if(stage==2){
                if(b->magnitude>=50&&spirit_available(o,15)){
                    wilds_spirit_direction(s,b);
                    WDecision d=wilds_moving_spirit_two(s,o);
                    return wilds_charge_result(s,d,elapsed);
                }
                WDecision d=wilds_native(s,o,o->valor?116:23);
                return wilds_charge_result(s,d,elapsed);
            }
            WDecision release=wilds_phase(s,o,stage==4?917:902);
            if(stage==4)release.blend=8;
            return wilds_charge_result(s,release,elapsed);
        }
        WDecision d={0};
        s->wilds_gauge_fraction+=dt*10;
        d.gauge_delta=(int)s->wilds_gauge_fraction;s->wilds_gauge_fraction-=d.gauge_delta;
        unsigned walk=b->magnitude>=50;
        unsigned change=(walk&&s->source!=915&&(!start||frame>=30))||
            (!walk&&s->source==915)||(start&&ready);
        if(change||b->motion_finished||frame>=clip->frames-2){
            unsigned source=walk?915:ready?909:908;
            WDecision next=wilds_phase(s,o,source);
            if(change)next.blend=6;
            if(source==908||source==915){
                /* Standing and walking share the same upper-body charge clock. */
                float elapsed=ready?W_CHARGE_READY_SECONDS:s->wilds_elapsed;
                next.start=s->entry_start=elapsed*(W_CHARGE_READY_FRAME/W_CHARGE_READY_SECONDS);
            }
            next.gauge_delta=d.gauge_delta;
            return next;
        }
        return d;
    }
    /* Match the World kernel's 0.10-second R confirmation, so a human
     * R+B chord cannot be stolen by a leading solo R request. */
    unsigned spirit_link=s->move==W_CHIJIN||s->move==W_SLIDE||s->move==W_FOCUS||s->source==902;
    /* MOT 287 plants the feet and finishes its backward cut at frame 40.
     * Let solo R take over there. */
    unsigned crimson=s->move==W_CHIJIN;
    unsigned valor_return=o->valor&&(s->source==907||
        (s->move==W_FOCUS&&(!s->wilds_focus_break||(s->source!=905&&s->source!=913))));
    unsigned retreat_spirit=crimson&&s->source==W_CRIMSON_RETREAT;
    unsigned crimson_cancel=retreat_spirit?40u:clip->cancel;
    if(crimson){
        unsigned thrust_chord=b->held==(ML_R|ML_X);
        if(thrust_chord&&(pressed&(ML_R|ML_X))&&frame+18>=crimson_cancel){
            s->wilds_thrust_buffer=.35f;
            s->wilds_r_pending=0;
            s->wilds_buffer=0;
        }else if((pressed&~(ML_R|ML_X))||((pressed&ML_X)&&!thrust_chord)){
            s->wilds_thrust_buffer=0;
        }
        // The chord owns the recovery window even after both buttons are released.
        if(o->grounded&&frame>=crimson_cancel&&(thrust_chord||s->wilds_thrust_buffer>0))
            return enter(s,o,W_THRUST,ML_R|ML_X);
    }
    if(valor_return){
        unsigned keys=b->held;
        if((pressed&(ML_R|ML_A))&&keys==(ML_R|ML_A)&&frame+18>=clip->cancel){
            s->wilds_r_pending=VALOR_RETURN_COUNTER;
        }else if((pressed&ML_R)&&keys==ML_R&&(crimson||frame+18>=clip->cancel)){
            s->wilds_r_pending=VALOR_RETURN_GP;s->wilds_r_wait=.10f;
            wilds_spirit_direction(s,b);
        }else if(pressed&~ML_R){
            s->wilds_r_pending=0;
        }
        if(o->grounded&&frame>=clip->cancel){
            if(s->wilds_r_pending==VALOR_RETURN_COUNTER){
                s->wilds_r_pending=0;
                return enter(s,o,W_FORESIGHT,ML_R|ML_A);
            }
            if(s->wilds_r_pending==VALOR_RETURN_GP&&s->wilds_r_wait<=0)
                return wilds_spirit_follow(s,o);
        }
    }
    /* A solo R during a Crimson swing is one queued follow-up. Keep the
     * stage's last attack window intact and take its normal cancel frame. */
    unsigned repeat_x=repeat&&s->wilds_buffer>0&&s->crimson_buffer_source==906;
    unsigned spirit_r=b->held==ML_R||(repeat_x&&b->held==(ML_R|ML_X));
    /* Take input during the strike; spend it at the clip's cancel frame. */
    unsigned moving_strike_open=s->move==W_SLIDE&&
        ((s->source==903&&frame>=30)||(s->source==W_SPIRIT_TWO_FORWARD&&frame>=28));
    if(valor_moving_one&&s->source!=903) moving_strike_open=frame>=(s->source==912?25:26);
    if(!valor_return&&spirit_link&&(pressed&ML_R)&&spirit_r&&(charged_three||crimson||moving_strike_open||frame+18>=clip->cancel)){
        s->wilds_r_pending=1;s->wilds_r_wait=.10f;
        if(fast_full)s->wilds_bridge_hold=0;
        wilds_spirit_direction(s,b);
        if(s->source==W_SPIRIT_TWO_FORWARD&&o->ascendance_active&&b->magnitude*5>128){
            s->wilds_r_direction=1;
            s->wilds_direction_input=o->dividing_stick_heading;
        }
        if(repeat_x){
            wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_NEXT,s->crimson_buffer_heading,s->crimson_buffer_direction?100:0);
            s->wilds_buffer=0;
        }
    }
    if(b->held&(ML_Y|ML_ZR|ML_B))s->wilds_r_pending=0;
    if(!valor_moving_one&&s->move==W_SLIDE&&s->wilds_r_pending&&s->wilds_r_wait>0&&b->held==ML_R&&
       !s->wilds_r_direction&&b->magnitude>=50)wilds_spirit_direction(s,b);
    if(crimson&&!valor_return&&frame<clip->cancel&&(b->held&(repeat?ML_A:ML_X|ML_A)))s->wilds_r_pending=0;
    if(repeat){
        /* X rolled onto a pending Spirit I belongs to that next action. */
        if(b->held&~(ML_R|ML_X))s->spirit_crimson_pending=0;
        else if(s->wilds_r_pending&&(pressed&ML_X)&&s->spirit_crimson_pending!=W_SPIRIT_CRIMSON_NEXT)
            wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_NEXT,b->input_heading,b->magnitude);
    }
    if(charged_three&&frame<90.f&&!holding_charge)s->wilds_r_pending=0;
    if(fast_full){
        if(!s->wilds_r_pending||(b->held&~ML_R)||s->spirit_crimson_pending==W_SPIRIT_CRIMSON_NEXT)
            s->wilds_bridge_hold=0;
        else if(holding_charge){
            s->wilds_bridge_hold+=dt;
            s->wilds_charge_link=4;
        }
        /* Keep a qualified hold if R is released before the cancel opens. */
        float charge_branch=charged_three?W_CHARGED_THREE_BRANCH_FRAME:clip->cancel;
        if(o->grounded&&frame>=charge_branch&&s->wilds_bridge_hold>=W_SPIRIT_FULL_HOLD_SECONDS)
            return w_wilds_round_charge(s,o);
    }
    if(crimson&&frame>=crimson_cancel&&!o->valor&&b->held==(ML_R|ML_B))
        return enter(s,o,W_SHEATHE,ML_R|ML_B);
    if(retreat_spirit&&frame<clip->cancel){
        if(!holding_charge&&frame>=40&&s->wilds_r_pending&&s->wilds_r_wait<=0)
            return wilds_spirit_follow(s,o);
    }
    /* Buffer the final twelve recovery frames so an early evade tap is not
     * swallowed by the native request filter while our action owns the slot. */
    unsigned release_sheathe=s->move==W_RELEASE&&!s->valor_release;
    if((pressed&ML_B)&&!(b->held&ML_ZR)&&frame+12>=clip->cancel){
        s->wilds_roll_buffer=release_sheathe?.5f:.25f;s->wilds_roll_mod=b->held&ML_R;
        s->wilds_roll_heading=b->magnitude>=50?b->input_heading:b->heading;
    }
    if(frame>=clip->cancel&&s->wilds_roll_buffer>0){
        if(release_sheathe&&!s->wilds_roll_mod&&!(b->held&ML_R)){
            /* Keep the buffered roll until the blade seats in the scabbard. */
            if(frame<W_RELEASE_SHEATHE_FRAME&&!b->motion_finished)return (WDecision){0};
            WDecision d=wilds_finish(s,W_ROLL,s->wilds_roll_heading);
            d.sheathed_roll=1;
            return d;
        }
        if(s->wilds_roll_mod||(b->held&ML_R)){
            if(!o->valor)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            s->wilds_roll_buffer=0;
        }else return wilds_finish(s,W_ROLL,s->wilds_roll_heading);
    }
    unsigned finish=frame>=(s->source==917?(s->red_foresight_chain?105:clip->frames-1):clip->recovery)||
        b->motion_finished;
    if(s->source==918&&!o->valor_style){
        enum { FORESIGHT_R_ROUND=1, FORESIGHT_R_STEP=2 };
        unsigned boosted=o->ascendance_active&&o->rank==3&&!o->valor;
        unsigned success=s->counter_success&&o->rank==3&&!o->valor;
        unsigned late=frame+15>=clip->cancel;
        if(b->held&~ML_R)s->wilds_r_pending=0;
        else if((pressed&ML_R)&&b->held==ML_R&&(late||(boosted&&frame>=15))){
            s->wilds_r_pending=(late?FORESIGHT_R_ROUND:0)|
                (boosted&&frame<clip->cancel?FORESIGHT_R_STEP:0);
            s->wilds_r_wait=boosted?.75f:.32f;
            s->wilds_bridge_hold=0;
            wilds_spirit_direction(s,b);
        }
        if(s->wilds_r_pending&&b->held==ML_R)s->wilds_bridge_hold+=dt;
        if(s->wilds_r_pending&&s->wilds_r_wait>0&&success&&
           frame>=(boosted?40:clip->cancel)&&s->wilds_bridge_hold>=W_SPIRIT_FULL_HOLD_SECONDS)
            return w_wilds_round_charge(s,o);
        if((s->wilds_r_pending&FORESIGHT_R_STEP)&&s->wilds_r_wait>0&&
           boosted&&success&&frame>=40&&!b->held){
            WObservation next=*o;
            next.base.magnitude=s->wilds_r_direction?100:0;
            if(s->wilds_r_direction){
                next.base.heading=s->wilds_direction_heading;
                next.base.input_heading=s->wilds_direction_input;
            }
            WDecision d=w_wilds_enter(s,&next,W_SLIDE);
            if(d.kind==W_ENTER){
                d.motion_contract=1;d.blend=6;d.speed=1;
            }
            return d;
        }
        if((s->wilds_r_pending&FORESIGHT_R_ROUND)&&s->wilds_r_wait>0&&frame>=clip->cancel){
            if(success&&b->held==ML_R)return (WDecision){0};
            WDecision d=ground_enter(s,o,5,ML_R);
            if(d.kind==W_ENTER){
                s->red_foresight_chain=1;s->wilds_previous=b->held;s->wilds_sample=b->sample_sequence;
                s->wilds_r_pending=0;
            }
            return d;
        }
    }
    if(s->move==W_CHIJIN){
        /* Queue Crimson II during the retreat sweep and release it after
         * the blade stops. Other retreat follow-ups retain their own timing. */
        unsigned retreat=s->source==W_CRIMSON_RETREAT;
        unsigned buffer_begin=retreat?26:clip->cancel-18;
        if((pressed&ML_X)&&!(b->held&(ML_A|ML_R|ML_ZR))&&frame>=buffer_begin&&!(repeat&&s->wilds_r_pending)){
            s->wilds_buffer=retreat?.75f:.35f;
            s->crimson_buffer_direction=b->magnitude>=50;
            s->crimson_buffer_heading=s->crimson_buffer_direction?b->input_heading:b->heading;
            s->crimson_buffer_source=s->source==906?907:906;
            if(s->source==907)
                s->crimson_buffer_source=crimson_one(o,&s->crimson_buffer_heading,&s->crimson_buffer_direction);
        }
        if(o->valor&&frame>=buffer_begin&&(pressed&(ML_X|ML_A))&&b->held==(ML_X|ML_A)){
            s->wilds_buffer=retreat?.75f:.35f;
            s->crimson_buffer_source=0;
            s->crimson_buffer_direction=w_wilds_fade_action(o);
        }
        unsigned cancel=retreat&&s->crimson_buffer_source?44:clip->cancel;
        /* Give an incoming R the same six-frame confirmation interval
         * before committing Crimson II on a repeating link. */
        if(repeat&&s->crimson_buffer_source==906)cancel+=6;
        if(frame>=cancel&&s->wilds_buffer>0){
            if(o->valor){
                if(!s->crimson_buffer_source)return wilds_native(s,o,s->crimson_buffer_direction);
                /* X may be the first half of a fade slash at the cancel boundary. */
                if(s->wilds_buffer>(retreat?.75f:.35f)-5.f/60.f)return (WDecision){0};
            }
            if(o->rank!=3)return wilds_native(s,o,0);
            s->heading=s->crimson_buffer_heading;
            WDecision d=wilds_phase(s,o,s->crimson_buffer_source);
            d.crimson_direction=s->crimson_buffer_direction;
            return d;
        }
    }
    if(s->move==W_FOCUS&&s->wilds_focus_break){
        if(s->source==905&&frame>=50)return wilds_phase(s,o,913);
        if(s->source==913&&frame>=84){
            unsigned next=914;
            if(b->magnitude>=50){
                int angle=ml_heading_delta(b->input_heading,s->heading);
                next=angle>24576||angle< -24576?W_FOCUS_BACK:
                    angle>=8192?W_FOCUS_LEFT:angle<=-8192?W_FOCUS_RIGHT:914;
            }
            return wilds_phase(s,o,next);
        }
    }
    if(s->move!=W_CHIJIN&&o->rank==3&&o->grounded&&o->drawn&&
       !(s->move==W_FOCUS&&s->wilds_focus_break&&(s->source==905||s->source==913))){
        if(b->held&~ML_X)s->spirit_crimson_pending=0;
        else if(pressed&ML_X)wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_INPUT,b->input_heading,b->magnitude);
        if(s->spirit_crimson_pending&&(frame>=clip->cancel||(s->move==W_RELEASE&&frame>=W_RELEASE_RECOVERY_CANCEL))){
            WObservation next=*o;
            next.base.input_heading=s->spirit_crimson_input;
            next.base.magnitude=s->spirit_crimson_magnitude;
            return w_wilds_enter(s,&next,W_CHIJIN);
        }
    }
    if(s->valor_release&&frame>=W_VALOR_RELEASE_END)
        return wilds_charge_recover(s,o);
    if(frame>=clip->cancel){
        if((pressed&(ML_X|ML_A))&&(b->held&(ML_X|ML_A|ML_R|ML_ZR))==(ML_X|ML_A)){
            return wilds_native(s,o,w_wilds_fade_action(o));
        }
        if((pressed&ML_B)&&!(b->held&ML_ZR)){
            if(b->held&ML_R)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        }
        if(!crimson&&(pressed&(ML_R|ML_X|ML_A))&&(b->held==(ML_R|ML_X)||b->held==(ML_R|ML_A))&&
           !(b->held==(ML_R|ML_X)&&repeat&&s->wilds_r_pending))
            return enter(s,o,b->held&ML_A?W_FORESIGHT:W_THRUST,b->held);
        if(!valor_return&&!holding_charge&&s->wilds_r_pending&&s->wilds_r_wait<=0&&
           !(s->move==W_SLIDE&&!valor_moving_one&&b->held==ML_R&&!s->wilds_r_direction))
            return wilds_spirit_follow(s,o);
        if((pressed&ML_X)&&s->move!=W_CHIJIN)return wilds_native(s,o,0);
        if(pressed&ML_A)return wilds_native(s,o,s->move==W_SLIDE?4:25);
    }
    /* Let movement or a new command interrupt the stand-up from the old exit point. */
    unsigned skip_rise=s->move==W_RELEASE&&frame>=W_RELEASE_RECOVERY_CANCEL&&
        (b->magnitude>=50||(pressed&(ML_X|ML_A|ML_R|ML_Y)));
    if(finish||skip_rise)return wilds_charge_recover(s,o);
    return (WDecision){0};
}
#endif
