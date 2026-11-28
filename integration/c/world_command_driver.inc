/* Source-owned input kernel. Legacy manual queues below w_step's VM branch
 * remain available only for historical development-build comparisons. */
typedef struct {WState *s;const WObservation *o;uint64_t known,values;} WGroundQuery;
static unsigned world_property(WGroundQuery *q,unsigned property)
{
    uint64_t bit=(uint64_t)1<<property;
    if(q->known&bit)return (q->values&bit)!=0;
    q->known|=bit;unsigned value=0;WState *s=q->s;const WObservation *o=q->o;
    static const unsigned commands[]={39,53,60,25,46,51,52,32};
    if(property<8)value=w_controls_query(&s->controls,commands[property],1,0);
    else switch(property){
    case 8:value=o->base.motion_finished;break;
    case 9:value=1;break; /* contact is validated by the native monster registry */
    case 10:value=o->rank>=1;break;
    case 11:value=w_controls_query(&s->controls,24,1,0);break;
    case 12:value=s->hit_success;break;
    case 13:value=o->grounded;break;
    case 14:value=1;break; /* source Finish gauge threshold is0; colour is separate */
    case 15:value=spirit_available(o,25);break;
    case 16:value=spirit_available(o,15);break;
    case 17:value=spirit_available(o,20);break;
    case 18:value=s->counter_success&&s->hit_success;break;
    }
    if(value)q->values|=bit;
    return value;
}
static int world_query_node(WGroundQuery *q,unsigned node)
{
    const WGroundNode *n=w_ground_node(node);if(!n)return -1;
    for(unsigned i=0;i<n->count;++i){
        const WGroundEdge *edge=world_ground_edges+n->offset+i;
        unsigned stack[32],top=0;
        for(unsigned j=0;j<edge->count;++j){
            WGroundTerm term=world_ground_terms[edge->offset+j];
            if(term.op<=1){if(top>=32)return -1;stack[top++]=term.op?world_property(q,term.value):term.value;}
            else{
                if(term.value>top)return -1;
                unsigned result=term.op==2;
                for(unsigned k=0;k<term.value;++k){unsigned v=stack[--top];if(term.op==2)result&=v;else result|=v;}
                stack[top++]=result;
            }
        }
        if(top==1&&stack[0])return (int)edge->target;
    }
    return -1;
}
static void world_input_permission(unsigned source,const WEvent *event,uint32_t *permission)
{
    for(unsigned i=0;i<8;++i)permission[i]=event&&i<7?event->input[i]:0;
    permission[2]=(permission[2]&~(3u<<22))|(w_foresight_stage(source,event)<<22);
}
static unsigned world_source_node(unsigned source)
{
    switch(source){
    case 1:return 0;case 101:return 11;case 102:return 10;case 103:return 7;
    case 104:return 1;case 105:return 4;case 106:return 12;case 107:return 3;
    case 108:return 2;case 109:return 5;case 110:return 6;case 112:return 23;
    case 127:return 9;case 128:return 8;case 140:return 28;case 170:return 45;
    case 174:return 57;case 307:return 64;
    case 308:case 309:case 310:case 311:return 65;
    default:return 0xffff;
    }
}
/* Only the post-small-Iai spirit opener has no GU counterpart. Resolve the
 * other World edges to stock commands so their follow-up context, hit rows,
 * gauge accounting and presentation all belong to the native handler. */
