/* SPDX-License-Identifier: MIT */
enum {
    PCI_PROTOCOL=0xb18006f0u, PCI_SYNC_SLOT=PCI_PROTOCOL+0x2cu,
    PCI_SYNC_ORIGINAL=0x10754829u, PCI_MAPPINGS=0xb1ce18c8u,
    CACHE_RANGE_LIMIT=0x45000u, CACHE_CHUNK=0x40000u
};
static u32 writer_stack_anchor,cache_begin,cache_end;

static u32 stack_pointer(void)
{
    u32 sp;
    __asm__ volatile("mov %0, sp":"=r"(sp));
    return sp;
}

static void raw_pci_sync(void)
{
    /* The card worker has an 8 KiB stack and a tail-call entry trampoline.
     * Restrict the experiment to its upper 4 KiB and the sync submit call. */
    u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (caller!=0x10515baau || load(&writer_stack_anchor)-stack_pointer()>=4096u) {
        CALL(PCI_SYNC_ORIGINAL,void,void)();
        return;
    }
    u32 start=tick();
    ++log.cache.sync_calls;
    /* Keep the stock 64-entry walk and clean+invalidate every mapping. */
    for (u32 i=0;i<64u;++i) {
        u32 address=WORD(PCI_MAPPINGS+8u*i);
        if (!address) continue;
        u32 size=WORD(PCI_MAPPINGS+8u*i+4u);
        if (size<=CACHE_RANGE_LIMIT) {
            CALL(0x1034fa5du,void,u32,u32)(address,size);
            continue;
        }
        if (address<cache_begin || address>=cache_end || size>cache_end-address ||
            (address&63u) || (size&63u)) {
            u32 before=tick();
            CALL(0x1034fa5du,void,u32,u32)(address,size);
            ++log.cache.other_large_calls;
            log.cache.other_large_ms+=tick()-before;
            continue;
        }
        if (!CALL(0x103c5ac7u,int,u32)(address)) {
            ++log.cache.uncached_calls;
            CALL(0x1034fa5du,void,u32,u32)(address,size);
            continue;
        }
        u32 before=tick();
        if (!log.cache.control_bytes) {
            /* Time one unmodified large mapping as an on-clip reference. */
            CALL(0x1034fa5du,void,u32,u32)(address,size);
            log.cache.control_bytes=size;
            log.cache.control_ms=tick()-before;
        } else {
            ++log.cache.range_calls;
            log.cache.range_bytes+=size;
#ifdef Z72_RAW_WIDE_RECORD_TOTALS
            if (log.cache.range_bytes<size) ++log.cache_range_bytes_high;
#endif
            while (size) {
                u32 chunk=size<CACHE_CHUNK ? size : CACHE_CHUNK;
                CALL(0x1034fa5du,void,u32,u32)(address,chunk);
                address+=chunk;
                size-=chunk;
            }
            log.cache.range_ms+=tick()-before;
        }
    }
    log.cache.sync_ms+=tick()-start;
}

static int begin_pci_sync(u32 counter,u32 address,u32 bytes)
{
    if (!counter || WORD(counter-0xa8u+0x10u)!=PCI_PROTOCOL ||
        WORD(PCI_PROTOCOL+0x18u)!=0x10754a35u || WORD(PCI_PROTOCOL+0x1cu)!=0x10754a55u)
        return 0;
    cache_begin=address;
    cache_end=address+bytes;
    u32 original=PCI_SYNC_ORIGINAL;
    return __atomic_compare_exchange_n(&WORD(PCI_SYNC_SLOT),&original,(u32)raw_pci_sync,0,
                                       __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}

static void end_pci_sync(void)
{
    u32 ours=(u32)raw_pci_sync;
    __atomic_compare_exchange_n(&WORD(PCI_SYNC_SLOT),&ours,PCI_SYNC_ORIGINAL,0,
                                __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}
