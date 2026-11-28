/* Use the active rModel's own weighted geometry for surface-locked wounds. */
typedef struct {float x,y,z;} WSurfaceVec;
typedef struct {
    const void *model;
    unsigned mesh;
    unsigned short vertex[3];
    float u,v;
} WSurfaceAnchor;
typedef struct {
    const void *model,*meshes,*positions,*indices,*rest,*posed,*body;
    unsigned mesh_count,bones,vertex_bytes,index_count;
    unsigned char skin_ready[256];
} WSurfaceModel;
typedef struct {
    const void *positions,*indices;
    unsigned count,indices_count,first,stride,format,single;
} WSurfaceMesh;
static struct {WSurfaceVec p;float affinity;} wound_vertices[8192];
static float wound_skin[256][12];

static WSurfaceVec surface_sub(WSurfaceVec a,WSurfaceVec b)
{return (WSurfaceVec){a.x-b.x,a.y-b.y,a.z-b.z};}
static WSurfaceVec surface_add(WSurfaceVec a,WSurfaceVec b,float t)
{return (WSurfaceVec){a.x+b.x*t,a.y+b.y*t,a.z+b.z*t};}
static float surface_dot(WSurfaceVec a,WSurfaceVec b)
{return a.x*b.x+a.y*b.y+a.z*b.z;}
static WSurfaceVec surface_cross(WSurfaceVec a,WSurfaceVec b)
{return (WSurfaceVec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float surface_half(unsigned v)
{
    unsigned exponent=(v>>10)&31u,mantissa=v&1023u;
    if(!exponent)return (v&32768?-1.f:1.f)*mantissa/16777216.f;
    if(exponent==31)return 0;
    union {u32 bits;float value;} f={.bits=((v&32768u)<<16)|((exponent+112)<<23)|(mantissa<<13)};
    return f.value;
}

static unsigned surface_model(WSurfaceModel *m,void *target)
{
    const void *r=(const void *)(uptr)word(target,0xf0);
    if(!r||word(r,0)!=main_base+0x1792f34||!(byte(r,0x50)&1))return 0;
    const void *vertices=(const void *)(uptr)word(r,0xa4),*indices=(const void *)(uptr)word(r,0xa0);
    if(!vertices||!indices)return 0;
    *m=(WSurfaceModel){.model=r,.body=(const void *)(uptr)word(r,0x64),
        .meshes=(const void *)(uptr)word(r,0x74),.mesh_count=word(r,0x78),
        .positions=(const void *)(uptr)word(vertices,0x18),.vertex_bytes=word(vertices,0x14),
        .indices=(const void *)(uptr)word(indices,0x18),.index_count=word(indices,0x14)/2,
        .rest=(const void *)(uptr)word(r,0x70),.bones=word(r,0x68),
        .posed=(const void *)(uptr)word(target,0x494)};
    if(!m->body||!m->meshes||!m->positions||!m->indices||!m->rest||!m->posed||
       !m->bones||m->bones>256||m->bones!=word(target,0x490)||m->mesh_count>256)return 0;
    return 1;
}

/* A wound samples just three skinned vertices. Prepare the bones those
 * vertices reference, while keeping placement's full mesh pass reusable. */
static const float *surface_skin(WSurfaceModel *m,unsigned n)
{
    float *skin=wound_skin[n];
    if(!m->skin_ready[n]){
        const float *p=(const float *)((uptr)m->posed+160*n+0x10);
        const float *r=(const float *)((uptr)m->rest+64*n);
        for(unsigned j=0;j<4;++j)
            for(unsigned k=0;k<3;++k)
                skin[3*j+k]=p[k]*r[4*j]+p[4+k]*r[4*j+1]+p[8+k]*r[4*j+2]+(j==3?p[12+k]:0);
        m->skin_ready[n]=1;
    }
    return skin;
}

static unsigned surface_mesh(const WSurfaceModel *m,void *target,unsigned id,WSurfaceMesh *mesh)
{
    const void *row=(const void *)((uptr)m->meshes+48*id);
    unsigned group=byte(row,4),stride=byte(row,10),count=u16(row,2),base=word(row,12);
    unsigned vb=word(row,16),ib=word(row,24),length=word(row,28),type=word(row,20)>>12;
    if(group>=100||!(word(target,0x110+4*(group>>5))&(1u<<(group&31)))||
       !stride||base>65535||vb>m->vertex_bytes||ib>m->index_count||
       count>sizeof(wound_vertices)/sizeof(*wound_vertices)||base+count>(m->vertex_bytes-vb)/stride||
       length>m->index_count-ib)return 0;
    unsigned format=0,minimum=0;
    switch(type){
    case 0x0cb68:format=1;minimum=16;break;
    case 0xb0983:case 0xa8fab:case 0x01cb8:case 0x667b1:case 0xc9cfc:case 0xd8778:case 0x707fb:
        format=2;minimum=8;break;
    case 0xdb7da:format=3;minimum=16;break;
    case 0x14d40:case 0xbde53:case 0x77d87:case 0xd86ca:case 0xda55a:case 0x64593:
        format=4;minimum=28;break;
    case 0xcc510:format=5;minimum=24;break;
    case 0xb3921:case 0x1b9a2:format=6;minimum=20;break;
    case 0xc31f2:case 0x6a2e1:case 0xa0135:case 0x0d9e8:format=7;minimum=24;break;
    case 0xcbf6c:format=8;minimum=6;break;
    case 0xa320c:format=9;minimum=24;break;
    case 0xbb424:case 0x12737:case 0x75c3e:case 0xd84e3:case 0xcbcf7:
        format=10;minimum=32;break;
    default:return 0;
    }
    if(stride<minimum)return 0;
    *mesh=(WSurfaceMesh){.positions=(const void *)((uptr)m->positions+vb+base*stride),
        .indices=(const void *)((uptr)m->indices+2*ib),.count=count,.indices_count=length,
        .first=base,.stride=stride,.format=format,.single=byte(row,8)};
    return 1;
}

static unsigned surface_vertex(WSurfaceModel *m,const WSurfaceMesh *mesh,unsigned id,
                               WSurfaceVec *position,const float *affinity,float *score)
{
    const void *p=(const void *)((uptr)mesh->positions+mesh->stride*id);
    unsigned bone[8]={0},count=1;
    float weight[8]={1.f};
    float packed=(int16_t)u16(p,6)*(1.f/32767);
    switch(mesh->format){
    case 1:
        count=4;
        for(unsigned i=0;i<4;++i){bone[i]=byte(p,8+i);weight[i]=byte(p,12+i)*(1.f/255);}
        break;
    case 2:bone[0]=u16(p,6);break;
    case 3:case 6:case 7:{
        unsigned off=mesh->format==3?12:mesh->format==6?16:20;
        count=2;weight[0]=packed;weight[1]=1.f-packed;
        for(unsigned i=0;i<2;++i){
            if(mesh->format==3)bone[i]=u16(p,off+2*i);
            else{
                float index=surface_half(u16(p,off+2*i));
                if(!(index>=0&&index<256))return 0;
                bone[i]=(unsigned)index;
            }
        }
        break;
    }
    case 4:case 5:{
        unsigned off=mesh->format==4?24:20;
        count=4;weight[0]=packed;weight[1]=surface_half(u16(p,off));weight[2]=surface_half(u16(p,off+2));
        weight[3]=1.f-weight[0]-weight[1]-weight[2];
        for(unsigned i=0;i<4;++i)bone[i]=byte(p,16+i);
        break;
    }
    case 8:bone[0]=mesh->single==255?0:mesh->single;break;
    case 9:
        count=6;
        for(unsigned i=0;i<6;++i){
            bone[i]=byte(p,8+i);
            weight[i]=i<4?byte(p,16+i)*(1.f/255):surface_half(u16(p,12+2*i));
        }
        break;
    case 10:{
        count=8;float sum=packed;
        weight[0]=packed;
        for(unsigned i=1;i<7;++i){
            weight[i]=i<5?byte(p,11+i)*(1.f/255):surface_half(u16(p,18+2*i));
            sum+=weight[i];
        }
        weight[7]=1.f-sum;
        for(unsigned i=0;i<8;++i)bone[i]=byte(p,16+i);
        break;
    }
    }
    WSurfaceVec pos={0};float total=0,match=0;
    float x=(int16_t)u16(p,0)*(1.f/32767),y=(int16_t)u16(p,2)*(1.f/32767),z=(int16_t)u16(p,4)*(1.f/32767);
    for(unsigned i=0;i<count;++i){
        float w=weight[i];if(w<=0)continue;
        if(bone[i]>=m->bones)return 0;
        const float *skin=surface_skin(m,bone[i]);
        pos=surface_add(pos,(WSurfaceVec){skin[0]*x+skin[3]*y+skin[6]*z+skin[9],
            skin[1]*x+skin[4]*y+skin[7]*z+skin[10],
            skin[2]*x+skin[5]*y+skin[8]*z+skin[11]},w);
        total+=w;
        if(affinity)match+=w*affinity[bone[i]];
    }
    if(!(total>.9f&&total<1.1f))return 0;
    *position=(WSurfaceVec){pos.x/total,pos.y/total,pos.z/total};
    if(score)*score=match/total;
    return 1;
}

static unsigned surface_vertices(WSurfaceModel *m,const WSurfaceMesh *mesh,const float *affinity)
{
    for(unsigned i=0;i<mesh->count;++i)
        if(!surface_vertex(m,mesh,i,&wound_vertices[i].p,affinity,&wound_vertices[i].affinity))return 0;
    return 1;
}

static unsigned surface_face(const WSurfaceMesh *mesh,unsigned n,unsigned *id)
{
    for(unsigned k=0;k<3;++k){
        unsigned v=u16(mesh->indices,2*(n+k));
        if(v<mesh->first||v-mesh->first>=mesh->count)return 0;
        id[k]=v-mesh->first;
    }
    return id[0]!=id[1]&&id[0]!=id[2]&&id[1]!=id[2];
}

static float surface_closest(WSurfaceVec p,WSurfaceVec a,WSurfaceVec b,WSurfaceVec c,float *u,float *v)
{
    WSurfaceVec edge1=surface_sub(b,a),edge2=surface_sub(c,a),ap=surface_sub(p,a);
    float aa=surface_dot(edge1,edge1),bb=surface_dot(edge2,edge2),ab=surface_dot(edge1,edge2);
    float pa=surface_dot(ap,edge1),pb=surface_dot(ap,edge2),det=aa*bb-ab*ab;
    if(det>.0001f){
        float x=(bb*pa-ab*pb)/det,y=(aa*pb-ab*pa)/det;
        if(x>=0&&y>=0&&x+y<=1){
            *u=x;*v=y;
            WSurfaceVec d=surface_sub(ap,surface_add(surface_add((WSurfaceVec){0},edge1,x),edge2,y));
            return surface_dot(d,d);
        }
    }
    WSurfaceVec ends[3]={a,b,c};
    float best=1.e20f;
    *u=*v=0;
    for(unsigned i=0;i<3;++i){
        WSurfaceVec from=ends[i],e=surface_sub(ends[(i+1)%3],from),d=surface_sub(p,from);
        float len=surface_dot(e,e),t=len>.0001f?surface_dot(d,e)/len:0;
        if(t<0)t=0;if(t>1)t=1;
        d=surface_add(d,e,-t);
        float distance=surface_dot(d,d);
        if(distance<best){best=distance;*u=i==0?t:i==1?1.f-t:0;*v=i==0?0:i==1?t:1.f-t;}
    }
    return best;
}

static unsigned surface_bind(void *target,const void *part,const float *contact,WSurfaceAnchor *anchor)
{
    WSurfaceModel m;
    if(!surface_model(&m,target))return 0;
    const void *remap=(const void *)(uptr)word(target,0x498);
    if(!remap)return 0;
    unsigned ids[2]={u16(part,0),u16(part,2)},bones[2]={255,255};
    for(unsigned i=0;i<2;++i)if(ids[i]<255)bones[i]=byte(remap,ids[i]);
    float affinity[256]={0};
    for(unsigned j=0;j<m.bones;++j){
        for(unsigned k=0;k<2;++k){
            unsigned b=bones[k];if(b>=m.bones)continue;
            float score=j==b?1.f:byte(m.body,j*24+1)==b||byte(m.body,b*24+1)==j?.56f:0;
            if(score>affinity[j])affinity[j]=score;
        }
    }
    WSurfaceVec p={contact[0],contact[1],contact[2]};
    float max=real(part,16)*2.f+36.f,max2=max*max,best=1.e20f;
    unsigned have=0,strict=bones[0]<m.bones||bones[1]<m.bones;
    for(unsigned j=0;j<m.mesh_count;++j){
        WSurfaceMesh mesh;
        if(!surface_mesh(&m,target,j,&mesh)||!surface_vertices(&m,&mesh,affinity))continue;
        unsigned start=0;
        for(unsigned n=0;n+2<mesh.indices_count;++n){
            if(u16(mesh.indices,2*(n+2))==65535){start=n+3;continue;}
            if(n<start)continue;
            unsigned id[3];if(!surface_face(&mesh,n,id))continue;
            if((n-start)&1){unsigned swap=id[1];id[1]=id[2];id[2]=swap;}
            WSurfaceVec a=wound_vertices[id[0]].p,b=wound_vertices[id[1]].p,c=wound_vertices[id[2]].p;
            WSurfaceVec normal=surface_cross(surface_sub(b,a),surface_sub(c,a));
            if(surface_dot(normal,normal)<=.0001f)continue;
            float x=wound_vertices[id[0]].affinity+wound_vertices[id[1]].affinity+wound_vertices[id[2]].affinity;
            if(strict&&x<.25f)continue;
            float u,v,d=surface_closest(p,a,b,c,&u,&v);
            float score=d*(strict?1.f+(3.f-x)*.45f:1.f);
            if(d<max2&&score<best){
                best=score;have=1;
                *anchor=(WSurfaceAnchor){.model=m.model,.mesh=j,.vertex={id[0],id[1],id[2]},.u=u,.v=v};
            }
        }
    }
    return have;
}

static unsigned surface_sample(WSurfaceModel *m,void *target,const WSurfaceAnchor *a,
                               WSurfaceVec *point,WSurfaceVec *normal,WSurfaceVec *edge)
{
    WSurfaceMesh mesh;
    if(a->model!=m->model||a->mesh>=m->mesh_count||!surface_mesh(m,target,a->mesh,&mesh))return 0;
    WSurfaceVec p[3];
    for(unsigned k=0;k<3;++k)
        if(a->vertex[k]>=mesh.count||!surface_vertex(m,&mesh,a->vertex[k],p+k,0,0))return 0;
    WSurfaceVec x=surface_sub(p[1],p[0]),y=surface_sub(p[2],p[0]);
    *point=surface_add(surface_add(p[0],x,a->u),y,a->v);
    *normal=surface_cross(x,y);
    if(edge)*edge=x;
    return surface_dot(*normal,*normal)>.0001f;
}
