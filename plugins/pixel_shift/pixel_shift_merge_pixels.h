/* SPDX-License-Identifier: MIT */
enum {
    MERGE_RAW_WIDTH=8288, MERGE_RAW_HEIGHT=5520,
    MERGE_WIDTH=8256, MERGE_HEIGHT=5504, MERGE_LEFT=16, MERGE_TOP=8,
    MERGE_ROW_BYTES=(MERGE_RAW_WIDTH*14/8+15)&~15, MERGE_ROWS=8, MERGE_HALO=2,
    MERGE_INPUT_BYTES=(MERGE_ROW_BYTES*(MERGE_ROWS+MERGE_HALO)+63)&~63
};
#include "pixel_shift_merge_alignment.h"
_Static_assert(MERGE_LEFT>=1+MERGE_HALO && MERGE_LEFT+MERGE_WIDTH+MERGE_HALO<=MERGE_RAW_WIDTH &&
               MERGE_TOP>=MERGE_HALO && MERGE_TOP+MERGE_HEIGHT+2+MERGE_HALO<=MERGE_RAW_HEIGHT &&
               MERGE_HEIGHT%MERGE_ROWS==0,"Shifted merge bands stay inside the RAW strip");

static void merge_unpack_packed(unsigned short *pixels,const unsigned char *packed,unsigned rows)
{
    for (unsigned y=0;y<rows;++y) {
        const unsigned char *row=packed+y*MERGE_ROW_BYTES;
        for (unsigned x=0;x<MERGE_RAW_WIDTH;x+=4,row+=7,pixels+=4) {
            pixels[0]=(row[0]|(unsigned)row[1]<<8)&0x3fffu;
            pixels[1]=((row[1]>>6)|(unsigned)row[2]<<2|(unsigned)row[3]<<10)&0x3fffu;
            pixels[2]=((row[3]>>4)|(unsigned)row[4]<<4|(unsigned)row[5]<<12)&0x3fffu;
            pixels[3]=(row[5]>>2)|(unsigned)row[6]<<6;
        }
    }
}

static inline unsigned merge_interpolate(const unsigned short *p,int xstep,int ystep,
                                         unsigned a,unsigned b,unsigned c,unsigned d)
{
    return (a*p[0]+b*p[xstep]+c*p[ystep]+d*p[ystep+xstep]+512)>>10;
}

static void merge_accumulate(unsigned short *band,const unsigned short *pixels,
                             const struct merge_alignment *plan,unsigned scale)
{
    static const unsigned char channels[2][2]={{0,1},{1,2}};
    unsigned xphase=(unsigned)plan->x&1u, yphase=(unsigned)plan->y&1u;
    unsigned wx=merge_abs(plan->fx),wy=merge_abs(plan->fy);
    /* Neighbors are two RAW pixels apart, on the same Bayer color. */
    int xstep=wx ? (plan->fx<0 ? -MERGE_HALO : MERGE_HALO) : 0;
    int ystep=wy ? (plan->fy<0 ? -MERGE_HALO : MERGE_HALO)*MERGE_RAW_WIDTH : 0;
    unsigned a=(32-wx)*(32-wy),b=wx*(32-wy),c=(32-wx)*wy,d=wx*wy;
    unsigned step=3*scale,stride=MERGE_WIDTH*step;

    for (unsigned y=0;y<MERGE_ROWS;++y) {
        const unsigned short *row=pixels+y*MERGE_RAW_WIDTH+MERGE_LEFT+plan->x;
        unsigned short *pixel=band+(y*scale+plan->suby)*stride+3*plan->subx;
        const unsigned char *cfa=channels[(y+yphase)&1u];
        unsigned first=cfa[xphase],second=step+cfa[xphase^1u];
        if (!wx && !wy) {
            for (unsigned x=0;x<MERGE_WIDTH;x+=2,pixel+=2*step) {
                pixel[first]+=row[x];
                pixel[second]+=row[x+1];
            }
        } else {
            for (unsigned x=0;x<MERGE_WIDTH;x+=2,pixel+=2*step) {
                pixel[first]+=(unsigned short)merge_interpolate(row+x,xstep,ystep,a,b,c,d);
                pixel[second]+=(unsigned short)merge_interpolate(row+x+1,xstep,ystep,a,b,c,d);
            }
        }
    }
}

static inline __attribute__((always_inline))
unsigned short merge_srgb_sample(const unsigned short *lut,unsigned value)
{
    unsigned lo=lut[value>>8],hi=lut[(value>>8)+1];
    return (unsigned short)(lo+((hi-lo)*(value&255u)+128u)/256u);
}

