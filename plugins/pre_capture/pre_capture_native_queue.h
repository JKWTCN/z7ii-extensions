/* SPDX-License-Identifier: MIT */
/* Pre-trigger interception in the original card output tasks. The local
 * task or slave owns the source through return/ACK. Keep only an owned JPEG
 * copy and the native, as yet unopened, file reservation. */
static z72_u32 card_queue_pre_slot(z72_u32 id)
{
    for (z72_u32 i=0;i<CARD_RECORDS && id;++i) {
        if (LOAD(card_jobs[i][3])==id && LOAD(card_jobs[i][4])==1 &&
            !(card_queue_frames[i].control&FROZEN)) return i;
    }
    return CARD_RECORDS;
}

static z72_u32 card_queue_copy(z72_u32 slot,z72_u32 record,z72_u32 mode)
{
    z72_u32 context=WORD(record+8),result=3;
    if ((mode==0 || mode==3) && WORD(record)==LOAD(card_jobs[slot][3]) &&
        BYTE(context+0x27fa)==0 && !BYTE(context+0xe7) &&
        WORD(context+0x2858)==card_queue_frames[slot].exposure) {
        _Alignas(8) const struct capture_job stamp={
            .request=slot+1,
            .observed_tick=card_queue_frames[slot].tick,
            .observed_control=card_queue_frames[slot].control,
            .order=card_queue_frames[slot].exposure
        };
        result=ring_copy_jpeg(record,mode,&stamp);
    }
    if (!card_queue_frames[slot].cached) STORE(card_queue_frames[slot].discard,1);

    __atomic_fetch_add(&card_queue_pre_done,1,__ATOMIC_ACQ_REL);
    return result;
}

static z72_u32 card_queue_local(void)
{
    z72_u32 request=WORD(FILE_STATE+0x2c);
    if (!pre_capture_ram(request,0x4bb8) || WORD(request+4) || WORD(request+0x7c)!=1) {

        return 0;
    }
    z72_u32 slot=card_queue_pre_slot(WORD(request+0xd8));
    if (slot==CARD_RECORDS || LOAD(card_routes[slot])!=1 || card_jobs[slot][0]!=request) {

        return 0;
    }
    z72_u32 owner=WORD(request+0xc);
    if (owner!=card_jobs[slot][1] || !pre_capture_ram(owner,0x4f8)) {

        return 0;
    }
    z72_u32 context=WORD(owner+0x4f4);
    if (context!=card_jobs[slot][2] || !pre_capture_ram(context,0x3400) ||
        WORD(request+0xe0)!=context || !WORD(owner+0x41c) || !WORD(owner+0x420)) {

        return 0;
    }

    z72_u32 format=CALL(0x105094dfu,z72_u32,z72_u32)(WORD(request+0xd8));
    _Alignas(8) z72_u32 record[14];
    for (z72_u32 i=0;i<13;++i) record[i]=WORD(request+0xd8+4*i);
    record[13]=0; /* Stock local card wrapper supplies this source flag. */

    /* 102c109e / 102c0fd0: preserve task enter/leave, lease release, the
     * per-record result, the original notification and the ready signal. */
    CALL(0x103bf225u,void,z72_u32)(0xb23af290u);
    z72_u32 result=card_queue_copy(slot,(z72_u32)record,format);
    BYTE(request+0x134)=(unsigned char)result;
    if (result) WORD(request+0x4bb4)=0;
    CALL(0x1030d9a5u,void,z72_u32,z72_u32)(WORD(owner+0x420),0xd);
    WORD(owner+0x420)=0;
    WORD(request+0x7c)=0;
    CALL(WORD(FILE_STATE+0x14),void,z72_u32,z72_u32)(0x041b0002u,0);
    WORD(request+0x78)|=2;
    /* No auxiliary output for this JPEG-only route. A configuration that
     * supplies sidecars retains stock processing in the surrounding task. */
    if (WORD(owner+0x4e8) || WORD(owner+0x4ec) || WORD(owner+0x4f0)) {

        CALL(0x102c0e69u,void,z72_u32)(request);
    }
    CALL(0x103bf215u,void,z72_u32,z72_u32)(0xb23af290u,200);
    CALL(0x103610cdu,void,z72_u32,z72_u32)(FILE_STATE+0x24,1);
    return 1;
}

static z72_u32 card_queue_peer(z72_u32 event,z72_u32 packet)
{
    if (event!=0x42070001u || !pre_capture_ram(packet,16) || WORD(packet)!=3 ||
        BYTE(packet+8)!=0 || (BYTE(packet+9)!=0 && BYTE(packet+9)!=3) ||
        !WORD(packet+12)) return 0;
    /* Avoid reconstructing post-trigger payloads once all earlier borrowed
     * work has retired. The original peer handler then owns write and ACK. */
    z72_u32 waiting=0;
    for (z72_u32 i=0;i<CARD_RECORDS && !waiting;++i)
        if (LOAD(card_routes[i])==2 && LOAD(card_jobs[i][4])==1 &&
            !(card_queue_frames[i].control&FROZEN)) waiting=1;
    if (!waiting) {

        return 0;
    }
    z72_u32 idle=0;
    if (!__atomic_compare_exchange_n(&peer_active,&idle,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;
    CALL(0x104ee77bu,z72_u32,z72_u32,void *,void *,void *,void *)(
        packet+8,peer_wire,peer_record,peer_context,peer_headers);
    z72_u32 slot=card_queue_pre_slot(peer_record[0]);
    if (slot==CARD_RECORDS || LOAD(card_routes[slot])!=2 ||
        peer_record[2]!=(z72_u32)peer_context || peer_record[3]!=(z72_u32)peer_headers ||
        peer_record[13]!=1 || (peer_record[0]&15u)!=BYTE(packet+9)) {

        STORE(peer_active,0); return 0;
    }
    for (z72_u32 i=0;i<9;++i) peer_extra[i]=WORD((z72_u32)peer_wire+0x120+4*i);
    peer_record[4]=(z72_u32)peer_extra;
    z72_u32 mode=BYTE(packet+9);
    z72_u32 ready=1;
    /* Mark before ACK; no pointer to this frame may be touched after ACK. */
    if (!__atomic_compare_exchange_n(&card_jobs[slot][4],&ready,3,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        STORE(peer_active,0); return 0;
    }
    unsigned char result=(unsigned char)card_queue_copy(slot,(z72_u32)peer_record,mode);
    z72_u32 ack=CALL(0x10388667u,z72_u32,z72_u32,z72_u32,z72_u32,const void *)(5,4,1,&result);

    if (ack==1) {
        STORE(card_jobs[slot][4],2);
        finish_card(result);
    } else STORE(gate_fault,1);
    STORE(peer_active,0);
    return 1;
}

static void card_queue_cancel_expired(void)
{
    for (z72_u32 i=0;i<CARD_RECORDS;++i)
        if (LOAD(card_queue_frames[i].discard)==1 && LOAD(card_jobs[i][4])==2) {
            STORE(card_queue_frames[i].discard,2);

            /* Native rollback for a reserved DCF ID that never opened. */
            CALL(0x105070fdu,void,z72_u32)(LOAD(card_jobs[i][3]));
        }
}
