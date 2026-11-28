/* SPDX-License-Identifier: MIT */
#define PROBE_ARM_LABEL "RAW 4K profile: arm"
#ifdef Z72_RAW_50P_SINGLE_FRAME
#define PROBE_MAGIC 0x46514b34u /* 4KQF */
#define PROBE_VERSION 51u
#elif defined(Z72_RAW_50P_PHASE_SAMPLES)
#define PROBE_MAGIC 0x45514b34u /* 4KQE */
#define PROBE_VERSION 50u
#elif defined(Z72_RAW_COMPRESSION_MENU)
#define PROBE_MAGIC 0x50434b34u /* 4KCP */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_DROP_EXPIRED)
#define PROBE_MAGIC 0x50444b34u /* 4KDP */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_MIXED_FRAMES)
#define PROBE_MAGIC 0x584d4b34u /* 4KMX */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PREFETCH_OUTPUT_WAIT)
#define PROBE_MAGIC 0x57424b34u /* 4KBW */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PREFETCH_PARALLEL)
#define PROBE_MAGIC 0x4b424b34u /* 4KBK */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PREFETCH_NEF)
#define PROBE_MAGIC 0x43324b34u /* 4K2C */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_STAGED_NEF_LIVE)
#define PROBE_MAGIC 0x42324b34u /* 4K2B */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_STAGED_NEF_TEST)
#define PROBE_MAGIC 0x5a414b34u /* 4KAZ */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PACKED_RASTER_BOUNDARY)
#define PROBE_MAGIC 0x59414b34u /* 4KAY */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_TERMINAL_ROW)
#define PROBE_MAGIC 0x57414b34u /* 4KAW */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_BANK_HANDOFF)
#define PROBE_MAGIC 0x56414b34u /* 4KAV */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PCM_2997_HANDOFF)
#define PROBE_MAGIC 0x55414b34u /* 4KAU */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PCM_AUTO_RATE)
#define PROBE_MAGIC 0x52414b34u /* 4KAR */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_REC_SPLIT_SNAPSHOT)
#define PROBE_MAGIC 0x54414b34u /* 4KAT */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PCM_ORDINARY_FILES)
#define PROBE_MAGIC 0x39414b34u /* 4KA9 */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PCM_SEGMENTS)
#define PROBE_MAGIC 0x53414b34u /* 4KAS */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_WIDE_RECORD_TOTALS)
#define PROBE_MAGIC 0x44514b34u /* 4KQD */
#define PROBE_VERSION 49u
#elif defined(Z72_RAW_PCM_MOV_CONTENTION)
#define PROBE_MAGIC 0x4d414b34u /* 4KAM */
#define PROBE_VERSION 48u
#elif defined(Z72_RAW_PCM_BATCH_WRITE)
#define PROBE_MAGIC 0x42414b34u /* 4KAB: PCM pool and two-frame batch counters */
#define PROBE_VERSION 48u
#elif defined(Z72_RAW_PCM_POOL)
#define PROBE_MAGIC 0x43414b34u /* 4KAC: layout 48 plus PCM pool bounds */
#define PROBE_VERSION 48u
#elif defined(Z72_RAW_MXIC_RECORD_TEST)
#define PROBE_MAGIC 0x43514b34u /* 4KQC */
#define PROBE_VERSION 48u
#elif defined(Z72_RAW_MXIC_MENU_TEST)
#define PROBE_MAGIC 0x42514b34u /* 4KQB */
#define PROBE_VERSION 47u
#elif defined(Z72_RAW_NVME_LIVE_DMA_TEST)
#define PROBE_MAGIC 0x41514b34u /* 4KQA */
#define PROBE_VERSION 46u
#elif defined(Z72_RAW_NVME_DISPLAY_TEST)
#define PROBE_MAGIC 0x39514b34u /* 4KQ9 */
#define PROBE_VERSION 45u
#elif defined(Z72_RAW_NVME_LIVE_RESOLUTION_TEST)
#define PROBE_MAGIC 0x38514b34u /* 4KQ8 */
#define PROBE_VERSION 44u
#elif defined(Z72_RAW_NVME_LIVE_MENU_TEST)
#define PROBE_MAGIC 0x37514b34u /* 4KQ7 */
#define PROBE_VERSION 43u
#elif defined(Z72_RAW_NVME_REC_MODE_TEST)
#define PROBE_MAGIC 0x36514b34u /* 4KQ6 */
#define PROBE_VERSION 42u
#elif defined(Z72_RAW_NVME_REC_WAKE_TEST)
#define PROBE_MAGIC 0x35514b34u /* 4KQ5 */
#define PROBE_VERSION 41u
#elif defined(Z72_RAW_NVME_REC_STOP_TEST)
#define PROBE_MAGIC 0x34514b34u /* 4KQ4 */
#define PROBE_VERSION 40u
#elif defined(Z72_RAW_NVME_REC_BUFFER_TEST)
#define PROBE_MAGIC 0x33514b34u /* 4KQ3 */
#define PROBE_VERSION 39u
#elif defined(Z72_RAW_NVME_REC_DMA_NATIVE_REGISTERS)
#define PROBE_MAGIC 0x32514b34u /* 4KQ2 */
#define PROBE_VERSION 38u
#elif defined(Z72_RAW_NVME_REC_DMA_PROGRESS)
#define PROBE_MAGIC 0x31514b34u /* 4KQ1 */
#define PROBE_VERSION 37u
#elif defined(Z72_RAW_NVME_REC_DMA_IRQ_TIME)
#define PROBE_MAGIC 0x30514b34u /* 4KQ0 */
#define PROBE_VERSION 36u
#elif defined(Z72_RAW_NVME_REC_DMA)
#define PROBE_MAGIC 0x5a504b34u /* 4KPZ */
#define PROBE_VERSION 35u
#elif defined(Z72_RAW_NVME_REC_MPS)
#define PROBE_MAGIC 0x59504b34u /* 4KPY */
#define PROBE_VERSION 34u
#elif defined(Z72_RAW_NVME_REC_MRRS)
#define PROBE_MAGIC 0x58504b34u /* 4KPX */
#define PROBE_VERSION 33u
#elif defined(Z72_RAW_NVME_KEEP_AWAKE)
#define PROBE_MAGIC 0x57504b34u /* 4KPW */
#define PROBE_VERSION 32u
#elif defined(Z72_RAW_NVME_REC_TEST)
#define PROBE_MAGIC 0x56504b34u /* 4KPV */
#define PROBE_VERSION 31u
#elif defined(Z72_RAW_NVME_POWER_TEST)
#define PROBE_MAGIC 0x55504b34u /* 4KPU */
#define PROBE_VERSION 30u
#elif defined(Z72_RAW_NVME_ADMIN_SNAPSHOT)
#define PROBE_MAGIC 0x54504b34u /* 4KPT */
#define PROBE_VERSION 29u
#elif defined(Z72_RAW_NEF_ADAPTIVE_SLOTS)
#define PROBE_MAGIC 0x53504b34u /* 4KPS */
#define PROBE_VERSION 28u
#elif defined(Z72_RAW_NEF_QUIET_STOP)
#define PROBE_MAGIC 0x52504b34u /* 4KPR */
#define PROBE_VERSION 27u
#elif defined(Z72_RAW_NVME_HEALTH_TEST)
#define PROBE_MAGIC 0x51504b34u /* 4KPQ */
#define PROBE_VERSION 26u
#elif defined(Z72_RAW_NVME_PRE_PAYLOAD_TEST)
#define PROBE_MAGIC 0x50504b34u /* 4KPP */
#define PROBE_VERSION 25u
#elif defined(Z72_RAW_NVME_STANDBY_TEST)
#define PROBE_MAGIC 0x4f504b34u /* 4KPO */
#define PROBE_VERSION 24u
#elif defined(Z72_RAW_NEF_DIRECT_OVERSIZE)
#define PROBE_MAGIC 0x4e504b34u /* 4KPN */
#define PROBE_VERSION 23u
#elif defined(Z72_RAW_NVME_MRRS_DWORD)
#define PROBE_MAGIC 0x4d504b34u /* 4KPM */
#define PROBE_VERSION 22u
#elif defined(Z72_RAW_NVME_MRRS_TEST)
#define PROBE_MAGIC 0x4c504b34u /* 4KPL */
#define PROBE_VERSION 21u
#elif defined(Z72_RAW_NVME_WRITE_SWEEP)
#define PROBE_MAGIC 0x4b504b34u /* 4KPK */
#define PROBE_VERSION 20u
#elif defined(Z72_RAW_NVME_WRITE_QD4)
#define PROBE_MAGIC 0x4a504b34u /* 4KPJ */
#define PROBE_VERSION 19u
#elif defined(Z72_RAW_NVME_WRITE_TEST)
#define PROBE_MAGIC 0x49504b34u /* 4KPI */
#define PROBE_VERSION 18u
#elif defined(Z72_RAW_NVME_PRIVATE_PRP)
#define PROBE_MAGIC 0x48504b34u /* 4KPH */
#define PROBE_VERSION 17u
#elif defined(Z72_RAW_NVME_ASYNC_TEST)
#define PROBE_MAGIC 0x47504b34u /* 4KPG */
#define PROBE_VERSION 16u
#elif defined(Z72_RAW_NVME_CQ_SAMPLE)
#define PROBE_MAGIC 0x46504b34u /* 4KPF */
#define PROBE_VERSION 15u
#elif defined(Z72_RAW_EXFAT_PREFETCH)
#define PROBE_MAGIC 0x45504b34u /* 4KPE */
#define PROBE_VERSION 14u
#elif defined(Z72_RAW_NEF_ROLLOVER)
#define PROBE_MAGIC 0x44504b34u /* 4KPD */
#define PROBE_VERSION 13u
#elif defined(Z72_RAW_NEF_COMPLETION_SNAPSHOT)
#define PROBE_MAGIC 0x43504b34u /* 4KPC */
#define PROBE_VERSION 12u
#elif defined(Z72_RAW_NEF_LATE_COMPLETION)
#define PROBE_MAGIC 0x42504b34u /* 4KPB */
#define PROBE_VERSION 11u
#elif defined(Z72_RAW_NVME_WAIT_TRACE)
#define PROBE_MAGIC 0x41504b34u /* 4KPA */
#define PROBE_VERSION 10u
#elif defined(Z72_RAW_NVME_TIMING)
#define PROBE_MAGIC 0x39504b34u /* 4KP9 */
#define PROBE_VERSION 9u
#elif defined(Z72_RAW_PCI_CACHE_RANGE)
#define PROBE_MAGIC 0x38504b34u /* 4KP8 */
#define PROBE_VERSION 8u
#elif defined(Z72_RAW_RECORDER_TARGET)
#define PROBE_MAGIC 0x37504b34u /* 4KP7 */
#define PROBE_VERSION 7u
#elif defined(Z72_RAW_VOLUME_TIMING)
#define PROBE_MAGIC 0x36504b34u /* 4KP6 */
#define PROBE_VERSION 6u
#elif defined(Z72_RAW_ALIGNED_WRITE)
#define PROBE_MAGIC 0x35504b34u /* 4KP5 */
#define PROBE_VERSION 5u
#elif defined(Z72_RAW_RASTER_COPY)
#define PROBE_MAGIC 0x34504b34u /* 4KP4 */
#define PROBE_VERSION 4u
#elif defined(Z72_RAW_SPLIT_DMA)
#define PROBE_MAGIC 0x33504b34u /* 4KP3 */
#define PROBE_VERSION 3u
#elif defined(Z72_RAW_FRAME_POOLS)
#define PROBE_MAGIC 0x32504b34u /* 4KP2 */
#define PROBE_VERSION 2u
#else
#define PROBE_MAGIC 0x31504b34u /* 4KP1 */
#define PROBE_VERSION 1u
#endif

