/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_ZEBRA_H
#define Z72_PHOTO_ZEBRA_H
#include "z72.h"
#include "config.h"

#define Z72_ZEBRA_WORD(a) (*(volatile z72_u32 *)(a))
#define Z72_ZEBRA_BYTE(a) (*(volatile unsigned char *)(a))
#define Z72_ZEBRA_CALL(a, result, ...) ((result (*)(__VA_ARGS__))(a))

/* Page visibility selects the existing photo pattern without changing its
 * saved pattern or threshold, or the EVF/movie preferences. */
static inline z72_u32 zebra_pattern(void)
{
#ifdef Z72_CUSTOM_DISP
    if (disp_information_visible()) return 0;
    if (disp_custom_live() && !(disp_components()&Z72_DISP_ZEBRA)) return 0;
#endif
    return z72_config_current.photo_zebra_pattern;
}

static volatile z72_u32 zebra_gui_calls,zebra_graphics_calls,zebra_probe_count;
static volatile z72_u32 zebra_probe_records[16][24],zebra_probe_last[24],zebra_probe_sequence;
static volatile z72_u32 zebra_packet_calls[2],zebra_packet_invalid[2];
static volatile z72_u32 zebra_packet_counts[4],zebra_packet_menus[4];
static volatile z72_u32 zebra_packet_sequence[4],zebra_packet_records[4][40];
static volatile z72_u32 zebra_packet_applied;
static volatile z72_u32 zebra_last_object,zebra_last_palette;
static volatile z72_u32 zebra_output_objects[2];
static volatile z72_u32 zebra_frame_batches,zebra_frame_images,zebra_frame_changed;
static volatile z72_u32 zebra_frame_sequence,zebra_frame_record[12];
static volatile z72_u32 zebra_frame_snapshot[236];
static volatile z72_u32 zebra_post_sequence,zebra_post_count,zebra_post_record[16];
static volatile z72_u32 zebra_commit_count,zebra_commit_result;
static volatile z72_u32 zebra_pending_sequence,zebra_pending_calls;
static volatile z72_u32 zebra_pending_changed,zebra_cached_changed,zebra_pending_record[8];
static volatile z72_u32 zebra_source_sequence;
static volatile z72_u32 zebra_source_calls,zebra_source_changed,zebra_source_record[38];
static volatile z72_u32 zebra_history_sequence,zebra_history_count,zebra_history[16][4];
static z72_u32 zebra_history_command,zebra_history_before;
static z72_u32 zebra_packet_apply(volatile z72_u32 *data,z72_u32 mode,z72_u32 command,
                                 z72_u32 pattern,z72_u32 threshold);
static z72_u32 zebra_evf_message[7],zebra_evf_payload[26],zebra_evf_key;
static volatile z72_u32 zebra_evf_pending,zebra_evf_generation,zebra_evf_applied_generation;
/* The first acknowledgement enables LCD restoration after EVF use. */
static volatile z72_u32 zebra_evf_completed;

static inline z72_u32 zebra_photo_output(void)
{
    z72_u32 output=Z72_ZEBRA_WORD(0xb181d3a4u)&7u;
    z72_u32 target=Z72_ZEBRA_BYTE(0xb180854cu);
    return (output==1 && target==0) || (output==4 && target==1) ? output : 0;
}

static inline z72_u32 zebra_command_output(z72_u32 command)
{
    z72_u32 family=command&0xffffff00u;
    /* The native EVF command decoder ends at 0x1E; LCD also has 0x1F. */
    return family==0x42060000u ? 1u :
           family==0x42062000u && (command&255u)<=0x1eu ? 4u : 0u;
}

static inline z72_u32 zebra_photo_context(z72_u32 mode)
{
    if (mode) return 2;
    z72_u32 output=zebra_photo_output();
    if (!output) return 3;
    z72_u32 menu=Z72_ZEBRA_BYTE(0xb1824f0cu),playback=Z72_ZEBRA_BYTE(0xb1828174u);
    /* Normal photo GUI scenes differ from the renderer target byte:
     * rear LCD uses scene 0, while the electronic viewfinder uses scene 2. */
    if (Z72_ZEBRA_BYTE(0xb181da91u)!=(output==4 ? 2u : 0u) || Z72_ZEBRA_BYTE(0xb1821b18u) ||
        Z72_ZEBRA_BYTE(0xb202be70u) || Z72_ZEBRA_BYTE(0xb202c016u) ||
        Z72_ZEBRA_WORD(0xb181dabcu) || (menu>=1 && menu<=3) ||
        (playback>=1 && playback<=3)) return 4;
    return 0;
}

static void zebra_evf_ack(z72_u32 command,z72_u32 generation)
{
    (void)command;
    __atomic_store_n(&zebra_evf_applied_generation,generation,__ATOMIC_RELEASE);
    ++zebra_evf_completed;
    __atomic_store_n(&zebra_evf_pending,0,__ATOMIC_RELEASE);
}

/* Only our owned asynchronous packet may be dropped after a mode transition.
 * Ordinary native packets retain their original handler and acknowledgement. */
static z72_u32 zebra_packet_cancel(const void *message)
{
    if (message!=zebra_evf_message) return 0;
    z72_u32 output=zebra_command_output(zebra_evf_message[4]);
    const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
    if (zebra_photo_output()==output && !zebra_photo_context(Z72_ZEBRA_WORD(0xb25519b8u)) &&
        row[37] && row[10]==zebra_pattern() &&
        row[11]==z72_config_current.photo_zebra_threshold) return 0;
    __atomic_store_n(&zebra_evf_pending,0,__ATOMIC_RELEASE);
    return 1;
}

