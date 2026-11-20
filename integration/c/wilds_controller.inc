#if ML_WILDS_MOVES
static WDecision wilds_phase(WState *s,const WObservation *o,unsigned source)
{
    unsigned previous=s->source;
    WDecision d=ground_motion(s,source,previous,o->base.animation_frame,W_MOTION);
    d.blend=4;s->wilds_bridge_hold=0;s->wilds_charge_link=0;s->spirit_crimson_pending=s->spirit_crimson_repeat=0;s->hit_success=0;s->wilds_buffer=s->wilds_roll_buffer=0;s->wilds_r_pending=0;
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
static unsigned wilds_fade_action(const WObservation *o)
{
    const MLObservation *b=&o->base;
    int delta=ml_heading_delta(b->input_heading,b->heading);
    if(b->magnitude>=50){
        if(delta>=8192&&delta<=24576)return 28;
        if(delta<=-8192&&delta>=-24576)return 29;
    }
    return o->valor?143:8;
}
static WDecision wilds_spirit(WState *s,const WObservation *o)
{
    if(o->valor&&s->move==W_CHIJIN){
        unsigned moving=s->wilds_r_direction,heading=s->wilds_direction_heading,input=s->wilds_direction_input;
        WDecision d=wilds_native(s,o,130);
        d.valor_slide=moving;d.valor_slide_heading=heading;d.valor_slide_input=input;
        return d;
    }
    if((s->move==W_CHIJIN||s->source==902)&&s->wilds_r_direction){
        WObservation next=*o;
        next.base.heading=s->wilds_direction_heading;next.base.input_heading=s->wilds_direction_input;
        next.base.magnitude=100;
        WDecision d=w_wilds_enter(s,&next,W_SLIDE);
        if(d.kind!=W_KEEP)return d;
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
            return wilds_native(s,o,o->valor?136:26);
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
    WDecision d=wilds_spirit(s,o);
    if(queued==W_SPIRIT_CRIMSON_NEXT&&d.kind==W_NATIVE&&(d.native_action==16||d.native_action==17))
        s->spirit_crimson_pending=queued;
    return d;
}
static WDecision wilds_charge(WState *s,const WObservation *o,unsigned stage)
{
    WDecision d=w_wilds_enter(s,o,W_CHARGE);s->wilds_stage=stage;return d;
}
/* Recovery may end before the hold reaches its threshold. Carry the
 * reserved follow-up across that natural exit while R stays down. */
static WDecision wilds_charge_recover(WState *s,const WObservation *o)
{
    unsigned link=s->wilds_charge_link;
    WDecision d=wilds_finish(s,W_IDLE,o->base.heading);
    s->wilds_charge_link=link;
    return d;
}
static WDecision wilds_round(WState *s,const WObservation *o)
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
    unsigned edge=b->held&~s->wilds_bridge_held;
    s->wilds_bridge_sample=b->sample_sequence;s->wilds_bridge_held=b->held;
    float dt=b->simulation_delta>0&&b->simulation_delta<30?b->simulation_delta/60:0;
    unsigned recover=source==1&&o->grounded&&b->held==ML_R&&s->wilds_charge_link;
    if(source!=s->wilds_chain_source){
        s->wilds_chain_source=source;s->wilds_r_pending=0;s->wilds_charge_open=0;
        if(!recover){s->wilds_bridge_hold=0;s->wilds_charge_link=0;}
    }
    if(recover){
        s->wilds_bridge_hold+=dt;
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
                if(d.kind==W_KEEP&&o->drawn)return wilds_native(s,o,spirit_available(o,15)?16:17);
                return d;
            }
        }
    }
    if(b->held!=ML_R)s->wilds_bridge_hold=0;
    else if(b->simulation_delta>0&&b->simulation_delta<30)s->wilds_bridge_hold+=b->simulation_delta/60;
    if(!o->grounded)return (WDecision){0};
    if(s->move==W_LAND&&s->wilds_helm_red&&b->animation_frame>=12&&
       (edge&ML_R)&&b->held==ML_R)return w_wilds_enter(s,o,W_RELEASE);
    unsigned spirit=w_spirit_charge_source(source);
    unsigned insertion=source==112||source==307;
    unsigned ordinary_source=!s->move&&w_ordinary_charge_source(source);
    unsigned ordinary_charge=ordinary_source&&b->magnitude<50;
    unsigned allowed=ordinary_charge||(!s->move&&spirit)||insertion;
    unsigned permission=allowed?input_stage(w_event(source,b->animation_frame),WI_R):0;
    if(ordinary_source&&!ordinary_charge)s->wilds_charge_link=0;
    if(permission==3)s->wilds_charge_open=1;
    if(allowed&&s->wilds_charge_open&&b->held==ML_R){
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
                if(source==108)return wilds_round(s,o);
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
static unsigned wilds_red_follow_r(WState *s,const MLObservation *b,unsigned edge,float cancel)
{
    if(b->held&~ML_R)s->wilds_r_pending=0;
    else if((edge&ML_R)&&b->held==ML_R&&b->animation_frame+15>=cancel){
        s->wilds_r_pending=1;s->wilds_r_wait=.32f;
    }
    return b->animation_frame>=cancel&&s->wilds_r_pending&&s->wilds_r_wait>0;
}

static WDecision wilds_red_round_step(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    if(s->wilds_sample==b->sample_sequence)return (WDecision){0};
    s->wilds_sample=b->sample_sequence;
    unsigned edge=b->held&~s->wilds_previous;s->wilds_previous=b->held;
    if(b->simulation_delta>0&&b->simulation_delta<30)s->wilds_r_wait-=b->simulation_delta/60;
    if(wilds_red_follow_r(s,b,edge,50)&&o->rank==3){
        WDecision d=enter(s,o,W_CHARGE,ML_R);
        s->red_foresight_chain=1;s->wilds_tier=4;
        d.source=s->source=W_CRIMSON_SPIN;d.slot=s->slot=w_clip(W_CRIMSON_SPIN)->slot;
        return d;
    }
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
    case 903:case 910:case 911:return 56;
    case 912:return 60;
    case W_SPIRIT_TWO_FORWARD:return 62;
    case W_SPIRIT_TWO_LEFT:case W_SPIRIT_TWO_RIGHT:return 72;
    case W_SPIRIT_TWO_BACK:return 80;
    case W_CRIMSON_SPIN:return 69;
    case 902:return 110;
    case 914:case W_FOCUS_BACK:return 50;
    case W_FOCUS_LEFT:case W_FOCUS_RIGHT:return 60;
    default:return 0;
    }
}

