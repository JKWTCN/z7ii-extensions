/* SPDX-License-Identifier: MIT */
enum { MOV_FS_DISPATCH=0x1044b301u, MOV_CF_WRITE=0x104825b3u };
static const u32 mov_tasks[]={0xb1d1b1bcu,0xb1d1b094u};
static struct mov_job { u32 stack,fd,buffer,bytes; } mov_jobs[2];
static u32 mov_wait_start,mov_wait_overlap,*mov_volume_slot,mov_cmd_stack,mov_cmd_begin,mov_cmd_end,mov_cmd_fd;

/* The stock dispatcher consumes the message, including its completion callback.
 * Only inspect the 16-byte file request before calling it. Each task is serial. */
static void mov_file_dispatch(u32 task,u32 message)
{
    const u32 i=task==mov_tasks[0] ? 0u : 1u;
    struct mov_job *job=&mov_jobs[i];
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (caller!=0x10294318u || task!=mov_tasks[i] || WORD(message+0x10u)!=0x42091001u) {
        CALL(MOV_FS_DISPATCH,void,u32,u32)(task,message);
        return;
    }
    const u32 req=WORD(message+0x14u);
    publish(&job->fd,WORD(req));
    publish(&job->buffer,WORD(req+4u));
    publish(&job->bytes,WORD(req+12u));
    publish(&job->stack,stack_pointer());
    CALL(MOV_FS_DISPATCH,void,u32,u32)(task,message);
    publish(&job->stack,0);
}

static int mov_nvme_submit(void *protocol,u32 ns,void *packet,void *event)
{
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (event || caller!=0x105139feu || mov_cmd_stack-stack_pointer()>=4096u ||
        load(&mov_fs_token)!=mov_cmd_fd)
        return CALL(NVME_SUBMIT,int,void *,u32,void *,void *)(protocol,ns,packet,event);
    const u32 address=WORD((u32)packet+8u),bytes=WORD((u32)packet+12u);
    if (!bytes || bytes>(1u<<20) || address<mov_cmd_begin || address>=mov_cmd_end ||
        bytes>mov_cmd_end-address)
        return CALL(NVME_SUBMIT,int,void *,u32,void *,void *)(protocol,ns,packet,event);

    /* The first overlapping native command is always measured at stock speed. */
    const int window=log.mov_io.native_overlap_ms && load(&mov_wait_start) && !RECORD_ONLY() &&
        log.route.descriptor_slot==0u &&
        mxic_record_begin(&log.mov_io.mxic,mov_cmd_begin,mov_cmd_end,address,bytes);
    const u32 start=tick();
    const int result=CALL(NVME_SUBMIT,int,void *,u32,void *,void *)(protocol,ns,packet,event);
    const u32 waiting=load(&mov_wait_start),end=tick(),elapsed=end-start;
    if (window) mxic_record_end(&log.mov_io.mxic,result ? 0u : bytes,elapsed);
    ++log.mov_io.writes;
    log.mov_io.bytes+=bytes;
#ifdef Z72_RAW_PCM_SEGMENTS
    if (log.mov_io.bytes<bytes) ++log.session.mov_high;
#endif
    log.mov_io.ms+=elapsed;
    if (waiting) {
        /* FS_LOCK is still held. A ZRV waiter cannot finish acquiring it here. */
        const u32 wait=end-waiting,overlap=wait<elapsed ? wait : elapsed;
        mov_wait_overlap+=overlap;
        log.mov_io.overlap_ms+=overlap;
        if (!window && !result) log.mov_io.native_overlap_ms+=overlap;
        if (overlap>log.mov_io.max_overlap_ms) log.mov_io.max_overlap_ms=overlap;
    }
    return result;
}

static int mov_volume_write(void *context,u32 sector,u32 count,const void *buffer)
{
    const u32 sp=stack_pointer(),addr=(u32)buffer,token=load(&mov_fs_token);
    u32 *slot=0;
    for (u32 i=0;token && i<2u;++i) {
        const u32 anchor=load(&mov_jobs[i].stack);
        if (!anchor || anchor-sp>=4096u) continue;
        const u32 fd=load(&mov_jobs[i].fd),base=load(&mov_jobs[i].buffer),length=load(&mov_jobs[i].bytes);
        if (fd!=token-1u || fd==(u32)file || (fd>>16)!=((u32)file>>16) ||
            length>~base || addr<base || addr>=base+length || !count || count>(base+length-addr)/512u) continue;
        ++log.mov_io.volume_calls;
        const u32 ops=WORD(0xb1801710u);
        const u32 controller=ops==0xb1801738u && WORD(ops+12u)==0x104b7a43u ?
            nvme_block_controller(WORD(0xb18012ecu)) : 0;
        if (controller && !RECORD_ONLY() && log.route.descriptor_slot==0u && mxic_record_route_enabled()) {
            mov_cmd_stack=anchor;
            mov_cmd_fd=token;
            mov_cmd_begin=addr;
            mov_cmd_end=addr+count*512u;
            slot=(u32 *)(controller+0x34u);
            u32 expected=NVME_SUBMIT;
            if (!__atomic_compare_exchange_n(slot,&expected,(u32)mov_nvme_submit,0,
                                             __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) slot=0;
        }
        break;
    }
    const int result=CALL(MOV_CF_WRITE,int,void *,u32,u32,const void *)(context,sector,count,buffer);
    if (slot) {
        u32 ours=(u32)mov_nvme_submit;
        __atomic_compare_exchange_n(slot,&ours,NVME_SUBMIT,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    }
    return result;
}

/* The volume slot is exchanged only under FS_LOCK. The ZRV writer still
 * installs its own callbacks for its own file and restores ours afterwards. */
static void mov_contention_install(u32 *slot)
{
    if (mov_volume_slot) return;
    for (u32 i=0;i<2u;++i) {
        u32 expected=MOV_FS_DISPATCH;
        if (__atomic_compare_exchange_n((u32 *)(mov_tasks[i]+0x24u),&expected,(u32)mov_file_dispatch,0,
                                        __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) log.mov_io.workers|=1u<<i;
    }
    if (log.mov_io.workers) {
        mov_volume_slot=slot;
        publish(slot,(u32)mov_volume_write);
    }
}

static void mov_contention_close(void)
{
    if (!mov_volume_slot) return;
    CALL(0x103d5919u,void,u32)(FS_LOCK);
    u32 ours=(u32)mov_volume_write;
    __atomic_compare_exchange_n(mov_volume_slot,&ours,MOV_CF_WRITE,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    mov_volume_slot=0;
    for (u32 i=0;i<2u;++i) {
        ours=(u32)mov_file_dispatch;
        __atomic_compare_exchange_n((u32 *)(mov_tasks[i]+0x24u),&ours,MOV_FS_DISPATCH,0,
                                    __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    }
    CALL(0x103d58fbu,void,u32)(FS_LOCK);
}