/* An output switch need not produce a native highlight packet. Queue one
 * owned request without the native GUI driver's completion wait. After EVF
 * use, restore LCD parameters on return to the shared renderer. The display
 * task converts each request only while its photo output still matches. */
static void zebra_evf_request(void)
{
#if !Z72_ZEBRA_OBSERVE_ONLY
    z72_u32 mode=Z72_ZEBRA_WORD(0xb25519b8u);
    z72_u32 output=zebra_photo_output();
    z72_u32 pattern=zebra_pattern();
    z72_u32 threshold=z72_config_current.photo_zebra_threshold;
    z72_u32 lcd_request=0;
#ifdef Z72_CUSTOM_DISP
    lcd_request=disp_custom_live();
#endif
    if (!output || (output==1 && !zebra_evf_completed && !lcd_request) ||
        zebra_photo_context(mode) || pattern>2 || threshold>7) {
        zebra_evf_key=0; return;
    }
    z72_u32 key=(output<<8)+(pattern<<4)+threshold;
    if (key!=zebra_evf_key) { zebra_evf_key=key; ++zebra_evf_generation; }
    z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u);
    if ((state&3u) || state<0xb0000000u || state>0xc0000000u-0x26cu ||
        (Z72_ZEBRA_BYTE(0xb1d178dcu)!=3 && Z72_ZEBRA_BYTE(0xb1d178dcu)!=4) ||
        Z72_ZEBRA_BYTE(state+0x268)>4) return;
    z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,output,0);
    if ((object&3u) || object<0xb0000000u || object>0xc0000000u-0x84u ||
        Z72_ZEBRA_BYTE(object+0x80)) return;
    const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
    if (__atomic_load_n(&zebra_evf_applied_generation,__ATOMIC_ACQUIRE)==zebra_evf_generation &&
        row[37] && row[10]==pattern && row[11]==threshold &&
        Z72_ZEBRA_WORD(state+0x28)==(pattern!=0) &&
        (!pattern || Z72_ZEBRA_BYTE(state+0x88)==(output==4 ? 2u : 0u))) return;
    if (__atomic_exchange_n(&zebra_evf_pending,1,__ATOMIC_ACQ_REL)) return;
    for (z72_u32 i=0;i<7;++i) zebra_evf_message[i]=0;
    for (z72_u32 i=0;i<26;++i) zebra_evf_payload[i]=0;
    zebra_evf_message[3]=(z72_u32)zebra_evf_ack;
    zebra_evf_message[4]=output==4 ? 0x42062007u : 0x42060007u;
    zebra_evf_message[5]=(z72_u32)zebra_evf_payload;
    zebra_evf_message[6]=zebra_evf_generation;
    __asm__ volatile("dmb sy" ::: "memory");
    z72_u32 result=Z72_ZEBRA_CALL(0x10446995u,z72_u32,void *)(zebra_evf_message);
    if (result) {
        __atomic_store_n(&zebra_evf_pending,0,__ATOMIC_RELEASE);
    }
#else
    (void)zebra_evf_ack;
#endif
}

static z72_u32 zebra_state_flags(z72_u32 state)
{
    return (Z72_ZEBRA_WORD(state+0x20)&255u) |
           ((Z72_ZEBRA_WORD(state+0x1c)&255u)<<8) |
           ((Z72_ZEBRA_WORD(state+0x28)&255u)<<16) |
           ((Z72_ZEBRA_WORD(state+0x24)&255u)<<24);
}

/* These callbacks precede the native metadata copy into the live source queues.
 * Change the frame template only during the original callback, then restore it. */
static volatile z72_u32 zebra_producer_busy,zebra_producer_calls,zebra_producer_changed;
static volatile z72_u32 zebra_producer_restored,zebra_producer_skipped,zebra_producer_hooks;
static volatile z72_u32 zebra_producer_sequence,zebra_producer_record[16];
static const z72_u32 zebra_producer_native[]={0x1026968bu,0x102697cfu,0x10269a55u,0x10269d37u};

