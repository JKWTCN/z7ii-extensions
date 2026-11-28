/* SPDX-License-Identifier: MIT */
enum { NVME_SUBMIT=0x10515705u };

#ifdef Z72_RAW_MXIC_RECORD_TEST
#include "movie_raw_mxic_record.h"
#endif

#ifdef Z72_RAW_NVME_WAIT_TRACE
static u32 nvme_wait_method;
#ifdef Z72_RAW_NVME_CQ_SAMPLE
#include "movie_raw_nvme_cq.h"
#endif
#ifdef Z72_RAW_NVME_ASYNC_TEST
#include "movie_raw_nvme_async.h"
#endif

static int timed_nvme_wait(u32 count,const u32 *events,u32 *selected)
{
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (caller!=0x10515c4eu || count!=1u ||
        load(&writer_stack_anchor)-stack_pointer()>=4096u)
        return CALL(nvme_wait_method,int,u32,const u32 *,u32 *)(count,events,selected);
#ifdef Z72_RAW_NVME_CQ_SAMPLE
    struct nvme_cq_sample *sample=observe_nvme_cq();
#endif
    const u32 start=tick();
    const int result=CALL(nvme_wait_method,int,u32,const u32 *,u32 *)(count,events,selected);
    const u32 elapsed=tick()-start;
    ++log.nvme_wait.writes;
    log.nvme_wait.write_ms+=elapsed;
    if (elapsed>log.nvme_wait.max_write_ms) log.nvme_wait.max_write_ms=elapsed;
    if (result) ++log.nvme_wait.wait_failures;
#ifdef Z72_RAW_NVME_CQ_SAMPLE
    if (sample) sample->wait_ms=elapsed;
#endif
    return result;
}

static u32 *begin_nvme_wait_timing(void)
{
    const u32 table=WORD(0xb1800584u);
    if ((table&3u) || table<0xb1800000u || table>0xb5efffd8u) return 0;
    u32 *slot=(u32 *)(table+0x24u),original=WORD(table+0x24u);
    if (original<0x10020000u || original>=0x12497b78u || (original&3u)==2u ||
        (nvme_wait_method && nvme_wait_method!=original)) return 0;
    nvme_wait_method=original;
    if (!__atomic_compare_exchange_n(slot,&original,(u32)timed_nvme_wait,0,
                                     __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return 0;
    log.nvme_wait.method=nvme_wait_method;
    return slot;
}

static void end_nvme_wait_timing(u32 *slot)
{
    u32 ours=(u32)timed_nvme_wait;
    __atomic_compare_exchange_n(slot,&ours,nvme_wait_method,0,
                                __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}
#endif

static int timed_nvme_submit(void *protocol,u32 namespace,void *packet,void *event)
{
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (event || (caller!=0x105139feu && caller!=0x10513cc8u) ||
        load(&writer_stack_anchor)-stack_pointer()>=4096u)
        return CALL(NVME_SUBMIT,int,void *,u32,void *,void *)(protocol,namespace,packet,event);
    const u32 bytes=WORD((u32)packet+0x0cu);
#ifdef Z72_RAW_NVME_WAIT_TRACE
    u32 *wait_slot=caller==0x105139feu ? begin_nvme_wait_timing() : 0;
#ifdef Z72_RAW_NVME_CQ_SAMPLE
    nvme_cq_controller=wait_slot ? (u32)protocol-0x30u : 0u;
    nvme_cq_bytes=bytes;
#endif
#endif
#ifdef Z72_RAW_MXIC_RECORD_TEST
    int window=caller==0x105139feu && mxic_record_begin(
#ifdef Z72_RAW_PCM_MOV_CONTENTION
        &log.mxic_record,volume_begin,volume_end,
#endif
        WORD((u32)packet+8u),bytes);
#endif
    u32 start=tick();
    int status=CALL(NVME_SUBMIT,int,void *,u32,void *,void *)(protocol,namespace,packet,event);
    const u32 elapsed=tick()-start;
#ifdef Z72_RAW_MXIC_RECORD_TEST
    if (window) mxic_record_end(
#ifdef Z72_RAW_PCM_MOV_CONTENTION
        &log.mxic_record,
#endif
        status ? 0u : bytes,elapsed);
#endif
#ifdef Z72_RAW_NVME_WAIT_TRACE
    if (wait_slot) end_nvme_wait_timing(wait_slot);
#ifdef Z72_RAW_NVME_CQ_SAMPLE
    nvme_cq_controller=0;
#endif
#endif
    if (status) ++log.nvme.failures;
    if (caller==0x105139feu) {
#ifdef Z72_RAW_NVME_ASYNC_TEST
        if (!status) save_async_reference((u32)protocol,namespace,(u32)packet);
#endif
        ++log.nvme.writes;
        log.nvme.bytes+=bytes;
        if (log.nvme.bytes<bytes) ++log.nvme.bytes_high;
        log.nvme.write_ms+=elapsed;
        if (elapsed>log.nvme.max_write_ms) log.nvme.max_write_ms=elapsed;
        if (bytes>log.nvme.largest_write) log.nvme.largest_write=bytes;
    } else {
        ++log.nvme.flushes;
        log.nvme.flush_ms+=elapsed;
        if (elapsed>log.nvme.max_flush_ms) log.nvme.max_flush_ms=elapsed;
    }
    return status;
}

static u32 *begin_nvme_timing(u32 counter)
{
    if (!counter) return 0;
    u32 *slot=(u32 *)(counter-0xa8u+0x34u),original=NVME_SUBMIT;
    if (!__atomic_compare_exchange_n(slot,&original,(u32)timed_nvme_submit,0,
                                     __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return 0;
    return slot;
}

static void end_nvme_timing(u32 *slot)
{
    u32 ours=(u32)timed_nvme_submit;
    __atomic_compare_exchange_n(slot,&ours,NVME_SUBMIT,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    ++log.nvme.scoped_calls;
}