static WDecision wilds_step(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;const WildsClip *clip=wilds_clip(s->source);
    if(!clip)return wilds_finish(s,W_ABORT,b->heading);
    float frame=b->animation_frame,dt=0;unsigned pressed=0;
    if(s->wilds_sample!=b->sample_sequence){
        s->wilds_sample=b->sample_sequence;
        pressed=b->held&~s->wilds_previous;s->wilds_previous=b->held;
        if(b->simulation_delta>0&&b->simulation_delta<30)dt=b->simulation_delta/60;
        if(s->wilds_buffer>0)s->wilds_buffer-=dt;
        if(s->wilds_roll_buffer>0)s->wilds_roll_buffer-=dt;
        if(s->wilds_r_wait>0)s->wilds_r_wait-=dt;
    }
    unsigned charge_window=wilds_charge_window(s,o);
    unsigned repeat=s->spirit_crimson_repeat;
    unsigned holding_charge=charge_window&&b->held==ML_R&&s->spirit_crimson_pending!=W_SPIRIT_CRIMSON_NEXT&&
        !(s->source==902&&(b->magnitude>=50||(s->wilds_r_pending&&s->wilds_r_direction)));
    s->wilds_charge_link=0;
    if(charge_window){
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
            s->wilds_tier=ready?4:s->wilds_elapsed<.8f?1:s->wilds_elapsed<1.6f?2:3;
            unsigned stage=s->wilds_stage;
            if(ready)stage=4;
            else if(s->wilds_elapsed>=1.6f&&stage<3)stage=3;
            if(stage==2){
                if(b->magnitude>=50&&spirit_available(o,15)){
                    wilds_spirit_direction(s,b);
                    return wilds_moving_spirit_two(s,o);
                }
                return wilds_native(s,o,o->valor?116:23);
            }
            return wilds_phase(s,o,stage==4?917:902);
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
    unsigned retreat_spirit=crimson&&s->source==W_CRIMSON_RETREAT;
    /* A solo R during a Crimson swing is one queued follow-up. Keep the
     * stage's last attack window intact and take its normal cancel frame. */
    unsigned repeat_x=repeat&&s->wilds_buffer>0&&s->crimson_buffer_source==906;
    unsigned spirit_r=b->held==ML_R||(repeat_x&&b->held==(ML_R|ML_X));
    if(spirit_link&&(pressed&ML_R)&&spirit_r&&(crimson||frame+18>=clip->cancel)){
        s->wilds_r_pending=1;s->wilds_r_wait=.10f;
        wilds_spirit_direction(s,b);
        if(repeat_x){
            wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_NEXT,s->crimson_buffer_heading,s->crimson_buffer_direction?100:0);
            s->wilds_buffer=0;
        }
    }
    if(b->held&(ML_Y|ML_ZR|ML_B))s->wilds_r_pending=0;
    if(crimson&&frame<clip->cancel&&(b->held&(repeat?ML_A:ML_X|ML_A)))s->wilds_r_pending=0;
    if(repeat){
        /* X rolled onto a pending Spirit I belongs to that next action. */
        if(b->held&~(ML_R|ML_X))s->spirit_crimson_pending=0;
        else if(s->wilds_r_pending&&(pressed&ML_X)&&s->spirit_crimson_pending!=W_SPIRIT_CRIMSON_NEXT)
            wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_NEXT,b->input_heading,b->magnitude);
    }
    /* Read the complete held chord when the cut permits a follow-up.
     * Its second button may have been pressed during the swing or hitstop. */
    if(crimson&&frame>=(retreat_spirit?40u:clip->cancel)){
        if(b->held==(ML_R|ML_A))return enter(s,o,W_FORESIGHT,ML_R|ML_A);
        if(!o->valor&&b->held==(ML_R|ML_B))return enter(s,o,W_SHEATHE,ML_R|ML_B);
    }
    if(retreat_spirit&&frame<clip->cancel){
        if(!holding_charge&&frame>=40&&s->wilds_r_pending&&s->wilds_r_wait<=0)
            return wilds_spirit_follow(s,o);
    }
    /* Buffer the final twelve recovery frames so an early evade tap is not
     * swallowed by the native request filter while our action owns the slot. */
    if((pressed&ML_B)&&!(b->held&ML_ZR)&&frame+12>=clip->cancel){
        s->wilds_roll_buffer=.25f;s->wilds_roll_mod=b->held&ML_R;
        s->wilds_roll_heading=b->magnitude>=50?b->input_heading:b->heading;
    }
    if(frame>=clip->cancel&&s->wilds_roll_buffer>0){
        if(s->wilds_roll_mod||(b->held&ML_R)){
            if(!o->valor)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            s->wilds_roll_buffer=0;
        }else return wilds_finish(s,W_ROLL,s->wilds_roll_heading);
    }
    unsigned finish=frame>=(s->source==917?(s->red_foresight_chain?105:clip->frames-1):clip->recovery)||
        b->motion_finished;
    if(s->source==918&&wilds_red_follow_r(s,b,pressed,clip->cancel)){
        WDecision d=ground_enter(s,o,5,ML_R);
        if(d.kind==W_ENTER){
            s->red_foresight_chain=1;s->wilds_previous=b->held;s->wilds_sample=b->sample_sequence;
            s->wilds_r_pending=0;
        }
        return d;
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
            s->crimson_buffer_direction=wilds_fade_action(o);
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
    if(frame>=clip->cancel){
        if((pressed&(ML_X|ML_A))&&(b->held&(ML_X|ML_A|ML_R|ML_ZR))==(ML_X|ML_A)){
            return wilds_native(s,o,wilds_fade_action(o));
        }
        if((pressed&ML_B)&&!(b->held&ML_ZR)){
            if(b->held&ML_R)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        }
        if((pressed&(ML_R|ML_X|ML_A))&&(b->held==(ML_R|ML_X)||b->held==(ML_R|ML_A))&&
           !(b->held==(ML_R|ML_X)&&repeat&&s->wilds_r_pending))
            return enter(s,o,b->held&ML_A?W_FORESIGHT:W_THRUST,b->held);
        if(!holding_charge&&s->wilds_r_pending&&s->wilds_r_wait<=0)return wilds_spirit_follow(s,o);
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