static z72_u32 zebra_producer(z72_u32 kind,z72_u32 context,z72_u32 a,z72_u32 b,z72_u32 c)
{
    z72_u32 original=zebra_producer_native[kind];
    __atomic_fetch_add(&zebra_producer_calls,1,__ATOMIC_RELAXED);
    if (__atomic_exchange_n(&zebra_producer_busy,1,__ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&zebra_producer_skipped,1,__ATOMIC_RELAXED);
        return Z72_ZEBRA_CALL(original,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(context,a,b,c);
    }
    z72_u32 image=0,work=0,before=0xffffffffu,changed=0,reason=1;
    z72_u32 mode=Z72_ZEBRA_WORD(0xb25519b8u),state=Z72_ZEBRA_WORD(0xb1808580u);
    z72_u32 output=zebra_photo_output();
    z72_u32 pattern=zebra_pattern(),object=zebra_output_objects[output==4];
    const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
    if (!zebra_photo_context(mode) &&
        pattern && pattern<=2 && row[37] && row[10]==pattern &&
        row[11]==z72_config_current.photo_zebra_threshold &&
        !(state&3u) && state>=0xb0000000u && state<=0xc0000000u-0x269u &&
        (Z72_ZEBRA_BYTE(0xb1d178dcu)==3 || Z72_ZEBRA_BYTE(0xb1d178dcu)==4) &&
        Z72_ZEBRA_BYTE(state+0x268)<=4 && Z72_ZEBRA_WORD(state+0x28)==1 &&
        Z72_ZEBRA_BYTE(state+0x88)==(output==4 ? 2u : 0u) &&
        !(object&3u) && object>=0xb0000000u && object<=0xc0000000u-0x84u &&
        !Z72_ZEBRA_BYTE(object+0x80)) {
        reason=2;
        if (!(context&3u) && context>=0xb0000000u && context<=0xc0000000u-0x78u &&
            !Z72_ZEBRA_WORD(context+4) && !Z72_ZEBRA_BYTE(context+0x74)) {
            work=Z72_ZEBRA_WORD(context+0xc);
            if (!(work&3u) && work>=0xb0000000u && work<=0xc0000000u-0x148u) {
                z72_u32 candidate=Z72_ZEBRA_WORD(work+0x144);
                if (!(candidate&3u) && candidate>=0xb0000000u && candidate<=0xc0000000u-0x46cu) {
                    image=candidate; before=Z72_ZEBRA_WORD(image+0x468);
                    reason=before>1 ? 3 : 0;
                    if (!before) {
                        Z72_ZEBRA_WORD(image+0x468)=1;
                        __asm__ volatile("dmb sy" ::: "memory");
                        changed=1; ++zebra_producer_changed;
                    }
                }
            }
        }
    }
    z72_u32 result=Z72_ZEBRA_CALL(original,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(context,a,b,c);
    z72_u32 after=image ? Z72_ZEBRA_WORD(image+0x468) : 0xffffffffu;
    if (changed && after==1) {
        Z72_ZEBRA_WORD(image+0x468)=before;
        __asm__ volatile("dmb sy" ::: "memory");
        ++zebra_producer_restored;
    }
    __atomic_fetch_add(&zebra_producer_sequence,1,__ATOMIC_ACQ_REL);
    zebra_producer_record[0]=kind; zebra_producer_record[1]=context;
    zebra_producer_record[2]=work; zebra_producer_record[3]=image;
    zebra_producer_record[4]=before; zebra_producer_record[5]=changed ? 1 : before;
    zebra_producer_record[6]=after;
    zebra_producer_record[7]=image ? Z72_ZEBRA_WORD(image+0x468) : 0xffffffffu;
    zebra_producer_record[8]=result; zebra_producer_record[9]=reason;
    zebra_producer_record[10]=state; zebra_producer_record[11]=mode;
    zebra_producer_record[12]=Z72_ZEBRA_WORD(0xb181d3a4u);
    zebra_producer_record[13]=changed; zebra_producer_record[14]=pattern;
    zebra_producer_record[15]=z72_config_current.photo_zebra_threshold;
    __atomic_fetch_add(&zebra_producer_sequence,1,__ATOMIC_RELEASE);
    __atomic_store_n(&zebra_producer_busy,0,__ATOMIC_RELEASE);
    return result;
}
#define Z72_ZEBRA_PRODUCER(n) \
static z72_u32 zebra_producer_##n(z72_u32 context,z72_u32 a,z72_u32 b,z72_u32 c) \
{ return zebra_producer(n,context,a,b,c); }
Z72_ZEBRA_PRODUCER(0)
Z72_ZEBRA_PRODUCER(1)
Z72_ZEBRA_PRODUCER(2)
Z72_ZEBRA_PRODUCER(3)
#undef Z72_ZEBRA_PRODUCER

static void zebra_producer_install(void)
{
#if !Z72_ZEBRA_OBSERVE_ONLY
    static const z72_u32 slots[][2]={
        {0xb2bcf484u,0},{0xb2bd0208u,0},{0xb2bd0f8cu,0},{0xb2bd1e98u,0},
        {0xb2bd27bcu,0},{0xb2bd49c4u,0},{0xb2bd3304u,1},{0xb2bd3e58u,1},
        {0xb2bcfeccu,2},{0xb2bc870cu,3},{0xb2bcf490u,3},{0xb2bd0214u,3},
        {0xb2bd0f98u,3},{0xb2bd1ea4u,3},{0xb2bd27c8u,3},{0xb2bd3310u,3},
        {0xb2bd3e64u,3},{0xb2bd49d0u,3}
    };
    const z72_u32 handlers[]={(z72_u32)zebra_producer_0,(z72_u32)zebra_producer_1,
                             (z72_u32)zebra_producer_2,(z72_u32)zebra_producer_3};
    for (z72_u32 i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        if (Z72_ZEBRA_WORD(slots[i][0])!=zebra_producer_native[slots[i][1]]) continue;
        Z72_ZEBRA_WORD(slots[i][0])=handlers[slots[i][1]];
        ++zebra_producer_hooks;
    }
    __asm__ volatile("dmb sy" ::: "memory");
#else
    (void)zebra_producer_0; (void)zebra_producer_1;
    (void)zebra_producer_2; (void)zebra_producer_3;
#endif
}

/* Inspect bounded cells for the selected display before native refresh.
 * LCD uses source queue 0; EVF uses queue 3. Queue 1 belongs to HDMI. */
static void zebra_source_update(z72_u32 event)
{
#if !Z72_ZEBRA_OBSERVE_ONLY
    ++zebra_source_calls;
    if (zebra_photo_context(Z72_ZEBRA_WORD(0xb25519b8u))) return;
    z72_u32 pattern=zebra_pattern();
    z72_u32 output=zebra_photo_output(),root=output==4 ? 0xb1d6ad3cu : 0xb1d697dcu;
    z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u),object=zebra_output_objects[output==4];
    z72_u32 active=Z72_ZEBRA_BYTE(root+0x5d8),capacity=Z72_ZEBRA_WORD(root+0xc);
    const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
    z72_u32 valid=!(state&3u) && state>=0xb0000000u && state<=0xc0000000u-0x269u;
    z72_u32 object_valid=!(object&3u) && object>=0xb0000000u && object<=0xc0000000u-0x84u;
    z72_u32 eligible=pattern && pattern<=2 && row[37] &&
        row[10]==pattern && row[11]==z72_config_current.photo_zebra_threshold && valid &&
        (Z72_ZEBRA_BYTE(0xb1d178dcu)==3 || Z72_ZEBRA_BYTE(0xb1d178dcu)==4) &&
        Z72_ZEBRA_BYTE(state+0x268)<=4 && Z72_ZEBRA_WORD(state+0x28)==1 &&
        Z72_ZEBRA_BYTE(state+0x88)==(output==4 ? 2u : 0u) &&
        object_valid && !Z72_ZEBRA_BYTE(object+0x80) && capacity==8 && active && active<=2;
    z72_u32 changed=0,first=0,before=0xffffffffu,seen=0;
    __atomic_fetch_add(&zebra_source_sequence,1,__ATOMIC_ACQ_REL);
    zebra_source_record[8]=capacity; zebra_source_record[9]=active;
    z72_u32 image=Z72_ZEBRA_WORD(0xb185f7d0u);
    zebra_source_record[10]=image;
    zebra_source_record[11]=!(image&3u) && image>=0xb0000000u && image<=0xc0000000u-0x46cu ?
                            Z72_ZEBRA_WORD(image+0x468) : 0xffffffffu;
    zebra_source_record[12]=object_valid ? Z72_ZEBRA_WORD(object+0x5c) : 0xffffffffu;
    zebra_source_record[13]=eligible;
    zebra_source_record[14]=Z72_ZEBRA_WORD(root+0x70);
    zebra_source_record[15]=Z72_ZEBRA_WORD(root+0x74);
    for (z72_u32 i=0;i<11;++i) {
        z72_u32 source=Z72_ZEBRA_WORD(root+(i<8 ? 0x10+i*4 : 0x714+(i-8)*4));
        zebra_source_record[16+i]=source;
        zebra_source_record[27+i]=0xffffffffu;
        if (source<root+0xf8 || source>=root+0x538 ||
            (source-root-0xf8)%0x88u) continue;
        z72_u32 flag=Z72_ZEBRA_WORD(source+0x68);
        zebra_source_record[27+i]=flag;
        if (!first) { first=source; before=flag; }
        ++seen;
        if (eligible && !flag) { Z72_ZEBRA_WORD(source+0x68)=1; ++changed; }
    }
    zebra_source_changed+=changed;
    zebra_source_record[0]=event; zebra_source_record[1]=seen;
    zebra_source_record[2]=changed; zebra_source_record[3]=first;
    zebra_source_record[4]=before;
    zebra_source_record[5]=first ? Z72_ZEBRA_WORD(first+0x68) : 0xffffffffu;
    zebra_source_record[6]=valid ? Z72_ZEBRA_WORD(state+0x20) : 0xffffffffu;
    zebra_source_record[7]=valid ? Z72_ZEBRA_WORD(state+0x1c) : 0xffffffffu;
    __atomic_fetch_add(&zebra_source_sequence,1,__ATOMIC_RELEASE);
#else
    (void)event;
#endif
}

/* Deferred display events consume the queued image, not the earlier batch entry.
 * Keep its gate enabled before the original consumer can disable the hardware. */
static void zebra_pending_update(void)
{
#if !Z72_ZEBRA_OBSERVE_ONLY
    if (zebra_photo_context(Z72_ZEBRA_WORD(0xb25519b8u))) return;
    z72_u32 output=zebra_photo_output();
    z72_u32 pattern=zebra_pattern();
    const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
    if (!pattern || pattern>2 || !row[37] || row[10]!=pattern ||
        row[11]!=z72_config_current.photo_zebra_threshold) return;
    z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u);
    if ((state&3u) || state<0xb0000000u || state>0xc0000000u-0x558u ||
        Z72_ZEBRA_WORD(state+0x28)!=1 ||
        Z72_ZEBRA_BYTE(state+0x88)!=(output==4 ? 2u : 0u)) return;
    z72_u32 phase=Z72_ZEBRA_BYTE(0xb1d178dcu);
    if ((phase!=3 && phase!=4) || Z72_ZEBRA_BYTE(state+0x268)>4) return;
    z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,output,0);
    if ((object&3u) || object<0xb0000000u || object>0xc0000000u-0x84u ||
        Z72_ZEBRA_BYTE(object+0x80)) return;
    z72_u32 pending=Z72_ZEBRA_WORD(0xb1d0da48u),cached=0;
    z72_u32 before=0xffffffffu,cache_before=0xffffffffu;
    __atomic_fetch_add(&zebra_pending_sequence,1,__ATOMIC_ACQ_REL);
    ++zebra_pending_calls;
    if (!(pending&3u) && pending>=0xb0000000u && pending<=0xc0000000u-0x34u) {
        before=Z72_ZEBRA_WORD(pending+0x30);
        if (!before) { Z72_ZEBRA_WORD(pending+0x30)=1; ++zebra_pending_changed; }
    }
    if (Z72_ZEBRA_BYTE(state+0x51c)==1 && !Z72_ZEBRA_BYTE(state+0x520)) {
        cached=state+0x524;
        cache_before=Z72_ZEBRA_WORD(cached+0x30);
        if (!cache_before) { Z72_ZEBRA_WORD(cached+0x30)=1; ++zebra_cached_changed; }
    }
    zebra_pending_record[0]=pending; zebra_pending_record[1]=before;
    zebra_pending_record[2]=before==0 ? 1 : before;
    zebra_pending_record[3]=cached; zebra_pending_record[4]=cache_before;
    zebra_pending_record[5]=cache_before==0 ? 1 : cache_before;
    zebra_pending_record[6]=Z72_ZEBRA_WORD(state+0x20);
    zebra_pending_record[7]=Z72_ZEBRA_WORD(state+0x1c);
    __atomic_fetch_add(&zebra_pending_sequence,1,__ATOMIC_RELEASE);