static WDecision world_route(WState *s,const WObservation *o,unsigned edge)
{
    const WGroundNode *n=w_ground_node(edge);
    if(!n)return (WDecision){0};
    int dest=n->kind==2?w_ground_select(edge,0):(int)edge;
#if ML_WILDS_MOVES
    if(s->move==W_FORESIGHT&&o->valor_style&&dest==5)return (WDecision){0};
    /* A resolved ordinary cut retains its source cancel window. */
    if(o->rank==3&&o->grounded&&o->base.held==ML_X&&
       (dest==3||dest==4||dest==10||dest==11||dest==38))return w_wilds_enter(s,o,W_CHIJIN);
    if(s->move==W_BIG&&!s->counter_success&&dest==2)return (WDecision){0};
#endif
    if(dest==28||dest==45||dest==61||(s->move==W_SMALL&&edge==82))
        return ground_enter(s,o,edge,0);
    if(!s->move){
        /* The two ordinary combo edges use GU's thrust and combo upper.
         * Spirit insertions share these poses but have different actions. */
        unsigned from=o->base.native_action, motion=o->base.motion;
        if(edge==1&&w_guild_spirit_opener(from,motion,o->previous_weapon_action))
            return (WDecision){.kind=W_NATIVE,.native_action=spirit_available(o,15)?16:17,.heading=o->base.heading};
        unsigned upper=(from==4&&motion==0x569&&edge==31)||(from==22&&(motion==0x569||motion==0x570)&&edge==3);
        unsigned thrust=(from==25||from==41)&&motion==0x56b&&edge==4;
        if((from==7||from==25||from==41)&&motion==0x56b&&edge==39)
            return (WDecision){.kind=W_NATIVE,.native_action=5,.heading=o->base.heading};
        if(upper||thrust)
            return (WDecision){.kind=W_NATIVE,.native_action=upper?25:4,.heading=o->base.heading};
        return (WDecision){0};
    }

    unsigned action;
    switch(dest){
    case 1:action=spirit_available(o,15)?16:17;break;
    case 2:action=26;break;
    case 3:case 38:action=25;break;
    case 4:action=4;break;
    case 5:action=30;break;
    case 7:action=8;break;
    /* Measured against camera-centered walking: GU 28 travels left, 29 right. */
    case 8:action=28;break;
    case 9:action=29;break;
    case 10:action=5;break;
    case 11:action=0;break;
    case 12:action=23;break;
    case 23:action=22;break;
    default:return (WDecision){0};
    }
#if ML_WILDS_MOVES
    if(!spirit_available(o,w_spirit_link_cost(action)))return (WDecision){0};
#endif
    WDecision d=w_leave(s);d.kind=W_NATIVE;d.native_action=action;d.heading=o->base.heading;
    return d;
}
/* Preserve a two-button tap across GU's native request hook and the
 * partner's delayed confirmation. Both physical buttons must overlap on
 * the same stock attack in a valid source window. */
static void world_guild_chords(WState *s,unsigned held,const uint32_t *permission)
{
    WControls *c=&s->controls;
    static const unsigned ids[]={53,60,24},partners[]={ML_X,ML_A,ML_B},buttons[]={0,1,3};
    for(unsigned i=0;i<3;++i){
        unsigned id=ids[i],mask=ML_R|partners[i];
        int bit=world_command_bits[id];
        unsigned stage=w_permission_bit(permission,bit-1)|(w_permission_bit(permission,bit)<<1);
        if(!stage||(held&~mask)){
            if(s->gu_chord_wait==id)s->gu_chord_wait=0;
            continue;
        }
        if(held==mask&&(w_command_has(c->commands.raw,id)||w_command_has(c->commands.observed,id)))
            s->gu_chord_wait=id;
        if(s->gu_chord_wait!=id)continue;
        if(c->buttons[buttons[i]].pressed){
            w_command_collect(&c->commands,id,stage,0,0,0,0);
            w_command_promote(&c->commands,id);
        }
        if(!c->buttons[buttons[i]].waiting)s->gu_chord_wait=0;
    }
}
#include "wilds_controller.inc"
#if ML_WILDS_MOVES
static WDecision valor_foresight_follow(WState *s,const WObservation *o,const WEvent *event,unsigned fresh_r)
{
    const MLObservation *b=&o->base;
    if(!o->grounded||(b->held&~ML_R)||(s->valor_foresight&&!o->valor))s->valor_foresight_r=0;
    else if(s->counter_success&&fresh_r)s->valor_foresight_r=1;
    if(!s->valor_foresight_r||!s->hit_success)return (WDecision){0};
    unsigned red=s->source==918;
    unsigned ready=red?b->animation_frame>=wilds_clip(918)->cancel:input_stage(event,WI_R)==3;
    if(!ready||!spirit_available(o,o->valor?15:20))return (WDecision){0};
    WDecision d=wilds_native(s,o,o->valor?116:26);
    d.valor_foresight_second=o->valor;
    s->wilds_native_r_spent=b->held==ML_R;
    s->wilds_bridge_held=b->held;
    return d;
}

/* The complete physical chord needs no single-button confirmation. Keep a
 * released tap on its source action until that action permits the cancel. */
