#ifndef DOUBLE_DRAW_DATA_H
#define DOUBLE_DRAW_DATA_H
enum { DOUBLE_DRAW_BASE=72, DOUBLE_DRAW_STEP_OFFSET=25, DOUBLE_DRAW_END=39, DOUBLE_DRAW_RETURN=76 };
static inline unsigned double_draw_slot(unsigned slot){
    return slot>=DOUBLE_DRAW_BASE&&slot<DOUBLE_DRAW_BASE+4;
}
static inline unsigned double_draw_offset(unsigned slot){
    return (slot-DOUBLE_DRAW_BASE)&2?DOUBLE_DRAW_STEP_OFFSET:0;
}
static inline unsigned double_draw_motion(unsigned motion){
    return motion>=0x500+DOUBLE_DRAW_BASE&&motion<=0x500+DOUBLE_DRAW_RETURN;
}
#endif