#endif
}

/* The native image consumer copies descriptor+0x30 to its per-frame gate,
 * independently of the highlight parameters carried by command 0x07. */
static void zebra_frame_update(const volatile z72_u32 *msg)
{
    ++zebra_frame_batches;
    z72_u32 batch=msg[5];
    if ((batch&3u) || batch<0xb0000000u || batch>0xc0000000u-8u) return;
    z72_u32 count=Z72_ZEBRA_WORD(batch),entries=Z72_ZEBRA_WORD(batch+4);
    if (!count || count>5 || (entries&3u) || entries<0xb0000000u ||
        entries>0xc0000000u-count*8u) return;
    for (z72_u32 i=0;i<count;++i) {
        if ((Z72_ZEBRA_WORD(entries+i*8)&255u)!=0) continue;
        z72_u32 descriptor=Z72_ZEBRA_WORD(entries+i*8+4);
        if ((descriptor&3u) || descriptor<0xb0000000u || descriptor>0xc0000000u-0x34u) continue;
        ++zebra_frame_images;
        z72_u32 mode=Z72_ZEBRA_WORD(0xb25519b8u);
        if (mode) continue;
        z72_u32 before=Z72_ZEBRA_WORD(descriptor+0x30),reason=0;
        z72_u32 pattern=zebra_pattern();
#if !Z72_ZEBRA_OBSERVE_ONLY
        reason=zebra_photo_context(mode);
        if (!reason && (!pattern || pattern>2)) reason=pattern ? 6u : 9u;
        if (!reason && before>1) reason=6;
        if (!reason) {
            z72_u32 output=zebra_photo_output(),state=Z72_ZEBRA_WORD(0xb1808580u);
            const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
            if (!row[37] || row[10]!=pattern || row[11]!=z72_config_current.photo_zebra_threshold ||
                (state&3u) || state<0xb0000000u || state>0xc0000000u-0x8cu ||
                Z72_ZEBRA_WORD(state+0x28)!=1 ||
                Z72_ZEBRA_BYTE(state+0x88)!=(output==4 ? 2u : 0u)) reason=6;
        }
        if (!reason) {
            z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,zebra_photo_output(),0);
            if ((object&3u) || object<0xb0000000u || object>0xc0000000u-0x84u) reason=7;
            else if (Z72_ZEBRA_BYTE(object+0x80)) reason=5;
        }
        if (!reason) { Z72_ZEBRA_WORD(descriptor+0x30)=1; reason=1; }
#endif
        z72_u32 after=Z72_ZEBRA_WORD(descriptor+0x30);
        if (after!=before) ++zebra_frame_changed;
        /* Keep the last normal photo view when menu/zoom packets arrive later. */
        if (zebra_photo_context(mode)) continue;
        __atomic_fetch_add(&zebra_frame_sequence,1,__ATOMIC_ACQ_REL);
        zebra_frame_record[0]=msg[4]; zebra_frame_record[1]=batch;
        zebra_frame_record[3]=descriptor;
        zebra_frame_record[4]=before; zebra_frame_record[5]=after;
        zebra_frame_record[6]=Z72_ZEBRA_BYTE(descriptor+1);
        zebra_frame_record[7]=Z72_ZEBRA_WORD(0xb181d3a4u);
        zebra_frame_record[8]=Z72_ZEBRA_BYTE(0xb1824f0cu);
        zebra_frame_record[9]=pattern; zebra_frame_record[10]=reason;
        z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u);
        z72_u32 valid=!(state&3u) && state>=0xb0000000u && state<=0xc0000000u-0x2cu;
        zebra_frame_record[2]=valid ? Z72_ZEBRA_WORD(state+0x28) : 0xffffffffu;
        zebra_frame_record[11]=valid ? Z72_ZEBRA_WORD(state+0x20) : 0xffffffffu;
        for (z72_u32 j=0;j<236;++j) zebra_frame_snapshot[j]=0;
        zebra_frame_snapshot[229]=state;
        if (valid && state<=0xc0000000u-0x280u && descriptor<=0xc0000000u-0xb0u) {
            for (z72_u32 j=0;j<160;++j) zebra_frame_snapshot[j]=Z72_ZEBRA_WORD(state+j*4);
            for (z72_u32 j=0;j<44;++j) zebra_frame_snapshot[160+j]=Z72_ZEBRA_WORD(descriptor+j*4);
            for (z72_u32 j=0;j<16;++j) zebra_frame_snapshot[204+j]=Z72_ZEBRA_WORD(0xb180854cu+j*4);
            for (z72_u32 j=0;j<8;++j) zebra_frame_snapshot[220+j]=Z72_ZEBRA_WORD(0xb1d178dcu+j*4);
            zebra_frame_snapshot[228]=1;
            zebra_frame_snapshot[230]=0xb0; zebra_frame_snapshot[231]=0x280;
            zebra_frame_snapshot[232]=msg[4];
            zebra_frame_snapshot[233]=Z72_ZEBRA_BYTE(0xb180854cu);
            zebra_frame_snapshot[234]=Z72_ZEBRA_BYTE(0xb1d178dcu);
        }
        __atomic_fetch_add(&zebra_frame_sequence,1,__ATOMIC_RELEASE);
    }
}

