/* SPDX-License-Identifier: MIT */
/* Select the arming source, without consuming AF-ON or changing AF settings.
 * SW1 ownership records real shutter edges separately from virtual AF arming. */
static volatile z72_u32 jpeg_sw1_owned,jpeg_sw1_down;
static z72_u32 jpeg_sw2_ignored;
static void jpeg_start_poll(void)
{
    z72_u32 af=CALL(0x1022cfa7u,z72_u32,z72_u32)(0x5a)==1;
    z72_u32 sources=(LOAD(jpeg_sw1_down) ? 1u : 0u)|(af ? 2u : 0u);
    z72_u32 selected=sources&(jpeg_start_mode==0 ? 1u : jpeg_start_mode==1 ? 2u : 3u);
    /* Finish actual swallowed SW2/SW1 up edges before rearming a held AF-ON.
     * An AF-only cycle must not invent a physical shutter-half release. */
    if (LOAD(cycle_done)==2 && !LOAD(full_latch) && !LOAD(jpeg_sw1_down)) selected=0;
    if (!LOAD(enabled)) selected=LOAD(jpeg_sw1_down);
    /* A new down stays recorded as physical input while the canceled
     * generation releases its own edges. Arm it on the next idle poll. */
    if (LOAD(control)&CANCELLED_GENERATION) selected=0;
    STORE(half,!!selected); STORE(half_latch,!!selected);
    if (selected && LOAD(enabled) && !LOAD(control) && !LOAD(in_flight) &&
        !LOAD(cycle_done) && !LOAD(release_pending) && photo_view()) {
        z72_u32 empty=0,next=__atomic_add_fetch(&generation_sequence,8,__ATOMIC_RELAXED);
        discard_review();
        __atomic_compare_exchange_n(&control,&empty,next,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
}
static z72_u32 jpeg_shutter_event(z72_u32 fn,z72_u32 i)
{
    if (fn==0x1022aa83u) {
        if (!LOAD(enabled) && !LOAD(in_flight)) return 0;
        if (CALL(0x1022cfa7u,z72_u32,z72_u32)(0x58)==1) {
            if (photo_view() || LOAD(control) || LOAD(in_flight)) {
                STORE(jpeg_sw1_owned,1); STORE(jpeg_sw1_down,1);
                ++half_consumed; jpeg_start_poll(); return 1;
            }
        } else if (LOAD(half_latch) || LOAD(in_flight)) return 1;
        return 0;
    }
    if (fn==0x1022a9edu) {
        /* AF can produce a logical half-up while physical SW1 stays down. */
        if (CALL(0x1022cfa7u,z72_u32,z72_u32)(0x58)==1)
            return LOAD(jpeg_sw1_owned) || LOAD(half_latch);
        STORE(jpeg_sw1_down,0);
        z72_u32 owned=__atomic_exchange_n(&jpeg_sw1_owned,0,__ATOMIC_ACQ_REL);
        if (owned) __atomic_fetch_or(&release_pending,1u<<i,__ATOMIC_RELEASE);
        jpeg_start_poll(); return owned;
    }
    if (fn==0x1022aa25u && jpeg_sw2_ignored) {
        jpeg_sw2_ignored=0; return 1;
    }
    if (fn==0x1022aa5du && (LOAD(control)&CANCELLED_GENERATION)) {
        jpeg_sw2_ignored=1; return 1;
    }
    if (fn==0x1022aa5du && LOAD(enabled) &&
        (photo_view() || (LOAD(half_latch) && LOAD(half) && !WORD(0xb25519b8u)))) {
        z72_u32 old=LOAD(control);
        if (!(old&~7u)) discard_review();
        while (!(old&FROZEN)) {
            if (old&CANCELLED_GENERATION) { jpeg_sw2_ignored=1; return 1; }
            STORE(trigger_tick,tick()); STORE(full_released,0);
            if (__atomic_compare_exchange_n(&control,&old,old|FROZEN|((old&~7u) ? 0 :
                __atomic_add_fetch(&generation_sequence,8,__ATOMIC_RELAXED)),
                0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) break;
        }
        if (!(old&FROZEN)) STORE(triggered,1);
        STORE(full_latch,1); return 1;
    }
    if (fn==0x1022aa5du && LOAD(in_flight) && !LOAD(enabled)) {
        STORE(full_latch,1); return 1;
    }
    if (fn==0x1022aa25u && LOAD(full_latch)) {
        __atomic_fetch_or(&release_pending,1u<<i,__ATOMIC_RELEASE);
        STORE(full_released,1); STORE(full_latch,0); return 1;
    }
    return 0;
}
