/* SPDX-License-Identifier: MIT */

enum {
    MOV_FILE_CALLBACKS=0xb1f2d598u,
    MOV_OPEN_RETURN=0x10536c36u,
    MOV_SINK_HANDLE=0x5a720001u,
    MOV_SINK_WAITING=0, MOV_SINK_ACTIVE, MOV_SINK_CLOSED
};

static const u32 mov_original[]={
    0x105adbb7u, 0x105adb6du, 0x105adb23u,
    0x105adad7u, 0x105ada8du, 0x105ada69u
};
static u32 mov_sink_state, mov_discarded_bytes;

#ifdef Z72_RAW_PCM_MOV_CONTENTION
static u32 mov_open_handle,mov_fs_token;

/* C 1.70: mirror the mux adapter's lookup at 10474DF2 under its own mutex.
 * The native table initialization clears 0x410 bytes (ten 0x68-byte rows). */
static u32 mov_file_token(u32 handle)
{
    u32 token=0;
    CALL(0x10360f19u,void,u32)(0xb188ac4cu);
    const u32 count=WORD(0xb188ac54u);
    for (u32 i=0;count<=10u && i<count;++i) {
        const u32 row=0xb2989228u+i*0x68u;
        if (WORD(row+0x60u)!=handle || WORD(row+0x58u)!=1u || WORD(row+0x5cu)) continue;
        const u32 object=i ? row+0x10u : WORD(row+8u);
        if (!(object&3u) && object>=0xb1800000u && object<=0xb5effff4u) {
            const u32 fd=WORD(object+8u);
            if ((fd>>16)<2u) token=fd+1u;
        }
        break;
    }
    CALL(0x10360f13u,void,u32)(0xb188ac4cu);
    return token;
}
#endif

static int mov_sink_open(const void *context,u32 mode)
{
    /* The muxer's open wrapper tail-calls this slot. Other open callers pass through. */
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (caller==MOV_OPEN_RETURN && mode==1u && waiting_for_rec(load(&state)) && !(load(&raw_settings)&1u))
        raw_latch_orientation();
    if (caller==MOV_OPEN_RETURN && mode==1u &&
#ifdef Z72_RAW_DUAL_SOLO
        solo_accept_mov()
#else
        waiting_for_rec(load(&state)) && RECORD_ONLY() && !load(&stop_requested)
#endif
    ) {
        u32 expected=MOV_SINK_WAITING;
        if (__atomic_compare_exchange_n(&mov_sink_state,&expected,MOV_SINK_ACTIVE,0,
                                        __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return MOV_SINK_HANDLE;
    }
    const int handle=CALL(mov_original[0],int,const void *,u32)(context,mode);
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    if (caller==MOV_OPEN_RETURN && mode==1u && !(load(&raw_settings)&0x11u)) {
        publish(&mov_fs_token,handle<0 ? 0u : mov_file_token((u32)handle));
        publish(&mov_open_handle,(u32)handle+1u);
    }
#endif
    return handle;
}

static int mov_sink_close(int handle)
{
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    if (load(&mov_open_handle)==(u32)handle+1u) {
        publish(&mov_fs_token,0);
        publish(&mov_open_handle,0);
    }
#endif
    if (handle!=MOV_SINK_HANDLE) return CALL(mov_original[1],int,int)(handle);
    publish(&mov_sink_state,MOV_SINK_CLOSED);
    return 0;
}

static int mov_sink_write(int handle,const void *data,u32 bytes,u32 *actual)
{
    if (handle!=MOV_SINK_HANDLE)
        return CALL(mov_original[3],int,int,const void *,u32,u32 *)(handle,data,bytes,actual);
    if (load(&mov_sink_state)!=MOV_SINK_ACTIVE) return -1;
    if (actual) *actual=bytes;
    __atomic_fetch_add(&mov_discarded_bytes,bytes,__ATOMIC_RELAXED);
    return 0;
}

/* AAPCS passes the signed 64-bit offset in r2:r3 and the origin on the stack.
 * The muxer callback reports status only. Discarded data has no file position to update. */
static int mov_sink_seek(int handle,long long offset,u32 origin)
{
    if (handle!=MOV_SINK_HANDLE)
        return CALL(mov_original[4],int,int,long long,u32)(handle,offset,origin);
    return load(&mov_sink_state)==MOV_SINK_ACTIVE ? 0 : -1;
}

static int mov_sink_flush(int handle)
{
    if (handle!=MOV_SINK_HANDLE) return CALL(mov_original[5],int,int)(handle);
    return load(&mov_sink_state)==MOV_SINK_ACTIVE ? handle : -1;
}

static int mov_sink_install(void)
{
    u32 *const slots=(u32 *)MOV_FILE_CALLBACKS;
    const u32 replacement[]={
        (u32)mov_sink_open, (u32)mov_sink_close, mov_original[2],
        (u32)mov_sink_write, (u32)mov_sink_seek, (u32)mov_sink_flush
    };
    const u32 count=sizeof(replacement)/sizeof(*replacement);
    const u32 *expected=load(slots)==replacement[0] ? replacement : mov_original;
    for (u32 i=0;i<count;++i)
        if (load(slots+i)!=expected[i]) return 0;
    if (expected==mov_original) {
        /* Publish open last, after a synthetic handle can be served by every writer callback. */
        for (u32 i=count;i>0;--i)
            if (replacement[i-1]!=mov_original[i-1]) publish(slots+i-1,replacement[i-1]);
    }
    return 1;
}