/* The parameter receiver only marks its state dirty. Photo live view can keep
 * running without another metadata update to consume that state. Apply the
 * native decision in the same display task after its original handler returns. */
static void zebra_commit(z72_u32 state,z72_u32 result)
{
#if Z72_ZEBRA_OBSERVE_ONLY
    (void)state; (void)result;
#else
    z72_u32 output=zebra_photo_output();
    if (result!=1 || !output) return;
    z72_u32 phase=Z72_ZEBRA_BYTE(0xb1d178dcu);
    z72_u32 format=Z72_ZEBRA_BYTE(state+0x268);
    if ((phase!=3 && phase!=4) || format>4) return;
    z72_u32 enabled=Z72_ZEBRA_WORD(state+0x28);
    z72_u32 pattern=zebra_pattern();
    if (enabled>1 || pattern>2 || z72_config_current.photo_zebra_threshold>7) return;
    if (pattern) {
        /* Require our current parameters, not an inherited movie request. */
        const volatile z72_u32 *row=zebra_packet_records[output==4 ? 2 : 0];
        if (!enabled || Z72_ZEBRA_BYTE(state+0x88)!=(output==4 ? 2u : 0u) ||
            !row[37] || row[10]!=pattern ||
            row[11]!=z72_config_current.photo_zebra_threshold) return;
        z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,output,0);
        if ((object&3u) || object<0xb0000000u || object>0xc0000000u-0x84u ||
            Z72_ZEBRA_BYTE(object+0x80)) return;
        Z72_ZEBRA_WORD(state+0x20)=1;
    } else if (enabled) return;
    if (!Z72_ZEBRA_WORD(state+0x24) &&
        Z72_ZEBRA_WORD(state+0x1c)==enabled) return;
    ++zebra_commit_count;
    zebra_commit_result=Z72_ZEBRA_CALL(0x10448251u,z72_u32,z72_u32)(format);
