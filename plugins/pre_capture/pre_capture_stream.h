/* SPDX-License-Identifier: MIT */
/* Observe the real SDK input listener; never consume native count/end events. */
static void stream_event(z72_u32 event,z72_u32 argument)
{
    z72_u32 state=LOAD(stream[0]);
    z72_u32 card=Z72_PRE_CARD_STREAM && fallback[7]==CARD_SHOT;
    z72_u32 owned=fallback[7]==1 || card;
    z72_u32 active=(state==1 || state==2) && owned;
    if (state==3 && owned && event==0x04100074u)
        __atomic_fetch_or(&stream[3],0x80,__ATOMIC_RELAXED);
    if (active && event==0x04100074u) {
        if (!pre_capture_ram(argument,4) || !WORD(argument) || WORD(argument)>0xffffffffu-LOAD(stream[1]))
            __atomic_fetch_or(&stream[3],1,__ATOMIC_RELAXED);
        else __atomic_fetch_add(&stream[1],WORD(argument),__ATOMIC_RELEASE);
        STORE(stream[7],tick());
    }
    /* The original terminal handler resets its accounting. Retain the actual
     * count before forwarding, then require its genuine SDK busy retirement. */
    z72_u32 final=active && event==0x04100075u ? WORD(0xb18693a4u) : 0;
    CALL(STREAM_NATIVE,void,z72_u32,z72_u32)(event,argument);
    if (active && event==0x04100075u && !BYTE(0xb1869364u) && !BYTE(0xb1869365u) && !BYTE(0xb186936bu)) {
        if (LOAD(stream[0])==1 && !card) __atomic_fetch_or(&stream[3],2,__ATOMIC_RELAXED);
        if (final!=LOAD(stream[1])) __atomic_fetch_or(&stream[3],4,__ATOMIC_RELAXED);
        STORE(stream[2],final);
        if (card) STORE(card_expected,final);
        else STORE(batch_expected,final);
        STORE(stream[11],tick()); STORE(stream[0],3);
    }
}

static z72_u32 stream_expired(z72_u32 now,z72_u32 stamp)
{
    /* Another task may publish a tick newer than the worker's entry sample. */
    z72_u32 elapsed=now-stamp;
    return elapsed>=20000u && elapsed<0x80000000u;
}


static z72_u32 stream_drain(z72_u32 now)
{
    z72_u32 state=LOAD(stream[0]);
    if (state==1) {
        z72_u32 menu_state=BYTE(0xb1824f0cu),playback=BYTE(0xb1828174u);
        if (!LOAD(enabled) ||
            ((!LOAD(half) || !LOAD(half_latch)) &&
             !((LOAD(control)&FROZEN) && !LOAD(promote[0])) &&
             !(LOAD(promote[0]) && LOAD(full_latch) && !LOAD(full_released)) &&
             !(LOAD(promote[0]) && !LOAD(promote[3]) && now-LOAD(promote[2])<2000u)) ||
            (LOAD(promote[0]) && LOAD(promote[1])) ||
            LOAD(enable_request)==1 || io_result || copy_failures || LOAD(gate_fault) || LOAD(stream[3]) ||
            (menu_state>=1 && menu_state<=3) || (playback>=1 && playback<=3) ||
            WORD(0xb25519b8u) || !capture_allowed() || stream_expired(now,LOAD(stream[7]))) {
            z72_u32 running=1;
            if (!__atomic_compare_exchange_n(&stream[0],&running,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;
            STORE(stream[8],now);
            z72_u32 mode=0;
            z72_u32 result=CALL(0x103bac41u,z72_u32,const z72_u32 *)(&mode);
            if (result) { __atomic_fetch_or(&stream[3],8,__ATOMIC_RELAXED); STORE(enabled,0); }
        }
        return 0;
    }
    if (state==3 && !(LOAD(stream[3])&0x80u) && !LOAD(peer_active) && !LOAD(gate_active) && !LOAD(ring_busy) &&
        !BYTE(0xb1869364u) && !BYTE(0xb1869365u) && !BYTE(0xb186936bu) &&
        LOAD(batch_created)==LOAD(stream[2]) && LOAD(batch_seen)==LOAD(stream[2])) {
        if (LOAD(stream[3])) {
            STORE(enabled,0); STORE(fallback[3],0);
        }
        return 1;
    }
    if ((state==2 || state==3) && stream_expired(now,state==2 ? LOAD(stream[8]) : LOAD(stream[11]))) {
        STORE(enabled,0); __atomic_fetch_or(&stream[3],0x20,__ATOMIC_RELAXED);
    }
    return 0;
}