#if defined(__arm__) && defined(__ARM_FEATURE_DSP)
static inline __attribute__((always_inline))
unsigned merge_matrix_channel(unsigned rg,int b,int cr,int cg,int cb)
{
    unsigned coefficients=(unsigned short)cr|((unsigned)(unsigned short)cg<<16);
    /* Undo the signed halfword bias applied when packing R and G. */
    int blue=b*cb+32768*(cr+cg);
    int sum;
    unsigned value;
    __asm__("smlad %0, %1, %2, %3" : "=r"(sum) : "r"(rg),"r"(coefficients),"r"(blue) : "cc");
    __asm__("usat %0, #16, %1, asr #10" : "=r"(value) : "r"(sum) : "cc");
    return value;
}
#endif

/* Linear sRGB from white-balanced camera RGB, Q12. Derived by inverting
 * the row-normalized Z7 II XYZ-to-camera matrix with sRGB primaries.
 * https://github.com/LibRaw/LibRaw/blob/master/src/tables/colordata.cpp#L999 */
static void merge_render(unsigned short *band,unsigned pixels,unsigned repeated,
                         const unsigned black[3],const unsigned gain[3])
{
    /* IEC sRGB transfer function at the endpoints of 256 equal intervals. */
    static const unsigned short srgb[257]={
        0,3255,5552,7237,8618,9809,10867,11827,12710,13531,14300,15025,
        15713,16368,16995,17595,18173,18730,19269,19790,20295,20786,21263,21728,
        22181,22624,23056,23478,23892,24297,24694,25083,25465,25840,26209,26571,
        26927,27278,27623,27963,28298,28627,28953,29273,29590,29902,30210,30515,
        30815,31112,31406,31696,31983,32266,32547,32824,33099,33370,33639,33906,
        34169,34430,34689,34945,35199,35450,35699,35947,36191,36434,36675,36914,
        37151,37385,37619,37850,38079,38307,38533,38757,38980,39201,39420,39638,
        39854,40069,40282,40494,40705,40914,41122,41328,41533,41737,41939,42141,
        42341,42539,42737,42934,43129,43323,43516,43708,43899,44089,44277,44465,
        44652,44837,45022,45206,45388,45570,45751,45931,46110,46288,46465,46642,
        46817,46992,47166,47339,47511,47682,47853,48023,48192,48360,48527,48694,
        48860,49025,49190,49354,49517,49679,49841,50002,50162,50322,50481,50639,
        50797,50954,51111,51266,51422,51576,51730,51884,52036,52189,52340,52491,
        52642,52792,52941,53090,53238,53386,53533,53680,53826,53972,54117,54262,
        54406,54549,54693,54835,54977,55119,55260,55401,55541,55681,55820,55959,
        56098,56236,56373,56510,56647,56783,56919,57054,57189,57324,57458,57592,
        57725,57858,57990,58122,58254,58385,58516,58647,58777,58907,59036,59165,
        59294,59422,59550,59678,59805,59932,60058,60184,60310,60435,60561,60685,
        60810,60934,61058,61181,61304,61427,61549,61671,61793,61915,62036,62157,
        62277,62398,62518,62637,62757,62876,62994,63113,63231,63349,63466,63584,
        63701,63817,63934,64050,64166,64281,64397,64512,64626,64741,64855,64969,
        65083,65196,65309,65422,65535,
    };
    for (unsigned i=0;i<pixels;++i,band+=3) {
        int r=(int)(band[0]>>repeated)-(int)black[0];
        int g=(int)(band[1]>>(1+repeated))-(int)black[1];
        int b=(int)(band[2]>>repeated)-(int)black[2];
        r=r>0 ? (r*(int)gain[0])>>10 : 0;
        g=g>0 ? (g*(int)gain[1])>>10 : 0;
        b=b>0 ? (b*(int)gain[2])>>10 : 0;
#if defined(__arm__) && defined(__ARM_FEATURE_DSP)
        /* Four times the corrected 14-bit white point is at most 65532. */
        unsigned rg=((unsigned)r|((unsigned)g<<16))^0x80008000u;
        band[0]=merge_srgb_sample(srgb,merge_matrix_channel(rg,b,5220,-354,-770));
        band[1]=merge_srgb_sample(srgb,merge_matrix_channel(rg,b,-444,5823,-1283));
        band[2]=merge_srgb_sample(srgb,merge_matrix_channel(rg,b,22,-1445,5519));
#else
        static const short matrix[3][3]={
            {5220,-354,-770},{-444,5823,-1283},{22,-1445,5519}
        };
        for (unsigned c=0;c<3;++c) {
            int linear=(matrix[c][0]*r+matrix[c][1]*g+matrix[c][2]*b)>>10;
            unsigned value=linear>0 ? (unsigned)linear : 0;
            band[c]=value>=65536 ? 65535 : merge_srgb_sample(srgb,value);
        }
#endif
    }
}
