/* SPDX-License-Identifier: MIT */
/* The HD recorder rotates four RAW slots at both high rates. Keep the
 * observed completion time and generation until the slot can be reused. */
static _Alignas(8) unsigned long long hd_tails[RAW_SLOT_COUNT];

static int hd_live_capture(void)
{
    return z72_mode==&z72_hd && (z72_clip_fps_num==50u || z72_clip_fps_num==60000u);
}

static unsigned long long hd_tail(u32 ordinal)
{
    return __atomic_load_n(&hd_tails[ordinal%RAW_SLOT_COUNT],__ATOMIC_ACQUIRE);
}

static u32 hd_capture_token(u32 latest,u32 wanted)
{
    const u32 ordinal=latest>>RAW_TOKEN_SHIFT;
    if (!(latest&RAW_TOKEN_PAIR) || ordinal<=wanted || ordinal-wanted>2u) return latest;
    const u32 token=(u32)(hd_tail(wanted)>>32);
    return (token>>RAW_TOKEN_SHIFT)==wanted && (token&RAW_TOKEN_PAIR) ? token : latest;
}

static u32 hd_copy_status(u32 token,struct z72_raw_status *status)
{
    const u32 ordinal=token>>RAW_TOKEN_SHIFT,slot=(token&RAW_TOKEN_SLOT)-1u;
    const unsigned long long tail=hd_tail(ordinal);
    /* Three frame periods after a tail the capture DMA can enter that slot.
     * Round down with a tick of margin for the two rates. */
    const u32 limit=z72_clip_fps_num==50u ? 59u : 49u;
    for (u32 retry=0;retry<3u;++retry) {
        const u32 observed=load(&tail_token),current=observed>>RAW_TOKEN_SHIFT;
        const u32 index=WORD(PREVIEW+0x74u),address=WORD(PREVIEW+0x78u),cursor=WORD(PREVIEW+0x7cu);
        status->observed_token=observed;
        status->live_index=index;
        status->live_address=address;
        status->live_tail=cursor;
        status->pending_stop=WORD(PENDING_STOP);
        if (!z72_raw_profile_matches() || status->pending_stop) return RAW_PROFILE_CHANGED;
        if ((u32)(tail>>32)!=token || current-ordinal>2u || tick()-(u32)tail>=limit)
            return RAW_WINDOW_EXPIRED;
        if (index>=RAW_SLOT_COUNT || address!=z72_raw_slot_address(index) ||
            cursor<address || cursor>address+clip_pixel_bytes()) return RAW_UNSAFE_SLOT;
        if (index!=WORD(PREVIEW+0x74u) || address!=WORD(PREVIEW+0x78u) || observed!=load(&tail_token)) {
            ++status->snapshot_retries;
            continue;
        }
        u32 expected=token;
        for (u32 i=ordinal;i<current;++i) {
            expected=z72_raw_next_tail(expected,(expected&RAW_TOKEN_SLOT)%RAW_SLOT_COUNT+1u);
            if ((u32)(hd_tail(i+1u)>>32)!=expected) return RAW_WINDOW_EXPIRED;
        }
        if (observed!=expected || observed!=load(&tail_token)) return RAW_WINDOW_EXPIRED;
        const u32 offset=(index+RAW_SLOT_COUNT-slot)%RAW_SLOT_COUNT,advance=current-ordinal;
        if (offset<advance || offset>advance+1u ||
            (!offset && cursor!=address+z72_hd.tail_offset)) return RAW_UNSAFE_SLOT;
        return RAW_COPY_OK;
    }
    return RAW_SNAPSHOT_UNSTABLE;
}