#endif
}

/* Observe retained state after the original callback and eligible photo apply. */
static void zebra_packet_complete(z72_u32 command,z72_u32 result)
{
    z72_u32 output=zebra_command_output(command);
    if (!output || output!=zebra_photo_output()) return;
    z72_u32 local=command-(output==4 ? 0x2000u : 0u);
    if (local!=0x42060007u && local!=0x42060004u && local!=0x42060005u &&
        local!=0x4206001eu && local!=0x4206001fu) return;
    z72_u32 mode=Z72_ZEBRA_WORD(0xb25519b8u);
    if (zebra_photo_context(mode)) return;
    z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u);
    z72_u32 valid=!(state&3u) && state>=0xb0000000u && state<=0xc0000000u-0x26cu;
    z72_u32 native_flags=valid ? zebra_state_flags(state) : 0xffffffffu;
    if (valid && local!=0x4206001eu && local!=0x4206001fu) zebra_commit(state,result);
    if (valid && zebra_history_command==command) {
        __atomic_fetch_add(&zebra_history_sequence,1,__ATOMIC_ACQ_REL);
        volatile z72_u32 *entry=zebra_history[zebra_history_count&15u];
        entry[0]=command; entry[1]=zebra_history_before;
        entry[2]=native_flags; entry[3]=zebra_state_flags(state);
        ++zebra_history_count;
        __atomic_fetch_add(&zebra_history_sequence,1,__ATOMIC_RELEASE);
    }
    zebra_history_command=0;
    __atomic_fetch_add(&zebra_post_sequence,1,__ATOMIC_ACQ_REL);
    zebra_post_record[0]=command; zebra_post_record[1]=result; zebra_post_record[2]=state;
    zebra_post_record[3]=valid ? Z72_ZEBRA_WORD(state+0x28) : 0xffffffffu;
    zebra_post_record[4]=valid ? Z72_ZEBRA_WORD(state+0x20) : 0xffffffffu;
    zebra_post_record[5]=valid ? Z72_ZEBRA_WORD(state+0x24) : 0xffffffffu;
    zebra_post_record[6]=valid ? Z72_ZEBRA_WORD(state+0x1c) : 0xffffffffu;
    zebra_post_record[7]=valid ? Z72_ZEBRA_BYTE(state+0x268) : 0xffffffffu;
    zebra_post_record[8]=Z72_ZEBRA_BYTE(0xb1d178dcu);
    zebra_post_record[9]=mode; zebra_post_record[10]=valid;
    zebra_post_record[11]=Z72_ZEBRA_WORD(0xb181d3a4u);
    zebra_post_record[12]=zebra_pattern();
    zebra_post_record[13]=z72_config_current.photo_zebra_threshold;
    z72_u32 object=zebra_last_object;
    zebra_post_record[14]=!(object&3u) && object>=0xb0000000u && object<=0xc0000000u-0x84u ?
                            Z72_ZEBRA_BYTE(object+0x80) : 0xffffffffu;
    zebra_post_record[15]=zebra_packet_applied;
    ++zebra_post_count;
    __atomic_fetch_add(&zebra_post_sequence,1,__ATOMIC_RELEASE);
}