WDecision w_foresight_press(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    const unsigned chord=ML_R|ML_A;
    if(s->foresight_press.motion&&s->foresight_press.valor!=o->valor){
        s->foresight_press.motion=0;
        s->consumed|=b->held&chord;
        return (WDecision){0};
    }
    unsigned stage=0;
    float frame=b->animation_frame;
    if(s->actor==b->actor&&s->epoch==b->epoch&&b->eligible&&o->alive&&o->drawn&&
       o->grounded&&o->resources_ready&&o->state==16&&frame>=0&&frame<100000){
        if(s->move==W_CHIJIN&&b->native_action==w_action(W_CHIJIN)&&b->motion==0x500+s->slot){
            const WildsClip *clip=wilds_clip(s->source);
            if(clip){
                unsigned cancel=s->source==W_CRIMSON_RETREAT?40u:clip->cancel;
                stage=frame>=cancel?3:frame+18>=cancel?1:0;
            }
        }else if(s->move==W_CHARGE&&!o->valor&&b->native_action==w_action(W_CHARGE)&&b->motion==0x500+s->slot){
            if(s->source==908)stage=frame>=20?3:1;
            else if(s->source==909||s->source==915)stage=3;
            else if(s->source==902||s->source==917){
                unsigned cancel=wilds_clip(s->source)->cancel;
                stage=frame>=cancel?3:frame+18>=cancel?1:0;
            }
        }else if(!s->move&&!o->valor){
            unsigned source=w_guild_source(o->state,b->native_action,b->motion);
            if(world_source_node(source)!=0xffff||source==149||source==154)
                stage=w_foresight_stage(source,w_event(source,frame));
        }
    }
    if(!stage||(b->held&~chord)){
        s->foresight_press.motion=0;
        return (WDecision){0};
    }
    if(b->held==chord&&(s->consumed&chord)!=chord){
        s->foresight_press.motion=b->motion;
        s->foresight_press.action=b->native_action;
        s->foresight_press.generation=b->bind_generation;
        s->foresight_press.valor=o->valor;
    }
    if(s->foresight_press.motion!=b->motion||s->foresight_press.action!=b->native_action||
       s->foresight_press.generation!=b->bind_generation){
        s->foresight_press.motion=0;
        return (WDecision){0};
    }
    if(stage&2)return enter(s,o,W_FORESIGHT,chord);
    return (WDecision){0};
}

