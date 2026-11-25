/* Measured from the World capture; PCM starts are already reproduced by the audio cues. */
static float world_iai_line_delay(unsigned rank)
{
    return rank==1?.710f:rank==2?.715f:.735f;
}
static float world_iai_tail_width(float age)
{
    return age<=.50f?1.f:age<.88f?(.88f-age)/.38f:0.f;
}
static void world_iai_shrink_width(float *v,float width)
{
    for(unsigned end=0;end<2;++end)for(unsigned k=0;k<3;++k){
        float *x=v+end*4+k,*y=x+8,middle=(*x+*y)*.5f;
        *x=middle+(*x-middle)*width;*y=middle+(*y-middle)*width;
    }
}
static void world_iai_stroke(float *v,float age)
{
    float u=age/.08f;if(u<0)u=0;if(u>1)u=1;
    float reveal=.025f+.975f*u;
    float width=age<=.08f?1:age<.18f?1-.7f*(age-.08f)/.10f:
        age<.5f?.30f-.18f*(age-.18f)/.32f:
        .12f*world_iai_tail_width(age);
    for(unsigned i=0;i<2;++i){
        float *near=v+8*i,*far=near+4;
        for(unsigned k=0;k<3;++k){
            float span=far[k]-near[k],pivot=near[k]+span*.124f;
            near[k]=pivot+(near[k]-pivot)*reveal;
            far[k]=pivot+(far[k]-pivot)*reveal;
        }
    }
    world_iai_shrink_width(v,width);
}