/* Preserve the message and completion callback. Both LCD and EVF packets
 * arrive on the secondary display task; the primary task handles HDMI. */
static void zebra_packet_update(z72_u32 channel,const void *message)
{
    ++zebra_packet_calls[channel];
    z72_u32 address=(z72_u32)message;
    if ((address&3u) || address<0xb0000000u || address>0xc0000000u-24u) {
        ++zebra_packet_invalid[channel]; return;
    }
    const volatile z72_u32 *msg=message;
    z72_u32 command=msg[4];
    z72_u32 output=zebra_command_output(command);
    z72_u32 local=command-(output==4 ? 0x2000u : 0u);
    z72_u32 selected=output && output==zebra_photo_output();
    if (channel==1) {
        zebra_history_command=0;
        if (selected && (local==0x42060007u || local==0x42060004u || local==0x42060005u ||
             local==0x4206001eu || local==0x4206001fu) &&
            !zebra_photo_context(Z72_ZEBRA_WORD(0xb25519b8u))) {
            z72_u32 state=Z72_ZEBRA_WORD(0xb1808580u);
            if (!(state&3u) && state>=0xb0000000u && state<=0xc0000000u-0x2cu) {
                zebra_history_before=zebra_state_flags(state);
                zebra_history_command=command;
            }
        }
    }
    if (channel==1 && selected && (local==0x4206001eu || local==0x4206001fu)) {
        zebra_source_update(command);
        zebra_pending_update(); return;
    }
    if (channel==1 && selected && (local==0x42060004u || local==0x42060005u)) {
        zebra_frame_update(msg); return;
    }
    if (channel!=1 || (command!=0x42060007u && command!=0x42062007u)) return;
    z72_u32 mode=Z72_ZEBRA_WORD(0xb25519b8u);
    if (mode>1) { ++zebra_packet_invalid[channel]; return; }
    z72_u32 bank=(command==0x42062007u ? 2u : 0u)+mode;
    z72_u32 count=++zebra_packet_counts[bank];
    z72_u32 menu=Z72_ZEBRA_BYTE(0xb1824f0cu);
    if (menu>=1 && menu<=3) { ++zebra_packet_menus[bank]; return; }
    z72_u32 payload=msg[5];
    if ((payload&3u) || payload<0xb0000000u || payload>0xc0000000u-100u) {
        ++zebra_packet_invalid[channel]; return;
    }
    volatile z72_u32 *data=(volatile z72_u32 *)payload;
    z72_u32 enabled=data[0];
    if (enabled>1) { ++zebra_packet_invalid[channel]; return; }
    __atomic_fetch_add(&zebra_packet_sequence[bank],1,__ATOMIC_ACQ_REL);
    volatile z72_u32 *row=zebra_packet_records[bank];
    row[0]=count; row[1]=command; row[2]=address; row[3]=payload;
    row[4]=mode; row[5]=menu; row[6]=Z72_ZEBRA_BYTE(0xb1828174u);
    row[7]=Z72_ZEBRA_WORD(0xb181d3a4u);
    row[8]=Z72_ZEBRA_BYTE(0xb202be70u); row[9]=Z72_ZEBRA_BYTE(0xb202c00eu);
    row[10]=zebra_pattern();
    row[11]=z72_config_current.photo_zebra_threshold;
    /* Disabled requests define only their first word; do not read stale fields. */
    row[12]=enabled;
    for (z72_u32 i=1;i<25;++i) row[12+i]=enabled ? data[i] : 0;
    row[39]=zebra_packet_apply(data,mode,command,row[10],row[11]);
    row[37]=row[39]==1;
    row[38]=data[0];
    if (row[37]) ++zebra_packet_applied;
    __atomic_fetch_add(&zebra_packet_sequence[bank],1,__ATOMIC_RELEASE);
}

static void zebra_graphics_observe(void)
{
    ++zebra_graphics_calls;
}

static void zebra_probe_snapshot(z72_u32 *row)
{
    z72_u32 (*property)(z72_u32)=Z72_ZEBRA_CALL(0x1063c7d5u,z72_u32,z72_u32);
    row[0]=zebra_gui_calls; row[1]=zebra_graphics_calls;
    row[2]=property(0x168c); row[3]=Z72_ZEBRA_WORD(0xb25519b8u);
    row[4]=Z72_ZEBRA_WORD(0xb181d3a4u); row[5]=Z72_ZEBRA_BYTE(0xb181da91u);
    row[6]=Z72_ZEBRA_BYTE(0xb1821b18u); row[7]=Z72_ZEBRA_BYTE(0xb202be70u);
    row[8]=Z72_ZEBRA_BYTE(0xb202c00eu); row[9]=Z72_ZEBRA_BYTE(0xb202c016u);
    row[10]=Z72_ZEBRA_WORD(0xb181dabcu);
    row[11]=Z72_ZEBRA_BYTE(0xb1824f0cu); row[12]=Z72_ZEBRA_BYTE(0xb1828174u);
    row[13]=Z72_ZEBRA_CALL(0x1057d299u,z72_u32,void)();
    row[14]=row[15]=row[16]=0;
    z72_u32 output=row[4]&7u;
    if (output==1 || output==4) {
        z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,output,0);
        row[14]=object;
        if (!(object&3) && object>=0xb0000000u && object<0xc0000000u-0x84) {
            row[15]=Z72_ZEBRA_WORD(object+0x5c); row[16]=Z72_ZEBRA_BYTE(object+0x80);
        }
    }
    row[17]=zebra_pattern();
    row[18]=z72_config_current.photo_zebra_threshold;
    row[19]=property(0x169a); row[20]=property(0x118e); row[21]=property(0x118f);
    row[22]=Z72_ZEBRA_WORD(0xb181dd90u); row[23]=Z72_ZEBRA_OBSERVE_ONLY;
}

