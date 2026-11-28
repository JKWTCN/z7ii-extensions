/* SPDX-License-Identifier: MIT */
enum { MXIC_REC_WORD=0x20012618u, MXIC_REC_VIDEO=0x01000002u };
#ifdef Z72_RAW_HD_MXIC_TABLE
enum { MXIC_REC_HD_PEER=0x00500002u };
#endif
static int mxic_record_disabled;

static struct mxic_record_snapshot mxic_record_snapshot(void)
{
    return (struct mxic_record_snapshot){WORD(MXIC_REC_WORD),WORD(0x20012490u),
                                         WORD(0x200124a0u),WORD(0x20015010u)};
}

/* Stock video tables share the target word. The native code programs the
 * three witnesses before the word when it changes tables. */
static int mxic_record_video(const struct mxic_record_snapshot *s)
{
#ifdef Z72_RAW_HD_MXIC_TABLE
    /* C 1.70 selects table 1 for both HD record (0x0A) and view (0x1D). */
    if (s->a==MXIC_REC_HD_PEER && s->b==MXIC_REC_HD_PEER)
        return s->group==0xfcfcfc00u || s->group==0xfcfcfcf0u;
#endif
    return s->a==s->b && (s->a==0x00a00002u || s->a==0x008c0002u) &&
           (s->group==0xf8f8fc00u || s->group==0xf8f8fcf0u);
}

static int mxic_record_current_video(const struct mxic_record_snapshot *s)
{
#ifdef Z72_RAW_HD_MXIC_TABLE
#ifdef Z72_RAW_HD_MXIC_PAIR
    const int hd=z72_mode==&z72_hd;
#else
    const int hd=hd_live_capture();
#endif
    if ((s->a==MXIC_REC_HD_PEER)!=hd) return 0;
#endif
    return mxic_record_video(s);
}

static int mxic_record_menu(const struct mxic_record_snapshot *s)
{
    return !s->a && !s->b && (s->group==0xfef8fc00u || s->group==0xfef8fcf0u);
}

static void mxic_record_set(u32 value)
{
    WORD(MXIC_REC_WORD)=value;
    __asm__ volatile("dsb sy" ::: "memory");
}

#ifdef Z72_RAW_PCM_MOV_CONTENTION
#define MXIC_REPORT (*metrics)
#define MXIC_BEGIN begin
#define MXIC_END end
static void mxic_record_end(struct mxic_record_metrics *metrics,u32 bytes,u32 elapsed)
{
#else
#define MXIC_REPORT log.mxic_record
#define MXIC_BEGIN volume_begin
#define MXIC_END volume_end
static void mxic_record_end(u32 bytes,u32 elapsed)
{
#endif
    struct mxic_record_snapshot s=mxic_record_snapshot();
    const int held=mxic_record_current_video(&s) && s.control==0;
    int wrote=0;
    for (u32 i=0;i<3u;++i) {
        if (mxic_record_video(&s) && s.control!=MXIC_REC_VIDEO && !(s.control&~MXIC_REC_VIDEO)) {
            mxic_record_set(MXIC_REC_VIDEO);
            wrote=1;
        } else if (wrote && mxic_record_menu(&s) && s.control==MXIC_REC_VIDEO) {
            /* The camera installed table 0 between our snapshot and restore. */
            mxic_record_set(0);
        } else break;
        s=mxic_record_snapshot();
    }
    MXIC_REPORT.last=s;
    if ((mxic_record_video(&s) && s.control!=MXIC_REC_VIDEO) ||
        (wrote && mxic_record_menu(&s) && s.control!=0u)) {
        ++MXIC_REPORT.faults;
        mxic_record_disabled=1;
        publish(&stop_requested,1u);
        return;
    }
    if (!wrote || !mxic_record_video(&s)) {
        ++MXIC_REPORT.handoffs;
        return;
    }
    ++MXIC_REPORT.restored;
    if (held && bytes) {
        ++MXIC_REPORT.confirmed;
        MXIC_REPORT.bytes+=bytes;
#ifdef Z72_RAW_WIDE_RECORD_TOTALS
        if (MXIC_REPORT.bytes<bytes) {
#ifdef Z72_RAW_PCM_SEGMENTS
            if (metrics==&log.mov_io.mxic) ++log.session.mov_mxic_high;
            else
#endif
            ++log.mxic_bytes_high;
        }
#endif
        MXIC_REPORT.ms+=elapsed;
    }
}

#ifdef Z72_RAW_MXIC_DUAL_SD
static int mxic_record_route_enabled(void)
{
    if (RECORD_ONLY()) return 1;
    /* WAIT_PROFILE sampled the native MOV descriptor under its lock.
     * The submit callback already holds the filesystem lock. */
    const u32 settings=load(&raw_settings);
#ifdef Z72_RAW_MXIC_DUAL_CFB
    /* Menu destinations SD/CF/factory are 0/1/2; recorder slots CF/SD are 0/1. */
    const u32 target=settings>>6,slot=log.route.descriptor_slot;
    if (slot>1u || (target!=2u && target!=1u-slot)) return 0;
    return (settings&0x15u)==4u && settings==log.route.settings &&
           log.route.descriptor_valid==1u && WORD(MEDIA_TABLE_SLOT)==MEDIA_TABLE_AB &&
           WORD(0xb2982160u)==1u && BYTE(0xb2982178u)==slot;
#else
    return (settings&0x15u)==4u && settings==log.route.settings && (settings>>6)!=1u &&
           log.route.descriptor_valid==1u && log.route.descriptor_slot==1u &&
           WORD(MEDIA_TABLE_SLOT)==MEDIA_TABLE_AB && WORD(0xb2982160u)==1u && BYTE(0xb2982178u)==1u;
#endif
}
#else
#define mxic_record_route_enabled RECORD_ONLY
#endif

#ifdef Z72_RAW_PCM_MOV_CONTENTION
static int mxic_record_begin(struct mxic_record_metrics *metrics,u32 begin,u32 end,u32 address,u32 bytes)
{
#else
static int mxic_record_begin(u32 address,u32 bytes)
{
#endif
    if (mxic_record_disabled || !bytes || bytes>(1u<<20) || address<MXIC_BEGIN || address>=MXIC_END ||
        bytes>MXIC_END-address || !mxic_record_route_enabled() || clip_path[0]!='A' ||
        (z72_mode!=&z72_uhd
#ifdef Z72_RAW_HD_MXIC_PAIR
         && z72_mode!=&z72_hd
#elif defined(Z72_RAW_HD_LIVE_CAPTURE)
         && !(hd_live_capture() && metrics==&log.mxic_record)
#endif
        ) || BYTE(CAPTURE_TYPE)!=z72_mode->type || nef_capture_stopping()) return 0;
    struct mxic_record_snapshot s=mxic_record_snapshot();
    if (!MXIC_REPORT.checked) MXIC_REPORT.first=s;
    ++MXIC_REPORT.checked;
    if (!mxic_record_current_video(&s) || s.control!=MXIC_REC_VIDEO) return 0;
    ++MXIC_REPORT.windows;
    mxic_record_set(0);
    if (!WORD(MXIC_REC_WORD)) return 1;
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    mxic_record_end(metrics,0,0);
#else
    mxic_record_end(0,0);
#endif
    return 0;
}
#undef MXIC_REPORT
#undef MXIC_BEGIN
#undef MXIC_END