struct channel_sample {
    u32 stride, select, banks[8], status[4], config, config_stride, config_banks[8];
};

struct mode_sample {
    u32 progress_ms, type, encoder, format_selector, fps_num, fps_den;
    u32 irq, second_irq, mask, pending_stop, geometry[8], control;
    struct channel_sample channels[2];
    u32 final_index[2];
};

#ifdef Z72_RAW_NVME_CQ_SAMPLE
enum { NVME_CQ_SAMPLES=8 };
struct nvme_cq_sample {
    u32 bytes,cid,observed,probe_ms,wait_ms,completion;
};
#endif

#ifdef Z72_RAW_NVME_STANDBY_TEST
struct nvme_native_write { u32 bytes, write_ms, close_ms, io_commands; };
#endif
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
enum { NVME_ADMIN_ARMED, NVME_ADMIN_STARTED, NVME_ADMIN_PRE_FRAME, NVME_ADMIN_FIRST_WRITE,
       NVME_ADMIN_CLOSED, NVME_ADMIN_STANDBY, NVME_ADMIN_POINTS };
enum { NVME_ADMIN_UNAVAILABLE=1, NVME_ADMIN_BUSY, NVME_ADMIN_SAMPLED };
struct nvme_admin_command { u32 cdw0, nsid, cdw10, cdw11, cdw12, completion; };
struct nvme_admin_snapshot {
    u32 status, movie_ms, phases, next_cid, command_mask;
    struct nvme_admin_command command[2];
};
#endif

