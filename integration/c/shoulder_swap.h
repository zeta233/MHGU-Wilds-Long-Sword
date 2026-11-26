#pragma once

enum {
    PAD_PHYSICAL_R=0x0200u, PAD_PHYSICAL_ZR=0x0800u,
    GU_BUTTON_R=0x0040u, GU_BUTTON_ZR=0x0800u,
    GU_BUTTON_X=0x0020u, GU_BUTTON_A=0x0100u
};

static inline unsigned ml_swap_shoulders(unsigned bits,unsigned r,unsigned zr)
{
    return (bits&~(r|zr))|((bits&r)?zr:0)|((bits&zr)?r:0);
}

static inline unsigned ml_gu_shoulders(unsigned bits,unsigned can_sprint,unsigned sprint)
{
    /* GU can swap the native shoulder roles in its own options menu. The
     * patch still gives the Spirit role to physical ZR in that layout. */
    unsigned mapped=sprint==GU_BUTTON_R?ml_swap_shoulders(bits,GU_BUTTON_R,GU_BUTTON_ZR):bits;
    if(can_sprint&&(bits&(GU_BUTTON_X|GU_BUTTON_A))!=(GU_BUTTON_X|GU_BUTTON_A))
        mapped=(mapped&~sprint)|((bits&GU_BUTTON_R)?sprint:0);
    return mapped;
}
