#if ML_WILDS_MOVES
static WDecision wilds_phase(WState *s,const WObservation *o,unsigned source)
{
    unsigned previous=s->source;
    WDecision d=ground_motion(s,source,previous,o->base.animation_frame,W_MOTION);
    d.blend=4;s->hit_success=0;s->wilds_buffer=s->wilds_roll_buffer=0;s->wilds_r_pending=0;
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
    if(s->source==902){s->wilds_tier=4;return wilds_phase(s,o,916);}
    if(o->valor&&s->move==W_CHIJIN){
        unsigned moving=s->wilds_r_direction,heading=s->wilds_direction_heading,input=s->wilds_direction_input;
        WDecision d=wilds_native(s,o,130);
        d.valor_slide=moving;d.valor_slide_heading=heading;d.valor_slide_input=input;
        return d;
    }
    if(s->move==W_CHIJIN&&s->wilds_r_direction){
        WObservation next=*o;
        next.base.heading=s->wilds_direction_heading;next.base.input_heading=s->wilds_direction_input;
        next.base.magnitude=100;
        WDecision d=w_wilds_enter(s,&next,W_SLIDE);
        if(d.kind!=W_KEEP)return d;
    }
    if(s->move==W_SLIDE){
        if(wilds_spirit_is_two(s->source)){
            /* Moving II occupies the same link as native action 23. */
            return wilds_native(s,o,o->valor?136:26);
        }
        if(s->wilds_r_direction&&spirit_available(o,15))return wilds_moving_spirit_two(s,o);
        return wilds_native(s,o,23);
    }
    return wilds_native(s,o,s->move==W_FOCUS?26:(spirit_available(o,15)?16:17));
}
static WDecision wilds_charge(WState *s,const WObservation *o,unsigned stage)
{
    WDecision d=w_wilds_enter(s,o,W_CHARGE);s->wilds_stage=stage;return d;
}
static WDecision wilds_round(WState *s,const WObservation *o)
{
    WDecision d=w_wilds_enter(s,o,W_CHARGE);if(d.kind!=W_ENTER)return d;
    s->wilds_tier=4;s->wilds_stage=4;s->source=916;s->slot=w_clip(916)->slot;
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
    if(source!=s->wilds_chain_source){s->wilds_chain_source=source;s->wilds_bridge_hold=0;s->wilds_r_pending=0;}
    float dt=b->simulation_delta>0&&b->simulation_delta<30?b->simulation_delta/60:0;
    unsigned ordinary=!s->move&&(source==1||source==101||source==102||source==103||source==105||source==107);
    unsigned opener=ordinary&&(source==1||b->animation_frame>=24);
    if(opener&&(b->held&(ML_R|ML_B))==(ML_R|ML_B)&&(edge&(ML_R|ML_B))&&
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
                return d.kind==W_KEEP?wilds_native(s,o,spirit_available(o,15)?16:17):d;
            }
        }
    }
    if(b->held!=ML_R)s->wilds_bridge_hold=0;
    else if(b->simulation_delta>0&&b->simulation_delta<30)s->wilds_bridge_hold+=b->simulation_delta/60;
    if(!o->grounded)return (WDecision){0};
    if(s->move==W_LAND&&s->wilds_helm_red&&b->animation_frame>=12&&
       (edge&ML_R)&&b->held==ML_R)return w_wilds_enter(s,o,W_RELEASE);
    unsigned spirit=source==104||source==106||source==108;
    unsigned insertion=source==112||source==307;
    if((!s->move&&spirit)||insertion){
        unsigned permission=input_stage(w_event(source,b->animation_frame),WI_R);
        if((edge&ML_R)&&b->held==ML_R&&permission){
            s->wilds_r_pending=1;s->wilds_r_wait=.28f;
            wilds_spirit_direction(s,b);
        }
        if(b->held&(ML_X|ML_A|ML_B|ML_Y|ML_ZR))s->wilds_r_pending=0;
        if(s->wilds_r_wait>0)s->wilds_r_wait-=dt;
        if(!(b->held&ML_R))s->wilds_r_wait=0;
        if(permission==3){
            if(spirit&&b->held==ML_R&&s->wilds_bridge_hold>=.6f)
                return wilds_charge(s,o,source==104?2:source==106?3:4);
            if(s->wilds_r_pending&&!(b->held&ML_R)){
                s->wilds_r_pending=0;
                if(source==108)return wilds_round(s,o);
                if(source==104&&s->wilds_r_direction&&spirit_available(o,15))
                    return wilds_moving_spirit_two(s,o);
                return wilds_native(s,o,source==104||source==307?23:26);
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
    if(s->move==W_SLIDE){
        if(b->held==ML_R)s->wilds_elapsed+=dt;else s->wilds_elapsed=0;
    }
    if(s->source==908||s->source==909||s->source==915){
        if(pressed&ML_B)return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        s->wilds_elapsed+=dt*(s->wilds_stage==3?1.3f:1.0f);
        if(!(b->held&ML_R)||s->wilds_elapsed>=3){
            s->wilds_tier=s->wilds_elapsed<.8f?1:s->wilds_elapsed<1.6f?2:s->wilds_elapsed<2.9f?3:4;
            unsigned stage=s->wilds_stage;
            if(s->wilds_elapsed>=2.9f)stage=4;
            else if(s->wilds_elapsed>=1.6f&&stage<3)stage=3;
            if(stage==2)return wilds_native(s,o,23);
            return wilds_phase(s,o,stage==4?916:902);
        }
        WDecision d={0};
        s->wilds_gauge_fraction+=dt*10;
        d.gauge_delta=(int)s->wilds_gauge_fraction;s->wilds_gauge_fraction-=d.gauge_delta;
        unsigned walk=b->magnitude>=50;
        if((s->source==909&&walk)||(s->source==915&&!walk))return wilds_phase(s,o,walk?915:909);
        if(b->motion_finished||frame>=clip->frames-2){
            WDecision next=wilds_phase(s,o,walk?915:909);next.gauge_delta=d.gauge_delta;return next;
        }
        return d;
    }
    /* Match the World kernel's 0.10-second R confirmation, so a human
     * R+B chord cannot be stolen by a leading solo R request. */
    unsigned spirit_link=s->move==W_CHIJIN||s->move==W_SLIDE||s->move==W_FOCUS||s->source==902;
    /* MOT 287 plants the feet and finishes its backward cut at frame 40.
     * Let solo R take over there. The later X chain still uses frame 90. */
    unsigned crimson=s->move==W_CHIJIN;
    unsigned retreat_spirit=crimson&&s->source==W_CRIMSON_RETREAT;
    /* A solo R during a Crimson swing is one queued follow-up. Keep the
     * stage's last attack window intact and take its normal cancel frame. */
    if(spirit_link&&(pressed&ML_R)&&b->held==ML_R&&(crimson||frame+18>=clip->cancel)){
        s->wilds_r_pending=1;s->wilds_r_wait=.10f;
        wilds_spirit_direction(s,b);
    }
    if(b->held&(ML_Y|ML_ZR|ML_B))s->wilds_r_pending=0;
    if(crimson&&frame<clip->cancel&&(b->held&(ML_X|ML_A)))s->wilds_r_pending=0;
    /* Read the complete held chord when the cut permits a follow-up.
     * Its second button may have been pressed during the swing or hitstop. */
    if(crimson&&frame>=(retreat_spirit?40u:clip->cancel)){
        if(b->held==(ML_R|ML_A))return enter(s,o,W_FORESIGHT,ML_R|ML_A);
        if(!o->valor&&b->held==(ML_R|ML_B))return enter(s,o,W_SHEATHE,ML_R|ML_B);
    }
    if(retreat_spirit&&frame<clip->cancel){
        if(frame>=40&&s->wilds_r_pending&&s->wilds_r_wait<=0){
            s->wilds_r_pending=0;
            return wilds_spirit(s,o);
        }
    }
    /* Buffer the final twelve recovery frames so an early evade tap is not
     * swallowed by the native request filter while our action owns the slot. */
    if((pressed&ML_B)&&!(b->held&ML_ZR)&&frame+12>=clip->cancel){
        s->wilds_roll_buffer=.25f;s->wilds_roll_mod=b->held&ML_R;
        s->wilds_roll_heading=b->magnitude>=50?b->input_heading:b->heading;
    }
    if(frame>=clip->cancel&&s->wilds_roll_buffer>0){
        if(s->wilds_roll_mod||(b->held&ML_R))return enter(s,o,W_SHEATHE,ML_R|ML_B);
        return wilds_finish(s,W_ROLL,s->wilds_roll_heading);
    }
    if(s->source==916&&(b->motion_finished||frame>=clip->frames-2))return wilds_phase(s,o,917);
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
        /* Wilds MOT 287 accepts the next X from frame 60 and branches at 90. */
        unsigned retreat=s->source==W_CRIMSON_RETREAT;
        unsigned buffer_begin=retreat?60:clip->cancel-18;
        if((pressed&ML_X)&&!(b->held&(ML_A|ML_R|ML_ZR))&&frame>=buffer_begin){
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
        if(frame>=clip->cancel&&s->wilds_buffer>0){
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
    if(s->move==W_FOCUS){
        if(s->wilds_focus_break&&(s->source==905||s->source==W_FOCUS_FLURRY||s->source==913)){
            if(b->magnitude>=50){
                int angle=ml_heading_delta(b->input_heading,s->heading);
                s->focus_finisher=angle>24576||angle< -24576?W_FOCUS_BACK:
                    angle>=8192?W_FOCUS_LEFT:angle<=-8192?W_FOCUS_RIGHT:914;
            }
            if(s->source==905&&frame>=50)return wilds_phase(s,o,W_FOCUS_FLURRY);
            if(s->source==W_FOCUS_FLURRY&&frame>=125)return wilds_phase(s,o,913);
            if(s->source==913&&frame>=84)
                return wilds_phase(s,o,s->focus_finisher?s->focus_finisher:914);
        }
    }
    if(frame>=clip->cancel){
        if(s->move==W_SLIDE&&b->held==ML_R&&s->wilds_elapsed>=.6f)
            return wilds_charge(s,o,wilds_spirit_is_two(s->source)?3:2);
        if((pressed&(ML_X|ML_A))&&(b->held&(ML_X|ML_A|ML_R|ML_ZR))==(ML_X|ML_A)){
            return wilds_native(s,o,wilds_fade_action(o));
        }
        if((pressed&ML_B)&&!(b->held&ML_ZR)){
            if(b->held&ML_R)return enter(s,o,W_SHEATHE,ML_R|ML_B);
            return wilds_finish(s,W_ROLL,b->magnitude>=50?b->input_heading:b->heading);
        }
        if((b->held&ML_R)&&(pressed&(ML_X|ML_A))){
            return enter(s,o,pressed&ML_A?W_FORESIGHT:W_THRUST,b->held);
        }
        if(s->wilds_r_pending&&s->wilds_r_wait<=0){s->wilds_r_pending=0;return wilds_spirit(s,o);}
        if((pressed&ML_X)&&s->move!=W_CHIJIN&&o->rank==3)return w_wilds_enter(s,o,W_CHIJIN);
        if((pressed&ML_X)&&s->move!=W_CHIJIN)return wilds_native(s,o,0);
        if(pressed&ML_A)return wilds_native(s,o,s->move==W_SLIDE?4:25);
    }
    if(finish)return wilds_finish(s,W_IDLE,b->heading);
    return (WDecision){0};
}
#endif