static _Alignas(64) struct {
    u32 magic, version, target, bytes, tails, missed, sampled;
    struct mode_sample frames[48];
#ifdef Z72_RAW_FRAME_POOLS
    u32 requested,free_bytes[2],largest_block[2],allocated_slots[2];
#endif
#ifdef Z72_RAW_SPLIT_DMA
    u32 single_transfers,split_transfers,last_channels,late_channels,completion_ms;
#endif
#ifdef Z72_RAW_RASTER_COPY
    u32 raster_requests,raster_rows,raster_result,raster_observed_ms;
#endif
#ifdef Z72_RAW_ALIGNED_WRITE
    u32 bulk_bytes,bulk_ms,edge_ms;
#endif
#ifdef Z72_RAW_VOLUME_TIMING
    struct {
        u32 requests,handler,sector_bytes,lock_ms,locked_ms;
        u32 data_calls,data_bytes,data_ms,max_sectors,other_calls,other_ms;
        u32 reader,read_calls,read_ms,device_write;
    } volume;
#endif
#ifdef Z72_RAW_RECORDER_TARGET
    struct {
        u32 settings,factory,descriptor_valid,descriptor_slot;
        u32 adapter,observed_calls,io_commands,io_alignment,command_bytes;
    } route;
#endif
#ifdef Z72_RAW_PCI_CACHE_RANGE
    struct {
        u32 sync_calls,sync_ms,uncached_calls,control_bytes,control_ms,range_calls,range_bytes,range_ms;
        u32 scoped_calls,other_large_calls,other_large_ms;
    } cache;
#endif
#ifdef Z72_RAW_NVME_TIMING
    struct {
        u32 scoped_calls, writes, bytes, write_ms, max_write_ms, largest_write;
        u32 flushes, flush_ms, max_flush_ms, failures;
        u32 bytes_high, bulk_bytes_high, payload_bytes_high;
    } nvme;
    struct {
        u32 ordinal, elapsed_ms, bulk_ms, reads, payload_calls, other_writes;
        u32 writes, write_ms, flushes, flush_ms;
    } slow_write;
#endif
#ifdef Z72_RAW_NVME_WAIT_TRACE
    struct {
        u32 method, writes, write_ms, max_write_ms, wait_failures;
    } nvme_wait;
#endif
#ifdef Z72_RAW_NEF_EVENT_WAIT
    struct {
        u32 signaled, polling, last_error_stage, last_error_ordinal, last_error_result;
    } nef;
#endif
#ifdef Z72_RAW_NEF_LATE_COMPLETION
    struct {
        u32 last_ms, max_ms, accepted_late;
    } nef_deadline;
#endif
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
    struct {
        u32 elapsed_ms, code, token, slot, validation, ordinal, backfilled;
    } nef_terminal;
#endif
#ifdef Z72_RAW_NEF_ROLLOVER
    struct {
        u32 index,previous_end,open_progress_ms;
    } nef_segment;
#endif
#ifdef Z72_RAW_EXFAT_PREFETCH
    struct {
        u32 candidates,hits,prefetch_calls,prefetch_sectors,retries,invalidations;
        u32 slow_candidates,slow_hits,slow_prefetch_calls;
    } bitmap;
#endif
#ifdef Z72_RAW_NVME_CQ_SAMPLE
    struct {
        u32 count,unavailable,base,head;
        struct nvme_cq_sample samples[NVME_CQ_SAMPLES];
    } cq;
#endif
#ifdef Z72_RAW_NVME_ASYNC_TEST
    struct {
        u32 stage,bytes,queue,submitted,completed,matched,first_ready,submit_ms,elapsed_ms;
        u32 cid[2],completion[2],result[2];
    } async_read;
#endif
#ifdef Z72_RAW_NVME_PRIVATE_PRP
    struct {
        u32 page[2],expected_hash[2],read_hash[2];
    } async_prp;
#endif
#ifdef Z72_RAW_NVME_WRITE_TEST
#ifdef Z72_RAW_NVME_WRITE_QD4
    struct {
        u32 stage,seed_mask,key,baseline_ms,parallel_ms,first_ready,accepted,completed;
        u32 cid[4],completion[4],page[4];
        u32 last_error,mismatches;
        u32 reaped_ms[4],irq_waits,irq_wakes,flush_ms;
    } async_write;
#else
    struct {
        u32 stage,seed_mask,key,baseline_ms,parallel_ms,first_ready,accepted,completed;
        u32 cid[2],completion[2],page[2];
        u32 last_error,mismatches;
    } async_write;
#endif
#endif
#ifdef Z72_RAW_NVME_WRITE_SWEEP
    struct {
        u32 pci_id,link_status,link_cap,device_control,device_cap,command_limit;
        u32 read_mask,write_mask;
        struct { u32 write_ms,flush_ms,read_ms,write_submit_ms,read_submit_ms; } size[3];
    } nvme_sweep;
#endif
#ifdef Z72_RAW_NVME_MRRS_TEST
    struct {
        u32 stage,control_offset,before,applied,restored,write_ms,submit_ms,flush_ms;
        u32 completed,mismatches,last_error;
    } nvme_mrrs;
#endif
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    struct { u32 largest_frame, direct_frames; } nef_output;
#endif
#ifdef Z72_RAW_NVME_STANDBY_TEST
    struct { u32 standby; struct nvme_native_write io; } nvme_native_file;
#endif
#ifdef Z72_RAW_NVME_PRE_PAYLOAD_TEST
    struct { u32 stage, movie_ms, error; struct nvme_native_write io; } nvme_pre_payload;
#endif
#ifdef Z72_RAW_NVME_HEALTH_TEST
    struct {
        u32 stage, version, error, completion, temperature, warning, warning_limit, critical_limit;
        u32 warning_minutes, critical_minutes, thermal_transitions[2];
    } nvme_health;
#endif
#ifdef Z72_RAW_NEF_ADAPTIVE_SLOTS
    struct { u32 payload_limit, combined_request, scratch_largest_block; } nef_buffers;
#endif
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
    struct nvme_admin_snapshot nvme_admin[NVME_ADMIN_POINTS];
#endif
#ifdef Z72_RAW_NVME_POWER_TEST
    struct {
        u32 stage, pci_id, operational, standby, standby_exit_us, skipped, first_movie_ms, last_movie_ms;
    } nvme_power;
#endif
#ifdef Z72_RAW_NVME_REC_TEST
    struct {
        u32 stage, oncs, factory_slot, descriptor_slot;
        struct nvme_rec_sample {
            u32 stage, movie_ms, read_ms, compare_ms, write_ms, flush_ms, error, compare_completion;
        } sample[2];
    } nvme_rec;
#endif
#ifdef Z72_RAW_NVME_KEEP_AWAKE
    struct {
        u32 stage, reads, max_gap_ms, inactive_observations, state_armed, state_before_io, state_after_io, error;
    } nvme_keep_awake;
#endif
#ifdef Z72_RAW_NVME_REC_MRRS
    struct {
        u32 stage, device_cap, original, applied, restored, ignored_mask, extended_end;
        struct nvme_rec_sample extended_request, restored_request;
    } nvme_rec_mrrs;
#endif
#ifdef Z72_RAW_NVME_REC_MPS
    struct {
        u32 stage;
        struct nvme_mps_port {
            u32 capability, original, applied, extended_end, restored, ignored_mask;
        } port[2];
        struct nvme_rec_sample extended_payload, restored_payload;
    } nvme_rec_mps;
#endif
#ifdef Z72_RAW_NVME_REC_DMA
#ifdef Z72_RAW_NVME_LIVE_DMA_TEST
#define REC_DMA_SAMPLES 4
#else
#define REC_DMA_SAMPLES 2
#endif
    struct {
        u32 buffer, bytes_per_transfer;
        struct nvme_rec_dma_sample {
            u32 stage, movie_ms, transfers, transfer_ms, channel_mask, mismatches;
        } sample[REC_DMA_SAMPLES];
    } nvme_rec_dma;
#endif
#ifdef Z72_RAW_NVME_REC_DMA_IRQ_TIME
    struct nvme_rec_dma_irq_sample {
        u32 count, completion_us, resume_us, max_resume_us;
    } nvme_dma_irq[REC_DMA_SAMPLES];
#endif
#ifdef Z72_RAW_NVME_REC_DMA_PROGRESS
    struct nvme_rec_dma_progress {
        u32 count_register, destination_register, source_half_samples, destination_half_samples;
        u32 source_half_us, destination_half_us, max_sample_gap_us;
    } nvme_dma_progress[REC_DMA_SAMPLES];
#endif
#ifdef Z72_RAW_NVME_REC_BUFFER_TEST
    struct {
        u32 heap_address, general_address;
        struct nvme_rec_sample general_standby, general_recording, heap_recording_repeat;
    } nvme_rec_buffers;
#endif
#ifdef Z72_RAW_NVME_REC_STOP_TEST
    struct {
        u32 stage, active_capture, active_stride, idle_ms, preview_ms;
        struct nvme_stop_sample {
            u32 elapsed_ms, before, after, stride_before, stride_after;
            struct nvme_rec_sample io;
        } sample[3];
    } nvme_rec_stop;
#endif
#ifdef Z72_RAW_NVME_REC_WAKE_TEST
    struct nvme_rec_wake_sample {
        u32 stage, movie_ms, idle_wait_ms, wake_ms, state_before, state_asleep, state_after;
        struct nvme_admin_snapshot admin[3];
        struct nvme_rec_sample io;
    } nvme_rec_wake[2];
#endif
#ifdef Z72_RAW_NVME_REC_MODE_TEST
    struct {
        u32 stage, active_capture, active_stride;
        struct nvme_rec_mode_sample {
            u32 elapsed_ms, before, after, stride_before, stride_after;
            struct nvme_rec_sample io;
        } sample[3];
    } nvme_rec_mode;
#endif
#ifdef Z72_RAW_NVME_LIVE_MENU_TEST
    u32 baseline_ui_before,baseline_ui_after;
#endif
#ifdef Z72_RAW_MXIC_MENU_TEST
    struct {
        struct nvme_rec_sample changed,restored;
        struct mxic_write_trial {
            u32 stage,completed;
            struct mxic_snapshot { u32 control,peer_a,peer_b,group,state; } point[4];
        } trial[4];
    } mxic;
#endif
#ifdef Z72_RAW_NVME_LIVE_DMA_TEST
    struct { u32 root,card; } view_link[REC_DMA_SAMPLES][2];
#endif
#ifdef Z72_RAW_NVME_LIVE_RESOLUTION_TEST
    struct {
        u32 flags,pool_word;
    } capture_setup[4][2];
#endif
#ifdef Z72_RAW_NVME_DISPLAY_TEST
    struct { u32 route, tails, ms; } display_sample[3][2];
    u32 last_route,last_capture,last_stride;
#endif
#ifdef Z72_RAW_MXIC_RECORD_TEST
    struct mxic_record_metrics {
        u32 checked,windows,confirmed,restored,handoffs,faults,bytes,ms;
        struct mxic_record_snapshot { u32 control,a,b,group; } first,last;
    } mxic_record;
#endif
#ifdef Z72_RAW_WIDE_RECORD_TOTALS
    u32 mxic_bytes_high,cache_range_bytes_high;
#endif
#ifdef Z72_RAW_50P_PHASE_SAMPLES
    struct phase_sample { u32 start_ms,end_ms,stable,pixels[4][3][4]; } phase_samples[48];
#endif
#ifdef Z72_RAW_50P_SINGLE_FRAME
    struct { u32 result,sample,elapsed_ms; } single_frame;
#endif
#ifdef Z72_RAW_PCM_POOL
    struct { u32 base,total,block,slots,source,bytes; } pcm_pool;
#endif
#ifdef Z72_RAW_PCM_BATCH_WRITE
    struct { u32 locks,reuses,max_hold_ms; } write_batch;
#endif
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    struct {
        u32 workers,file_seen,volume_calls,writes,bytes,ms,overlap_ms,native_overlap_ms,max_overlap_ms;
        u32 max_wait_ms,mov_ms_in_max_wait;
        struct mxic_record_metrics mxic;
    } mov_io;
#endif
#ifdef Z72_RAW_PCM_SEGMENTS
    struct { u32 sealed,produced,written,reason,last_ordinal,failure,pcm_partial,mov_high,mov_mxic_high; } session;
#endif
#ifdef Z72_RAW_PCM_ORDINARY_FILES
    struct { u32 opened,closed,operation,result; } segment_files;
#endif
#ifdef Z72_RAW_REC_SPLIT_SNAPSHOT
    struct { u32 selection,cap_low,cap_high,submitted_ms,callback_ms; } recording;
#endif
#ifdef Z72_RAW_PCM_AUTO_RATE
    struct { u32 type,num,den,width,height,stride,format,progress; } source;
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
    struct { u32 late_submits,late_completions,prepare_cursor,jitter_blocks,pcm_errors,
                 previous_slot,slot,elapsed,previous_bytes; } handoff;
#endif
#ifdef Z72_RAW_BANK_HANDOFF
    struct { u32 attempts,saved; } bank_handoff;
#endif
#ifdef Z72_RAW_TERMINAL_ROW
    struct { u32 deferred,final_result,first_anchored,audio_ms,submitted; } terminal_row;
#endif
#if defined(Z72_RAW_PACKED_RASTER_BOUNDARY) && !defined(Z72_RAW_COMPRESSION_MENU)
    struct { u32 accepted,raw_result,cursor; } raster_boundary;
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
    struct {
        u32 source,reference,anchor_token,copy_done,reuse_token,reuse_tick;
        u32 changed_offset,reference_word,reused_word,encoded_source,reference_bytes;
    } staged_nef;
#endif
#ifdef Z72_RAW_STAGED_NEF_LIVE
    struct {
        u32 addresses[2], staged, max_copy_ms, overlap, peak_input;
        u32 last_source_result, pool_busy, max_output_wait_ms, max_encode_ms;
    } staged_stream;
#endif
#ifdef Z72_RAW_PREFETCH_NEF
    struct {
        u32 address,attempts,ready,used,missed,last_ordinal,last_result,max_copy_ms,max_wait_ms;
    } prefetch;
#ifdef Z72_RAW_PREFETCH_PARALLEL
    struct { u32 racing,live_saved,retries,recovered,last_ordinal,last_reason; } backup;
#ifdef Z72_RAW_MIXED_FRAMES
    struct { u32 nef_frames,packed_frames,packed_written,overlaps,max_overlap_ms; } mixed;
#ifdef Z72_RAW_DROP_EXPIRED
    struct { u32 frames,last_ordinal,last_reason; } dropped;
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
    struct { u32 accepted,raw_result,cursor; } raster_boundary;
    struct { u32 choice,mixed_active; } compression;
#endif
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    struct { u32 offers,started,ready,preferred,last_ordinal; } output_wait;
#endif
#endif
#endif
} log;
#ifdef Z72_RAW_50P_SINGLE_FRAME
static u32 frame_sample_ready;
#endif

