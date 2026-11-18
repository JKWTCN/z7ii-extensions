/* SPDX-License-Identifier: MIT */
/* C 1.70 SDK entry relocation metadata; no vendor code bytes in source.
 * Runtime copies 103ba6ea..103bac40 into module-owned RAM. All calls keep
 * their native target/state except local AF admission.
 * Internal branches, three TBB tables and eight literal loads stay in place. */
#define AF_SDK_START 0x103ba6eau
#define AF_SDK_BYTES 0x556u
static const z72_u32 af_sdk_calls[][2]={
    {0x00eu,0x103c09e9u},
    {0x022u,0x101004cfu},
    {0x032u,0x103f350bu},
    {0x046u,0x10361117u},
    {0x050u,0x103610d3u},
    {0x0a2u,0x1032a1bdu},
    {0x0aau,0x1022827du},
    {0x0b6u,0x1034ffbfu},
    {0x0bcu,0x103bbb59u},
    {0x0c4u,0x103bbba9u},
    {0x0d0u,0x1022aebdu},
    {0x0d6u,0x1022bbd5u},
    {0x0dcu,0x1022c8c7u},
    {0x0e6u,0x103bbaf5u},
    {0x0f6u,0x1034ffbfu},
    {0x104u,0x1034ffbfu},
    {0x10eu,0x1034ffbfu},
    {0x136u,0x1034ffbfu},
    {0x150u,0x1034ffbfu},
    {0x15eu,0x1034ffbfu},
    {0x172u,0x1034ffbfu},
    {0x180u,0x1034ffbfu},
    {0x194u,0x1034ffbfu},
    {0x1b4u,0x10229cddu},
    {0x1c0u,0x10229cddu},
    {0x1ccu,0x10229cddu},
    {0x1d8u,0x10229cddu},
    {0x1e2u,0x1022b949u},
    {0x1f2u,0x1034ffbfu},
    {0x1fcu,0x10229fddu},
    {0x208u,0x103bbb37u},
    {0x214u,0x1022a229u},
    {0x21au,0x103bbb37u},
    {0x224u,0x103b9e8du},
    {0x23eu,0x10508901u},
    {0x246u,0x1034ffbfu},
    {0x264u,0x1034ffbfu},
    {0x29eu,0x1034ffbfu},
    {0x2a8u,0x1034ffbfu},
    {0x2cau,0x1034ffbfu},
    {0x2dau,0x1034ffbfu},
    {0x2eeu,0x10360f44u},
    {0x2feu,0x103bbb37u},
    {0x30eu,0x103b92a5u},
    {0x314u,0x103b9ed1u},
    {0x328u,0x103bbafbu},
    {0x32cu,0x10506e65u},
    {0x33eu,0x103b92a5u},
    {0x350u,0x103ba2a9u},
    {0x372u,0x1034ffbfu},
    {0x382u,0x103ba0e3u},
    {0x38au,0x103bbafbu},
    {0x38eu,0x10506e65u},
    {0x3a0u,0x103b92a5u},
    {0x3aeu,0x1034ffbfu},
    {0x3beu,0x1034ffbfu},
    {0x3c8u,0x103bbd7bu},
    {0x3d4u,0x103bbb37u},
    {0x3dau,0x103bbd1fu},
    {0x406u,0x1034ffbfu},
    {0x43au,0x1034ffbfu},
    {0x450u,0x103ba341u},
    {0x45cu,0x103ba3ddu},
    {0x464u,0x103bbb3du},
    {0x468u,0x103bbafbu},
    {0x46cu,0x10506e65u},
    {0x47eu,0x103b92a5u},
    {0x49cu,0x103b9fd3u},
    {0x4a4u,0x103bbb37u},
    {0x4bcu,0x103ba165u},
    {0x4c6u,0x103b93d1u},
    {0x4d2u,0x103bbb3du},
    {0x4d6u,0x103bbafbu},
    {0x4dau,0x10506e65u},
    {0x4ecu,0x103b92a5u},
    {0x502u,0x103ba3ddu},
    {0x50eu,0x103ba341u},
    {0x512u,0x103bbafbu},
    {0x516u,0x10506e65u},
    {0x528u,0x103b92a5u},
    {0x53eu,0x103bbb37u},
    {0x54au,0x1022827du},
};