static WDecision wilds_iai_follow(WState *s,const WObservation *o)
{
    const MLObservation *b=&o->base;
    float frame=b->animation_frame;
    unsigned held=b->held&~s->consumed,edge=0;
    if(s->wilds_sample!=b->sample_sequence){
        s->wilds_sample=b->sample_sequence;
        edge=held&~s->wilds_previous;s->wilds_previous=b->held;
        if(b->simulation_delta>0&&b->simulation_delta<30)
            s->iai_follow_wait-=b->simulation_delta/60;
    }
    if(s->move==W_SMALL){
        if(o->rank!=3||held&~ML_X){s->iai_follow_keys=0;return (WDecision){0};}
        if(frame>=24&&(edge&ML_X)){s->iai_follow_keys=ML_X;s->iai_follow_wait=.40f;}
        if(frame>=42&&s->iai_follow_keys&&s->iai_follow_wait>0){
            WDecision d=w_wilds_enter(s,o,W_CHIJIN);
            if(d.kind==W_ENTER)s->consumed|=ML_X;
            return d;
        }
        return (WDecision){0};
    }
    if(!s->counter_success){
        /* The opener consumes R for single-button commands, but a sustained
         * physical hold is an explicit charge gesture. A fresh R in recovery
         * needs only the tap discrimination interval. */
        float threshold=w_charge_gesture_time(s);
        if(frame>=37&&b->held==ML_R){
            s->wilds_charge_link=2;
            if(s->wilds_bridge_hold+1.e-5f>=threshold)return wilds_charge(s,o,2);
        }
        return (WDecision){0};
    }
    const unsigned thrust=ML_R|ML_X,sheathe=ML_R|ML_B;
    unsigned keys=b->held;
    /* A complete chord supersedes the pending solo R, even when its R
     * came from the Iai opener. Keep a pre-window tap through release. */
    if(keys&~(thrust|ML_B))s->iai_follow_keys=0;
    else if((keys==thrust||keys==sheathe)&&(edge&keys))s->iai_follow_keys=keys;
    else if(s->iai_follow_keys!=thrust&&s->iai_follow_keys!=sheathe){
        if(keys==ML_X&&(edge&ML_X)){
            s->iai_follow_keys=ML_X;
            s->iai_follow_wait=.05f;
        }else if(edge&(ML_R|ML_B)){
            if(s->iai_follow_wait<=0||s->iai_follow_keys==ML_X)s->iai_follow_keys=0;
            s->iai_follow_keys|=edge&(ML_R|ML_B);
            s->iai_follow_wait=.10f;
        }
    }
    if(frame<37)return (WDecision){0};
    WObservation next=*o;
    /* The rank change is committed after this decision. */
    if(!o->valor_style&&s->rank_paid&&o->rank==s->rank&&o->rank<3){
        next.rank=o->rank+1;if(next.rank==3)next.base.gauge=100;
    }
    if(s->iai_follow_keys==thrust)return enter(s,&next,W_THRUST,thrust);
    if(s->iai_follow_keys==sheathe){
        WDecision d=enter(s,&next,W_SHEATHE,sheathe);
        d.counter_resheathe=d.kind==W_ENTER&&!o->valor_style;
        return d;
    }
    if(frame>=70&&b->held==ML_R){
        s->wilds_charge_link=3;
        float threshold=w_charge_gesture_time(s);
        if(s->wilds_bridge_hold+1.e-5f>=threshold)return wilds_charge(s,&next,3);
    }
    if(next.rank==3&&!(keys&~ML_X)&&
       (s->iai_follow_keys==ML_X?s->iai_follow_wait<=0:held==ML_X))
        return w_wilds_enter(s,&next,W_CHIJIN);
    if(s->iai_follow_keys==ML_R&&s->iai_follow_wait<=0){
        if(held==ML_R){
            float after_confirm=w_charge_gesture_time(s)-.10f;
            if(frame>=70&&s->iai_follow_wait<=-after_confirm)return wilds_charge(s,&next,3);
            return (WDecision){0};
        }
        return wilds_native(s,&next,spirit_available(&next,20)?26:27);
    }
    return (WDecision){0};
}
#endif
#if ML_WILDS_MOVES
static unsigned wilds_focus_window(const WState *s,const WObservation *o)
{
    float frame=o->base.animation_frame;
    if(s->move==W_BIG)return (s->counter_success||o->counter)&&frame>=37;
    if(s->move==W_CHIJIN||s->move==W_SLIDE||s->source==902){
        const WildsClip *clip=wilds_clip(s->source);
        return clip&&frame>=clip->cancel;
    }
    if(s->move)return 0;
    if(!o->grounded)return 0;
    if(o->valor_draw_cancel)return frame>=o->valor_draw_cancel;
    if(o->valor)return w_valor_chord(o->state,o->base.native_action,o->base.motion,
                                      frame,ML_R|ML_Y,1)==W_FOCUS;
    unsigned source=w_guild_source(o->state,o->base.native_action,o->base.motion);
    return source==1||(source&&frame>=24);
}
#endif
static WDecision world_command_step(WState *s,const WObservation *o,unsigned source,const WEvent *event)
{
    const MLObservation *b=&o->base;
#if ML_WILDS_MOVES
    if(s->wilds_three_exit_r&&!s->wilds_three_exit_sample){
        if(s->move||!s->wilds_charge_link||!o->grounded||o->valor||(b->held&~ML_R)){
            s->wilds_three_exit_r=0;
        }else if((o->state==1||o->state==2)&&b->native_action<=5&&!b->held){
            unsigned source=s->wilds_three_exit_r;
            s->wilds_three_exit_r=0;
            s->wilds_r_pending=0;
            return source==902?w_wilds_round(s,o):wilds_spirit(s,o,907);
        }
    }
    if(s->move==W_CHIJIN&&s->source!=906&&s->source!=907&&b->animation_frame<=8){
        if(b->held==(ML_R|ML_X)&&!(s->consumed&ML_R)&&!s->spirit_crimson_repeat)
            return enter(s,o,W_THRUST,b->held);
        if(o->valor&&b->held==(ML_X|ML_A))return wilds_native(s,o,w_wilds_fade_action(o));
    }
    if(o->focus_strike&&wilds_focus_window(s,o)){
        WDecision focus=w_wilds_enter(s,o,W_FOCUS);
        if(focus.kind==W_ENTER){s->consumed|=ML_R|ML_Y;return focus;}
    }
    WDecision foresight=w_foresight_press(s,o);
    if(foresight.kind!=W_KEEP)return foresight;
    WDecision spin=wilds_round_spin(s,o);
    if(spin.kind!=W_KEEP)return spin;
    if(w_round_spin_recovery(s,b->sample_sequence)&&b->held==ML_R)return (WDecision){0};
    /* Native recovery owns the cancel window. Capture X before GU can
     * queue a regular thrust or mistake the tail of Spirit I's R for a chord. */
    unsigned spirit_one=!s->move&&w_spirit_one_crimson_source(o->state,b->native_action,b->motion);
    unsigned standing_spirit=!s->move&&w_native_spirit_one(o->state,b->native_action);
    unsigned valor_spirit=!s->move&&o->valor&&w_valor_spirit_one(o->state,b->native_action);
    unsigned valor_reaction=!s->move&&o->valor&&w_valor_guard_reaction(o->state,b->native_action,b->motion);
    unsigned native_spirit=standing_spirit||valor_spirit;
    unsigned prep=standing_spirit&&!spirit_one;
    /* Valor binds two different GP I clips; the earliest X frame belongs to
     * the bound native clip. Before it binds, keep the same X through prep. */
    unsigned red_native=!s->move&&o->rank==3&&o->grounded&&o->state==16&&
        (native_spirit||w_valor_chord(o->state,b->native_action,b->motion,100000.f,ML_X,0)==W_CHIJIN);
    unsigned previous=s->spirit_crimson_held;
    unsigned x_edge=(b->held&ML_X)&&!(previous&ML_X);
    unsigned into_guard=valor_reaction&&s->spirit_crimson_action!=b->native_action;
    if(!valor_reaction||!(b->held&ML_R))s->spirit_crimson_guard_r=0;
    else if(into_guard)s->spirit_crimson_guard_r=previous&ML_R;
    unsigned rolled_r=native_spirit||s->spirit_crimson_guard_r;
    unsigned rolled_x=rolled_r&&b->held==(ML_R|ML_X)&&(previous&ML_R)&&x_edge;
    s->spirit_crimson_held=b->held;
    unsigned carried=s->spirit_crimson_pending==W_SPIRIT_CRIMSON_NEXT;
    if(red_native){
        unsigned keep_gp_input=into_guard&&s->spirit_crimson_action==130;
        if(!carried&&!keep_gp_input&&(s->spirit_crimson_action!=b->native_action||
           (!native_spirit&&s->spirit_crimson_motion!=b->motion)))s->spirit_crimson_pending=0;
        s->spirit_crimson_action=b->native_action;
        s->spirit_crimson_motion=b->motion;
        if((b->held&~(ML_R|ML_X))||(!rolled_r&&(b->held&ML_R)))
            s->spirit_crimson_pending=0;
        else if(!carried&&x_edge&&(b->held==ML_X||rolled_x)){
            wilds_queue_spirit_crimson(s,W_SPIRIT_CRIMSON_INPUT,b->input_heading,b->magnitude);
            s->spirit_crimson_confirm=native_spirit||valor_reaction||o->valor_move==W_CHIJIN?0:5;
        }
        if(s->spirit_crimson_pending){
            if(!carried&&s->spirit_crimson_confirm>0&&b->simulation_delta>0&&b->simulation_delta<30)
                s->spirit_crimson_confirm-=b->simulation_delta;
            unsigned ready=prep?0:spirit_one?b->animation_frame>=W_SPIRIT_ONE_CRIMSON_FRAME:
                w_valor_chord(o->state,b->native_action,b->motion,b->animation_frame,ML_X,0)==W_CHIJIN;
            if(!ready||(!carried&&s->spirit_crimson_confirm>0))return (WDecision){0};
            WObservation next=*o;
            next.base.input_heading=s->spirit_crimson_input;
            next.base.magnitude=s->spirit_crimson_magnitude;
            WDecision d=w_wilds_enter(s,&next,W_CHIJIN);
            if(d.kind==W_ENTER)s->spirit_crimson_repeat=native_spirit||valor_reaction;
            return d;
        }
    }else if(!s->move&&!s->spirit_crimson_repeat&&
             !(carried&&native_spirit)){
        s->spirit_crimson_pending=0;
    }
    if(red_native&&o->valor_move==W_CHIJIN)return (WDecision){0};
    if(!o->valor&&!s->move&&o->valor_move){
        unsigned partner=o->valor_move==W_THRUST?ML_X:o->valor_move==W_SHEATHE?ML_B:ML_A;
        return enter(s,o,o->valor_move,ML_R|partner);
    }
    if(!s->move&&s->wilds_charge_link&&(o->state==1||o->state==2)&&b->native_action<=5&&
       b->held==ML_R&&o->grounded)return wilds_bridge(s,o,1);
    if(o->valor&&!s->move){
        s->wilds_charge_link=0;
        if(!o->grounded||!o->valor_move)return (WDecision){0};
        if(o->valor_move==W_FOCUS||o->valor_move==W_SLIDE||o->valor_move==W_CHIJIN)return w_wilds_enter(s,o,o->valor_move);
        return enter(s,o,o->valor_move,b->held);
    }
    if(s->red_foresight_chain&&s->source==109)return wilds_red_round_step(s,o);
    if(s->move>=W_CHIJIN||s->source==918){
        unsigned end=s->move==W_FOCUS&&s->wilds_focus_break?w_focus_finisher_end(s->source):0;
        unsigned wounds=s->wilds_focus_wounds;
        WDecision d=wilds_step(s,o);
        /* The completed finisher owns the promotion. Early forced exits
         * and internal motion changes never settle it. */
        if(wounds&&end&&b->animation_frame>=end&&
           (d.kind==W_IDLE||d.kind==W_ENTER||d.kind==W_NATIVE||d.kind==W_NATIVE_R||d.kind==W_ROLL)){
            if(o->valor_style){
                if(!o->valor)d.valor_gain=W_VALOR_COUNTER_GAIN;
            }else if(o->rank<3){
                unsigned levels=3-o->rank;
                d.rank_delta+=(int)(wounds<levels?wounds:levels);
            }
        }
        return d;
    }
    unsigned bridge_source=s->move?s->source:w_guild_source(o->state,b->native_action,b->motion);
    if(!s->move&&(s->wilds_three_exit_sample||s->wilds_slide_exit_sample)&&
       (o->state==1||o->state==2)&&b->native_action<=5)
        bridge_source=1;
    WDecision bridge=wilds_bridge(s,o,bridge_source);
    if(bridge.kind!=W_KEEP)return bridge;
    if(s->wilds_direction_pending)return (WDecision){0};
    if(s->wilds_native_three_sample&&s->wilds_native_three_held&&b->held==ML_R)return (WDecision){0};
    if(s->wilds_r_pending&&b->held==ML_R&&
       (s->move==W_SMALL||(!s->move&&(bridge_source==112||w_ordinary_charge_source(bridge_source)))))return (WDecision){0};
    if(s->wilds_three_exit_sample&&s->wilds_three_exit_r==902&&b->held==ML_R)return (WDecision){0};
    if(s->wilds_slide_exit_r&&b->held==ML_R)return (WDecision){0};
#endif
    unsigned idle=!s->move&&(o->state==1||o->state==2)&&b->native_action<=5&&o->grounded;
    unsigned air_spirit=!o->valor_style&&!s->move&&o->state==16&&
        ((((b->native_action>=42&&b->native_action<=47)||(b->native_action>=76&&b->native_action<=81))&&
          (b->motion==0x598||b->motion==0x568))||
         (b->native_action>=72&&b->native_action<=75&&
          ((b->motion>=0x599&&b->motion<=0x59c)||b->motion==0x568)));
    if(s->air_sheathe_sample!=b->sample_sequence){
        s->air_sheathe_sample=b->sample_sequence;
        if(!air_spirit)s->air_sheathe_buffer=0;
        else{
            if(b->simulation_delta>0&&b->simulation_delta<30)
                s->air_sheathe_buffer-=b->simulation_delta/60;
            if(!o->grounded&&b->held==(ML_R|ML_B)&&s->air_sheathe_held!=(ML_R|ML_B))
                s->air_sheathe_buffer=.35f;
        }
        s->air_sheathe_held=b->held;
    }
    if(!s->move){
        source=w_guild_source(o->state,b->native_action,b->motion);
        if(!source||!o->grounded){s->controls=(WControls){0};s->gu_chord_wait=0;return (WDecision){0};}
        if(air_spirit&&(b->motion==0x568||b->motion==0x59a||b->motion==0x59c)&&b->animation_frame>=10&&b->animation_frame<60&&
           s->air_sheathe_buffer>0&&!(b->held&(ML_X|ML_A|ML_Y|ML_ZR))){
            s->air_sheathe_buffer=0;
            return enter(s,o,W_SHEATHE,ML_R|ML_B);
        }
        event=w_event(source,idle?0:b->animation_frame);
    }
    /* R and X are unambiguous in the ready sheath. Do not wait for
     * their ordinary ground-combo chord timers before starting Iai. */
    WDecision iai=w_iai_press(s,o);
    if(iai.kind!=W_KEEP)return iai;
    uint32_t permission[8];world_input_permission(source,event,permission);
    if(o->valor_style)permission[WI_SHEATHE.word]&=~(3u<<WI_SHEATHE.shift);
    const float direction[4]={(float)b->input_heading,0,0,0};
    int delta=ml_heading_delta(b->input_heading,b->heading);
    /* Recorded GU input: positive quarter-turn is the hunter's left.
     * Original World motion128 is left,127 is right; legacy preview labels
     * had these reversed and must not define this command binding. */
    unsigned facing=delta>=8192&&delta<=24576?3:
        delta<=-8192&&delta>=-24576?1:delta>24576||delta< -24576?2:0;
    if(s->input_motion!=b->motion){w_controls_bind(&s->controls);s->input_motion=b->motion;s->gu_chord_wait=0;}
    /* A short R+B tap can end before the two Foresight cancel windows.
     * Latch only a fresh chord; the source timeline still decides entry. */
    unsigned sheath_edge=!o->valor_style&&b->held==(ML_R|ML_B)&&
        !(s->controls.buttons[2].raw&&s->controls.buttons[3].raw);
    w_controls_tick(&s->controls,b->actor,b->epoch,b->sample_sequence,b->held,
        b->simulation_delta/60,permission,direction,b->magnitude>=50,facing,facing);
    if(!s->move)world_guild_chords(s,b->held,permission);
    else s->gu_chord_wait=0;
    float frame=b->animation_frame;
    if(s->main_source&&(b->motion_finished||frame>=w_clip(source)->length-1)){
        unsigned main=s->main_source;s->main_source=0;
        return ground_motion(s,main,source,frame,W_MOTION);
    }
    if(s->move==W_SHEATHE){
#if ML_WILDS_MOVES
        if(b->held==ML_B&&!(s->consumed&ML_B)&&w_iai_ready(s,o)){
            WDecision d=w_leave(s);d.kind=W_ROLL;d.sheathed_roll=1;
            d.heading=b->magnitude>=50?b->input_heading:b->heading;return d;
        }
#endif
        if(event&&event->phase==1&&b->simulation_delta>0&&b->simulation_delta<30)
            s->hold_seconds+=b->simulation_delta/60;
        if(w_controls_query(&s->controls,39,1,0))return enter(s,o,W_BIG,ML_R);
        if(w_controls_query(&s->controls,25,1,0))return enter(s,o,W_SMALL,ML_X);
        if(s->hold_seconds>=3.5f)return enter(s,o,W_SHEATHE_END,0);
        return (WDecision){0};
    }
    if(s->move==W_THRUST&&s->hit_success&&o->rank>=1)return enter(s,o,W_RISE,0);
#if ML_WILDS_MOVES
    if(s->move==W_SMALL||s->move==W_BIG){
        WDecision follow=wilds_iai_follow(s,o);
        if(follow.kind!=W_KEEP)return follow;
        /* The command graph can otherwise dispatch a single button while
         * its partner is still within the chord window. */
        if(s->move==W_BIG&&b->held==ML_R&&!b->motion_finished)return (WDecision){0};
        if((s->move==W_SMALL||s->counter_success)&&s->iai_follow_keys&&s->iai_follow_wait>0)
            return (WDecision){0};
    }
#endif
    if(s->move==W_RISE){
        /* The last displayed pose in the two original jump starts is F45.32.
         * GU binds the successor on the next engine update. */
        if(b->motion_finished||frame>=w_clip(source)->length-2)return enter(s,o,W_AIR,0);
        return (WDecision){0};
    }
    if(s->move==W_AIR){
        if(o->grounded&&frame>0)return enter(s,o,W_LAND,0);
        WGroundQuery air={.s=s,.o=o};
        int next=world_query_node(&air,56);
        if(next==58)return enter(s,o,W_DOWN,ML_R|ML_X|ML_A);
        /* The GU air pose has no slash collider; finish the released-input
         * route with an actual downward swing after the manual input window. */
        if(frame>=60)return enter(s,o,W_DOWN,0);
        return (WDecision){0};
    }
    if(s->move==W_DOWN){if(o->grounded&&frame>0)return enter(s,o,W_LAND,0);return (WDecision){0};}
    if(s->move==W_SHEATHE_END){
#if ML_WILDS_MOVES
        if(!o->drawn&&b->held==ML_B&&!(s->consumed&ML_B)){
            WDecision d=w_leave(s);d.kind=W_ROLL;
            d.heading=b->magnitude>=50?b->input_heading:b->heading;return d;
        }
#endif
        if(b->motion_finished||frame>=w_clip(source)->length-1){WDecision d=w_leave(s);d.kind=W_IDLE;return d;}
        return (WDecision){0};
    }
    if(!s->move&&(source==149||source==154)){
        if(w_controls_query(&s->controls,24,1,0))return enter(s,o,W_SHEATHE,ML_R|ML_B);
        if(w_controls_query(&s->controls,60,1,0))
            return enter(s,o,W_FORESIGHT,ML_R|ML_A);
        if(w_controls_query(&s->controls,53,1,0))return enter(s,o,W_THRUST,ML_R|ML_X);
        return (WDecision){0};
    }
#if ML_WILDS_MOVES
    /* World/Foresight/Helm land use the native timing graph. Its X command
     * can still be pending after the pad has returned to neutral. */
    if(s->move&&o->rank==3&&o->grounded&&!(b->held&~ML_X)&&
       w_controls_query(&s->controls,25,1,0))return w_wilds_enter(s,o,W_CHIJIN);
#endif
    unsigned node=s->move==W_GROUND?s->ground_node:world_source_node(source);
    if(node==0xffff||!o->grounded)return (WDecision){0};
    if(s->move==W_FORESIGHT){
        if(sheath_edge&&frame>=13&&frame<90){
            s->queued=W_SHEATHE;s->queue_until=frame<47?47:160;
        }
        if(s->queued==W_SHEATHE){
            if(frame>=s->queue_until||(b->held&(ML_X|ML_A|ML_Y|ML_ZR)))s->queued=0;
            else if(input_stage(event,WI_SHEATHE)==3)return ground_enter(s,o,61,ML_R|ML_B);
        }
    }
    WGroundQuery query={.s=s,.o=o};
    /* Shared weapon-ground special-sheathe transition, before local links.
     * Its own TIML command permission still owns both buffering and entry. */
    if(node!=0&&node!=64&&node!=65&&world_property(&query,11))
        return ground_enter(s,o,61,ML_R|ML_B);
    int next=world_query_node(&query,node);
    if(next>=0){
        WDecision d=world_route(s,o,(unsigned)next);
        if(d.kind!=W_KEEP)return d;
    }
    if(s->move&&event&&((event->input[0]>>8)&3)==3&&
       w_controls_query(&s->controls,17,0,0)){
        WDecision d=w_leave(s);d.kind=W_ROLL;
        d.heading=b->magnitude>=50?b->input_heading:b->heading;return d;
    }
    if(s->move&&(b->motion_finished||frame>=w_clip(source)->length-1)){
#if ML_WILDS_MOVES
        if(s->wilds_charge_link)return wilds_charge_recover(s,o);
#endif
        WDecision d=w_leave(s);d.kind=W_IDLE;return d;
    }
    return (WDecision){0};
}
