/* SPDX-License-Identifier: MIT */
static void read_nvme_admin_snapshot(struct nvme_admin_snapshot *sample)
{
    sample->status=NVME_ADMIN_UNAVAILABLE;
    sample->command_mask=0;
    sample->movie_ms=WORD(MOVIE_PROGRESS_MS);
    sample->phases=BYTE(MOVIE_PHASE)|(BYTE(RECORDER_PHASE)<<8);
    if (WORD(MEDIA_TABLE_SLOT)!=MEDIA_TABLE_AB || WORD(FS_VOLUMES+0x1cu)!=0x104825b3u ||
        WORD(0xb1801710u)!=0xb1801738u || WORD(0xb1801744u)!=0x104b7a43u) return;

    const u32 block=WORD(0xb18012ecu),controller=nvme_block_controller(block);
    if (!controller || WORD(controller+0x10u)!=PCI_PROTOCOL || WORD(controller+0x34u)!=NVME_SUBMIT) return;
    const u32 sq=WORD(controller+0x58u),cq=WORD(controller+0x64u);
    if (sq<0xb1800000u || sq>0xb7ffe000u || (sq&4095u) || cq!=sq+4096u ||
        !z72_raw_identity_span(sq,128u) || !z72_raw_identity_span(cq,32u)) return;

    /* The original synchronous Admin queue rotates through two entries. */
    sample->status=NVME_ADMIN_BUSY;
    const u32 next=HALF(controller+0xa6u),tail=HALF(controller+0x88u),phase=BYTE(controller+0xa2u);
    if (tail>1u || phase>1u || HALF(controller+0x94u)!=tail) return;
    u32 mask=0;
    for (u32 i=0;i<2u;++i) {
        const u32 address=sq+64u*(tail^i),first=WORD(address);
        const u32 cid=(unsigned short)(next-2u+i);
        if ((first>>16)!=cid) continue;
        struct nvme_admin_command *command=&sample->command[i];
        command->cdw0=first;
        command->nsid=WORD(address+4u);
        command->cdw10=WORD(address+40u);
        command->cdw11=WORD(address+44u);
        command->cdw12=WORD(address+48u);
        command->completion=~0u;
        const u32 entry=cq+16u*(tail^i),status=WORD(entry+12u);
        const u32 completed_phase=phase^(i==0u || tail==0u);
        if ((status&0xffffu)==cid && ((status>>16)&1u)==completed_phase &&
            !(WORD(entry+8u)>>16) && WORD(entry+12u)==status)
            command->completion=status;
        if (WORD(address)==first) mask|=1u<<i;
    }
    if (WORD(0xb18012ecu)!=block || WORD(controller+0x58u)!=sq || WORD(controller+0x64u)!=cq ||
        HALF(controller+0x88u)!=tail || HALF(controller+0x94u)!=tail || HALF(controller+0xa6u)!=next ||
        BYTE(controller+0xa2u)!=phase) return;
    sample->next_cid=next;
    sample->command_mask=mask;
    sample->status=NVME_ADMIN_SAMPLED;
}

static void snapshot_nvme_admin(u32 point)
{
    read_nvme_admin_snapshot(&log.nvme_admin[point]);
}