static _Alignas(64) unsigned char af_sdk_code[2048];
/* Init result, SDK calls, raw AF mask, allowed module admissions,
 * allowed SDK predicates, local-control busy exclusions, restoration exclusions. */
static z72_u32 af_stats[7];
static z72_u32 local_af_active(void)
{
    /* Only the locally held AF source and a module-owned shutter/destination.
     * Other active AF masks, foreign capture, menus/playback/movie remain native.
     * Do not write the AF mask, AF activation setting or provider state. */
    if (!(LOAD(control)&~7u) || (!LOAD(half_latch) && !LOAD(in_flight) &&
        !destination_phase && !LOAD(release_pending)) || (view_blockers()&0x1fu)) return 0;
    if (CALL(0x1022bbd5u,z72_u32,void)()!=1 ||
        !CALL(0x1022cfa7u,z72_u32,z72_u32)(0x5a)) return 0;
    return CALL(0x1022bbd5u,z72_u32,void)()==1;
}

static z72_u32 af_sdk_input_mask(void)
{
    z72_u32 mask=CALL(0x1022bbd5u,z72_u32,void)();
    af_stats[2]=mask;
    if (mask==1 && local_af_active()) { ++af_stats[4]; return 0; }
    return mask;
}

static z72_u32 local_control_busy(void)
{
    z72_u32 busy=WORD(0xb18693a8u) || WORD(0xb18693acu);
    if (busy && local_af_active()) { ++af_stats[5]; return 0; }
    return busy;
}

static z72_u32 af_sdk_prepare(void)
{
    /* Fail before publishing hooks if even one native byte differs. */
    z72_u32 hash=2166136261u;
    for (z72_u32 i=0;i<AF_SDK_BYTES;++i) hash=(hash^BYTE(AF_SDK_START+i))*16777619u;
    if (hash!=0x084406f6u) { af_stats[0]=1; return 1; }
    /* Source is 2 mod4. Preserve PC alignment for literal loads and TBBs. */
    unsigned char *entry=af_sdk_code+2;
    for (z72_u32 i=0;i<AF_SDK_BYTES;++i) entry[i]=BYTE(AF_SDK_START+i);
    z72_u32 count=sizeof(af_sdk_calls)/sizeof(af_sdk_calls[0]);
    for (z72_u32 i=0;i<count;++i) {
        z72_u32 site=(z72_u32)entry+af_sdk_calls[i][0];
        z72_u32 stub=(z72_u32)af_sdk_code+0x558u+8*i;
        z72_u32 target=af_sdk_calls[i][1];
        if (af_sdk_calls[i][0]==0xd6u) target=(z72_u32)af_sdk_input_mask;
        WORD(stub)=0xf000f8dfu; /* LDR.W pc,[pc,#0]; interworks for native BLX. */
        WORD(stub+4)=target;
        z72_u32 delta=stub-site-4,s=delta>>24&1u;
        z72_u32 j1=1u^((delta>>23&1u)^s),j2=1u^((delta>>22&1u)^s);
        *(volatile unsigned short *)site=(unsigned short)(0xf000u|(s<<10)|(delta>>12&0x3ffu));
        *(volatile unsigned short *)(site+2)=(unsigned short)(0xd000u|(j1<<13)|(j2<<11)|(delta>>1&0x7ffu));
    }
    /* Same PoU clean / inner-shareable I-cache publication as the existing
     * SD host loader, applied only to this module-owned executable buffer. */
    for (z72_u32 p=(z72_u32)af_sdk_code;p<(z72_u32)af_sdk_code+sizeof(af_sdk_code);p+=32)
        __asm__ volatile("mcr p15,0,%0,c7,c11,1" :: "r"(p) : "memory");
    z72_u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15,0,%0,c7,c1,0\n dsb sy\n isb sy" :: "r"(zero) : "memory");
    af_stats[0]=0; return 0;
}

__attribute__((noinline)) static z72_u32 af_capture(const z72_u32 *request,void *callback)
{
    ++af_stats[1];
    return CALL((z72_u32)af_sdk_code+3u,z72_u32,const z72_u32 *,void *)(request,callback);
}
