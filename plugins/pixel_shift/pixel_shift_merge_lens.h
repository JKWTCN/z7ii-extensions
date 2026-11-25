/* SPDX-License-Identifier: MIT */
enum { MERGE_CA_NO_PROFILE, MERGE_CA_APPLIED, MERGE_CA_NO_MEMORY, MERGE_CA_RANGE };
enum {
    MERGE_CA_RADIUS_BITS=12, MERGE_CA_RADIUS_SIZE=1<<MERGE_CA_RADIUS_BITS,
    MERGE_CA_COEFFICIENT_ONE=1<<20, MERGE_CA_MAX_MARGIN=16
};

struct merge_ca_profile { int coefficients[2][3]; unsigned present; };

static void merge_ca_read(z72_u32 *file,struct merge_ca_profile *profile)
{
    profile->present=0;
    const unsigned char *outer=nx_entry(nx_scan,0xc7d5);
    if (!outer || nx_u16(outer+2)!=7) return;
    unsigned bytes=nx_u32(outer+4),offset=nx_u32(outer+8);
    unsigned window=bytes<NX_SCAN_BYTES ? bytes : NX_SCAN_BYTES;
    static const unsigned char header[]={'N','i','k','o','n',0,1,1,0,0,'I','I',42,0};
    if (window<24 || offset<8 || offset>file[6] || bytes>file[6]-offset ||
        !nx_read(file,offset,nx_scan,window) || !nx_equal(nx_scan,header,sizeof(header))) return;
    unsigned directory=nx_u32(nx_scan+14);
    if (directory<8 || directory>window-16) return;
    const unsigned char *ifd=nx_scan+10+directory;
    unsigned count=nx_u16(ifd);
    if (count>(window-16-directory)/NX_ENTRY) return;

    const unsigned char *entry=nx_entry((unsigned char *)ifd,7);
    if (!entry || nx_u16(entry+2)!=7) return;
    enum { VALUE_BYTES=88 };
    unsigned size=nx_u32(entry+4),value=nx_u32(entry+8);
    unsigned ifd_end=directory+6+count*NX_ENTRY;
    if (size<VALUE_BYTES || value<ifd_end || value>bytes-10 || size>bytes-10-value) return;
    const unsigned char *data=nx_scan;
    if (value<=window-10 && VALUE_BYTES<=window-10-value) data+=10+value;
    else if (!nx_read(file,offset+10+value,nx_scan,VALUE_BYTES)) return;
    if (!nx_equal(data,(const unsigned char *)"0100",4)) return;

    /* Nikon stores six signed rationals for the red and blue radial scales.
     * The green channel keeps its original coordinates. */
    static const unsigned char fields[2][3]={{44,28,20},{80,64,56}};
    for (unsigned c=0;c<2;++c) {
        for (unsigned term=0;term<3;++term) {
            const unsigned char *p=data+fields[c][term];
            int coefficient=(int)nx_u32(p);
            if (nx_u32(p+4)!=MERGE_CA_COEFFICIENT_ONE || coefficient< -16384 || coefficient>16384) return;
            profile->coefficients[c][term]=coefficient;
        }
    }
    profile->present=1;
}

struct merge_ca_factors { short red,blue; };
struct merge_ca {
    unsigned width,height,stride,margin,capacity,scale;
    unsigned head,input_rows,output_rows;
    int center_x,center_y;       /* Output coordinates in sixteenths of a pixel. */
    struct merge_ca_factors *factors;
    unsigned short *radius_x,*rows;
};

static int merge_ca_factor(const int coeff[3],unsigned radius2)
{
    int t=(int)radius2;
    int inner=coeff[1]+((coeff[2]*t+(MERGE_CA_RADIUS_SIZE/2))>>MERGE_CA_RADIUS_BITS);
    return coeff[0]+((inner*t+(MERGE_CA_RADIUS_SIZE/2))>>MERGE_CA_RADIUS_BITS);
}

static unsigned merge_ca_radius(int delta,unsigned scale)
{
    /* Q30 reciprocal of half the 8288 x 5520 RAW diagonal. */
    enum { INV_RADIUS=215655 };
    unsigned shift=18+(scale==2u);
    unsigned normalized=(unsigned)(((unsigned long long)merge_abs(delta)*INV_RADIUS+
                                    (1u<<(shift-1)))>>shift);
    return (normalized*normalized+(1u<<19))>>20;
}

static unsigned merge_ca_init(struct merge_ca *state,const struct merge_ca_profile *profile,
                              struct merge_offset position,unsigned scale,void **owner)
{
    state->rows=0;
    if (!profile->present) return MERGE_CA_NO_PROFILE;

    unsigned peak=0;
    for (unsigned i=0;i<MERGE_CA_RADIUS_SIZE;++i) {
        unsigned red=merge_abs(merge_ca_factor(profile->coefficients[0],i));
        unsigned blue=merge_abs(merge_ca_factor(profile->coefficients[1],i));
        if (red>peak) peak=red;
        if (blue>peak) peak=blue;
    }
    if (!peak) return MERGE_CA_NO_PROFILE;
    state->width=MERGE_WIDTH*scale;
    state->height=MERGE_HEIGHT*scale;
    state->stride=state->width*3;
    state->scale=scale;
    /* Match the RAW optical center used at merge, translated into our crop. */
    state->center_x=((MERGE_RAW_WIDTH-1)*8-16*(MERGE_LEFT+1)+position.x)*(int)scale;
    state->center_y=((MERGE_RAW_HEIGHT-1)*8-16*(MERGE_TOP+1)+position.y)*(int)scale;
    unsigned far=merge_abs(state->center_y);
    unsigned lower=merge_abs((int)((state->height-1)*16)-state->center_y);
    if (lower>far) far=lower;
    unsigned margin=(unsigned)(((unsigned long long)peak*far+(1u<<24)-1)>>24);
    if (margin>MERGE_CA_MAX_MARGIN*scale) return MERGE_CA_RANGE;
    state->margin=margin;
    state->capacity=2*margin+1;
    state->head=state->input_rows=state->output_rows=0;

