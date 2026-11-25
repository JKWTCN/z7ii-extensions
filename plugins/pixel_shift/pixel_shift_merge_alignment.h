/* SPDX-License-Identifier: MIT */
enum {
    MERGE_SHIFT_ONE=16, MERGE_SHIFT_LIMIT=8,
    MERGE_PROBE_BANDS=64, MERGE_PROBE_TILES=8, MERGE_PROBE_POINTS=32,
    MERGE_PROBE_SAMPLES=MERGE_PROBE_POINTS*MERGE_PROBE_BANDS*2
};

struct merge_offset { int x,y; };
struct merge_alignment {
    int x,y;                      /* Integer source coordinates relative to the output band. */
    int fx,fy;                    /* Remaining source offset in sixteenths of a RAW pixel. */
    unsigned subx,suby;           /* Output phase on the doubled grid. */
};

static struct merge_alignment merge_nominal(unsigned frame,unsigned scale)
{
    static const unsigned char x[]={0,1,1,0}, y[]={0,0,1,1};
    unsigned phase=scale==2 ? (frame>>2)&3u : 0;
    /* In the RAW, the image moves left for a positive X command and down
     * for a positive Y command. A half-Y step uses the next source row
     * to land on the positive half-row of the output grid. */
    return (struct merge_alignment){-(int)x[frame&3u],(int)(y[frame&3u]+y[phase]),
                                    0,0,x[phase],y[phase]};
}

static struct merge_offset merge_nominal_offset(const struct merge_alignment *plan)
{
    return (struct merge_offset){plan->x*MERGE_SHIFT_ONE-(int)plan->subx*8,
                                 plan->y*MERGE_SHIFT_ONE-(int)plan->suby*8};
}

static unsigned merge_abs(int value)
{
    return (unsigned)(value<0 ? -value : value);
}

static int merge_near(struct merge_offset a,struct merge_offset b,unsigned limit)
{
    return merge_abs(a.x-b.x)<=limit && merge_abs(a.y-b.y)<=limit;
}

/* The probe compares the same green phase in every RAW. Gradients and
 * differences are in linear sensor values, before white balance. */
struct merge_probe_point { short value,gx,gy; };

static struct merge_probe_point merge_probe_point(const unsigned short *p)
{
    return (struct merge_probe_point){(short)*p,(short)((int)p[2]-p[-2]),
                                     (short)((int)p[2*MERGE_RAW_WIDTH]-p[-2*MERGE_RAW_WIDTH])};
}

struct merge_probe_fit {
    long long xx,xy,yy,xd,yd;
    int x,y,d;
};

static void merge_probe_band(const unsigned short *raw,struct merge_probe_point *reference,
                             struct merge_probe_fit *fit)
{
    for (unsigned row=2;row<=4;row+=2) {
        const unsigned short *p=raw+row*MERGE_RAW_WIDTH+MERGE_LEFT+17;
        for (unsigned tile=0;tile<MERGE_PROBE_TILES;++tile) {
            for (unsigned n=0;n<MERGE_PROBE_POINTS;++n,p+=32,++reference) {
                struct merge_probe_point point=merge_probe_point(p);
                if (!fit) {
                    *reference=point;
                    continue;
                }
                int gx=(point.gx+reference->gx)>>3;
                int gy=(point.gy+reference->gy)>>3;
                int d=reference->value-point.value;
                fit[tile].xx+=gx*gx;
                fit[tile].xy+=gx*gy;
                fit[tile].yy+=gy*gy;
                fit[tile].xd+=gx*d;
                fit[tile].yd+=gy*d;
                fit[tile].x+=gx;
                fit[tile].y+=gy;
                fit[tile].d+=d;
            }
        }
    }
}

static int merge_fit_shift(long long numerator,long long denominator)
{
    /* Round to a sixteenth of a pixel without requiring a runtime divider. */
    unsigned value=0;
    long long target=(numerator<0 ? -numerator : numerator)*MERGE_SHIFT_ONE+denominator/2;
    for (unsigned bit=32;bit;bit>>=1)
        if (denominator*(value|bit)<=target) value|=bit;
    return numerator<0 ? -(int)value : (int)value;
}

static int merge_fit_tile(const struct merge_probe_fit *fit,struct merge_offset *offset)
{
    /* Center the terms so small exposure differences do not look like motion. */
    long long xx=fit->xx-(long long)fit->x*fit->x/MERGE_PROBE_SAMPLES;
    long long xy=fit->xy-(long long)fit->x*fit->y/MERGE_PROBE_SAMPLES;
    long long yy=fit->yy-(long long)fit->y*fit->y/MERGE_PROBE_SAMPLES;
    long long xd=fit->xd-(long long)fit->x*fit->d/MERGE_PROBE_SAMPLES;
    long long yd=fit->yd-(long long)fit->y*fit->d/MERGE_PROBE_SAMPLES;
    if (xx<MERGE_PROBE_SAMPLES*16 || yy<MERGE_PROBE_SAMPLES*16) return 0;
    /* Bound the products in the two-variable solve. */
    while ((xx|yy|(xd<0 ? -xd : xd)|(yd<0 ? -yd : yd))>=1LL<<23) {
        xx>>=1; xy>>=1; yy>>=1; xd>>=1; yd>>=1;
    }
    long long det=xx*yy-xy*xy;
    if (!xx || !yy || det<=xx*yy/4) return 0;
    *offset=(struct merge_offset){merge_fit_shift(yy*xd-xy*yd,det),
                                  merge_fit_shift(xx*yd-xy*xd,det)};
    return 1;
}

static int merge_select_fit(const struct merge_probe_fit fit[MERGE_PROBE_TILES],
                            struct merge_offset nominal,struct merge_offset initial,
                            struct merge_offset *result)
{
    struct merge_offset offsets[MERGE_PROBE_TILES];
    unsigned count=0,best=1,best_distance=2*MERGE_SHIFT_LIMIT+1;
    for (unsigned i=0;i<MERGE_PROBE_TILES;++i) {
        struct merge_offset candidate;
        if (merge_fit_tile(fit+i,&candidate) &&
            merge_near(candidate,nominal,MERGE_SHIFT_LIMIT) &&
            merge_near(candidate,initial,MERGE_SHIFT_LIMIT))
            offsets[count++]=candidate;
    }
    /* Pick the estimate with the most agreeing tiles. If support ties,
     * prefer the one closer to the NEF position. */
    for (unsigned i=0;i<count;++i) {
        unsigned support=0;
        for (unsigned j=0;j<count;++j) support+=merge_near(offsets[i],offsets[j],3);
        unsigned distance=merge_abs(offsets[i].x-initial.x)+merge_abs(offsets[i].y-initial.y);
        if (support>=2 && (support>best || (support==best && distance<best_distance))) {
            *result=offsets[i];
            best=support;
            best_distance=distance;
        }
    }
    return best>=2;
}