#ifdef Z72_RAW_SELECT_OUTPUT
static void reset_profile_log(void)
{
    CALL(0x10361035u,void,void *,u32)(&log,sizeof(log));
    log.magic=PROBE_MAGIC;
    log.version=PROBE_VERSION;
    log.target=Z72_TARGET;
    log.bytes=sizeof(log);
}
#endif

static void read_channel(struct channel_sample *ch,u32 hw,u32 sw)
{
    ch->stride=WORD(hw+8u);
    ch->select=WORD(hw+0x3cu);
    ch->config=HALF(sw);
    ch->config_stride=HALF(sw+0x1au);
    for (u32 i=0;i<8u;++i) {
        ch->banks[i]=WORD(hw+0x50u+4u*i);
        ch->config_banks[i]=WORD(sw+0x24u+4u*i);
    }
    for (u32 i=0;i<4u;++i) ch->status[i]=WORD(hw+0x70u+4u*i);
}

#ifdef Z72_RAW_50P_PHASE_SAMPLES
static u32 tick(void);

static void sample_50p_phase(const struct mode_sample *s,struct phase_sample *p)
{
    const struct channel_sample *ch=&s->channels[1];
    const u32 index=ch->status[1],phase=ch->status[0];
    if (s->type!=0x12u || s->fps_num!=50000u || s->fps_den!=1000u ||
        s->format_selector!=3u || s->pending_stop || s->control!=2u || s->geometry[5]!=2u ||
        s->geometry[7]!=(2176u<<16) ||
#ifdef Z72_RAW_50P_SINGLE_FRAME
        ch->select>1u || ch->config!=(2u|(ch->select<<8)) ||
#else
        ch->config!=2u || ch->select ||
#endif
        ch->stride!=5792u || ch->config_stride!=5792u || index>=4u || index!=s->final_index[1]) return;
    if (!((phase==3u && s->geometry[6]==(3856u<<16)) || (!phase && !s->geometry[6]))) return;
    const u32 reported=z72_raw_slot_address((phase==3u ? index+3u : index)%4u);
    if (ch->status[2]!=reported || ch->status[3]!=reported+0xc04f00u) return;
    for (u32 i=0;i<8u;++i) {
        const u32 base=z72_raw_slot_address(i%4u);
        if (ch->banks[i]!=base || ch->config_banks[i]!=base) return;
        if (i<4u && !z72_raw_identity_span(base,5792u*2176u)) return;
    }
    p->start_ms=tick();
    static const u32 rows[]={0,1088,2175};
    for (u32 i=0;i<4u;++i) {
        for (u32 j=0;j<3u;++j) {
            const u32 line=(z72_raw_slot_address(i)+rows[j]*5792u+2880u)&~63u;
            CALL(0x1034fa29u,u32,u32,u32)(line,64u);
            for (u32 k=0;k<4u;++k) p->pixels[i][j][k]=WORD(line+4u*k);
        }
    }
    p->end_ms=tick();
    for (u32 i=0;i<4u;++i)
        if (WORD(PREVIEW+0x70u+4u*i)!=ch->status[i]) return;
    p->stable=BYTE(CAPTURE_TYPE)==s->type && !WORD(PENDING_STOP) &&
              WORD(GEOMETRY+0x18u)==s->geometry[6] && WORD(PREVIEW+8u)==ch->stride;
#ifdef Z72_RAW_50P_SINGLE_FRAME
    if (WORD(PREVIEW+0x3cu)!=ch->select || HALF(PREVIEW_CONFIG)!=ch->config) p->stable=0;
    if (p->stable && !phase) publish(&frame_sample_ready,log.sampled+1u);
#endif
}
#endif