    unsigned lookup_bytes=MERGE_CA_RADIUS_SIZE*sizeof(*state->factors)+state->width*sizeof(*state->radius_x);
    state->factors=nx_alloc(lookup_bytes+state->capacity*state->stride*sizeof(*state->rows),owner);
    if (!state->factors) return MERGE_CA_NO_MEMORY;
    state->radius_x=(unsigned short *)(state->factors+MERGE_CA_RADIUS_SIZE);
    state->rows=state->radius_x+state->width;
    for (unsigned i=0;i<MERGE_CA_RADIUS_SIZE;++i) {
        state->factors[i].red=(short)merge_ca_factor(profile->coefficients[0],i);
        state->factors[i].blue=(short)merge_ca_factor(profile->coefficients[1],i);
    }
    for (unsigned x=0;x<state->width;++x)
        state->radius_x[x]=(unsigned short)merge_ca_radius((int)(16*x)-state->center_x,scale);
    return MERGE_CA_APPLIED;
}

static const unsigned short *merge_ca_source(const struct merge_ca *state,unsigned y)
{
    unsigned slot=state->head+state->capacity-(state->input_rows-y);
    if (slot>=state->capacity) slot-=state->capacity;
    return state->rows+slot*state->stride;
}

/* Convert the signed Q20 radial scale and Q4 distance into a Q8 coordinate. */
static inline __attribute__((always_inline))
unsigned merge_ca_coordinate(int pos,int delta,int factor,unsigned maximum)
{
    int source=pos+((factor*delta+32768)>>16);
    if (source<=0) return 0;
    return (unsigned)source>maximum ? maximum : (unsigned)source;
}

static inline __attribute__((always_inline))
unsigned short merge_ca_sample(const unsigned short *const *rows,unsigned first,unsigned channel,
                               unsigned x,unsigned y)
{
    unsigned left=3*(x>>8)+channel,right=3*((x+255)>>8)+channel;
    const unsigned short *a=rows[(y>>8)-first],*b=rows[((y+255)>>8)-first];
    unsigned wx=x&255u,wy=y&255u;
    unsigned top=(256-wx)*a[left]+wx*a[right];
    unsigned bottom=(256-wx)*b[left]+wx*b[right];
    return (unsigned short)(((256-wy)*top+wy*bottom+32768u)>>16);
}

static void merge_ca_row(struct merge_ca *state,unsigned short *output)
{
    unsigned y=state->output_rows++,margin=state->margin;
    unsigned first=y>margin ? y-margin : 0;
    unsigned last=y+margin;
    if (last>=state->height) last=state->height-1;
    const unsigned short *rows[2*MERGE_CA_MAX_MARGIN*2+1];
    for (unsigned i=first;i<=last;++i) rows[i-first]=merge_ca_source(state,i);
    const unsigned short *green=rows[y-first]+1;
    unsigned max_x=(state->width-1)*256,max_y=(state->height-1)*256;
    int dy=(int)(16*y)-state->center_y,dx=-state->center_x;
    unsigned radius_y=merge_ca_radius(dy,state->scale);
    int py=(int)(y*256),px=0;
    for (unsigned x=0;x<state->width;++x,output+=3,green+=3,dx+=16,px+=256) {
        unsigned radius=state->radius_x[x]+radius_y;
        if (radius>=MERGE_CA_RADIUS_SIZE) radius=MERGE_CA_RADIUS_SIZE-1;
        int red=state->factors[radius].red,blue=state->factors[radius].blue;
        unsigned rx=merge_ca_coordinate(px,dx,red,max_x),ry=merge_ca_coordinate(py,dy,red,max_y);
        unsigned bx=merge_ca_coordinate(px,dx,blue,max_x),by=merge_ca_coordinate(py,dy,blue,max_y);
        output[0]=merge_ca_sample(rows,first,0,rx,ry);
        output[1]=*green;
        output[2]=merge_ca_sample(rows,first,2,bx,by);
    }
}

/* Reuse the accumulated band for corrected output. Each output row replaces
 * a row already saved in the ring. Empty input drains one output band. */
static unsigned merge_ca_band(struct merge_ca *state,unsigned short *band,unsigned input)
{
    unsigned output=0,stride=state->stride;
    if (!input) {
        unsigned capacity=MERGE_ROWS*state->scale;
        while (output<capacity && state->output_rows<state->height) {
            merge_ca_row(state,band+output*stride);
            ++output;
        }
        return output;
    }
    for (unsigned i=0;i<input;++i) {
        __aeabi_memcpy4(state->rows+state->head*stride,band+i*stride,stride*2);
        if (++state->head==state->capacity) state->head=0;
        if (++state->input_rows>state->output_rows+state->margin) {
            merge_ca_row(state,band+output*stride);
            ++output;
        }
    }
    return output;
}