static void zebra_probe_capture(void)
{
    ++zebra_gui_calls;
    z72_u32 row[24];
    zebra_probe_snapshot(row);
    z72_u32 count=zebra_probe_count;
    if (count<16) {
        for (z72_u32 i=0;i<24;++i) zebra_probe_records[count][i]=row[i];
        __asm__ volatile("dmb sy" ::: "memory");
        zebra_probe_count=count+1;
    }
    if (!row[2] && !row[6] && !row[7] && !row[10] &&
        !(row[11]>=1 && row[11]<=3) && !(row[12]>=1 && row[12]<=3)) {
        __atomic_fetch_add(&zebra_probe_sequence,1,__ATOMIC_ACQ_REL);
        for (z72_u32 i=0;i<24;++i) zebra_probe_last[i]=row[i];
        __atomic_fetch_add(&zebra_probe_sequence,1,__ATOMIC_RELEASE);
    }
}

/* Native 36-byte highlight request. Only live-view output state is changed. */
struct z72_zebra_request {
    z72_u32 enabled;
    unsigned char reserved[2];
    unsigned short first[4], second[4];
    unsigned short padding;
    z72_u32 horizontal, vertical;
    unsigned char pattern[2], tail[2];
};
_Static_assert(sizeof(struct z72_zebra_request)==36,"Highlight request size");

static inline void z72_zebra_make(struct z72_zebra_request *request,
                                 z72_u32 output,z72_u32 pattern,z72_u32 threshold)
{
    static const unsigned short levels[]={255,248,235,224,213,202,191,180};
    unsigned short value=threshold<8 ? levels[threshold] : 248;
    for (z72_u32 i=0;i<sizeof(*request);++i) ((volatile unsigned char *)request)[i]=0;
    request->enabled=pattern==1 || pattern==2;
    request->first[0]=request->second[0]=255;
    request->first[1]=request->second[1]=value;
    request->first[2]=output==4 ? 13 : 15;
    request->second[2]=15;
    request->first[3]=request->second[3]=5;
    request->horizontal=request->vertical=4;
    request->pattern[0]=request->pattern[1]=pattern==1 ? 0 : pattern==2 ? 1 : 2;
}

/* Called only while an existing LCD/EVF highlight packet is being dispatched.
 * The native converter reads tables and writes this task's temporary buffer;
 * it neither queues a request nor waits for a graphics completion. */
static z72_u32 zebra_packet_apply(volatile z72_u32 *data,z72_u32 mode,z72_u32 command,
                                 z72_u32 pattern,z72_u32 threshold)
{
#if Z72_ZEBRA_OBSERVE_ONLY
    (void)data; (void)mode; (void)command; (void)pattern; (void)threshold;
    return 0;
#else
    if (mode) return 2;
    z72_u32 output=zebra_command_output(command);
    if (!output || output!=zebra_photo_output()) return 3;
    z72_u32 context=zebra_photo_context(mode);
    if (context) return context;
    zebra_last_object=zebra_last_palette=0;
    zebra_output_objects[output==4]=0;
    if (pattern>2 || threshold>7) return 6;
    if (!pattern) { data[0]=0; return 1; }
    z72_u32 object=Z72_ZEBRA_CALL(0x1068cf07u,z72_u32,z72_u32,z72_u32,z72_u32)(0,output,0);
    zebra_last_object=object;
    zebra_output_objects[output==4]=object;
    if ((object&3u) || object<0xb0000000u || object>0xc0000000u-0x84u) return 7;
    if (Z72_ZEBRA_BYTE(object+0x80)) return 5;
    z72_u32 palette=Z72_ZEBRA_CALL(0x1068f3efu,z72_u32,z72_u32,z72_u32)(1,0);
    zebra_last_palette=palette;
    /* The native getter supplies a mapped display address, not a heap pointer. */
    if (!palette || (palette&3u)) return 8;
    struct z72_zebra_request request;
    z72_u32 converted[25];
    for (z72_u32 i=0;i<25;++i) ((volatile z72_u32 *)converted)[i]=0;
    z72_zebra_make(&request,output,pattern,threshold);
    Z72_ZEBRA_CALL(0x10578921u,void,void *,const void *,z72_u32)(converted,&request,output);
    for (z72_u32 i=1;i<25;++i) data[i]=converted[i];
    __asm__ volatile("dmb sy" ::: "memory");
    data[0]=converted[0];
    return 1;
#endif
}

static inline void z72_photo_zebra_update(void)
{
    zebra_producer_install();
    zebra_evf_request();
    zebra_probe_capture();
}
#endif