static void record_tail(u32 first,u32 second,u32 mask)
{
    ++log.tails;
    if (log.sampled>=sizeof(log.frames)/sizeof(log.frames[0]) || BYTE(0xb1811788u)!=1u) return;
    const u32 progress=WORD(0xb1f58f44u), encoder=BYTE(0xb181176cu);
#ifdef Z72_RAW_EARLY_DUAL_START
    if (!raw_start_ready(progress) || (encoder!=1u && encoder!=2u)) return;
#else
    if (progress<2000u || (encoder!=1u && encoder!=2u)) return;
#endif
    struct mode_sample *s=&log.frames[log.sampled];
    s->progress_ms=progress;
    s->type=BYTE(CAPTURE_TYPE);
    s->encoder=encoder;
    s->format_selector=BYTE(0xb1f598c8u+0x6cu);
    s->fps_num=WORD(0xb1f59958u+4u);
    s->fps_den=WORD(0xb1f59958u+8u);
    s->irq=first;
    s->second_irq=second;
    s->mask=mask;
    s->pending_stop=WORD(PENDING_STOP);
    for (u32 i=0;i<8u;++i) s->geometry[i]=WORD(GEOMETRY+4u*i);
    s->control=WORD(GEOMETRY+0x2cu);
    read_channel(&s->channels[0],PRIMARY,0xb26b48fcu);
    read_channel(&s->channels[1],PREVIEW,PREVIEW_CONFIG);
    s->final_index[0]=WORD(PRIMARY+0x74u);
    s->final_index[1]=WORD(PREVIEW+0x74u);
#ifdef Z72_RAW_50P_PHASE_SAMPLES
    sample_50p_phase(s,&log.phase_samples[log.sampled]);
#endif
    ++log.sampled;
}
