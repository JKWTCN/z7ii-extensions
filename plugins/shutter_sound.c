/* SPDX-License-Identifier: MIT */
/* Included after pre-capture in the combined SD module. */
#define NATIVE CALL

enum {
    SOUND_START_SLOT=0xb18646c8u, SOUND_START_NATIVE=0x102611cbu,
    SOUND_MAILBOX=0xb18646acu, AUDIO_REPLY_EVENT=0xb18646b0u,
    SOUND_STATE=0xb18646b4u, SOUND_CONTEXT=0xb21e2630u,
    PHOTO_NOTIFY_SLOT=0xb1864d5cu, PHOTO_NOTIFY_NATIVE=0x10321a4du,
    RAW_NOTIFY_SLOT=0xb1887e80u, RAW_NOTIFY_RETURN=0x10230058u,
    PHOTO_RAW_READY=0x041a000fu, DRIVE_MODE_NATIVE=0x104d1e8bu, SINGLE_RELEASE=1u,
    SHUTTER_FULL_NATIVE=0x1022aa5du, SHUTTER_UP_NATIVE=0x1022aa25u,
    PHOTO_INPUT_READY=0x041b0090u, SOUND_REQUEST=0x0419001eu,
    SOUND_FINISHED=0x04190023u, FILE_OK=0x043a0080u,
    RATE=48000u, MAX_FRAMES=RATE, MAX_WAV=MAX_FRAMES*4+4096,
    IDLE=0, QUEUED=1, PLAYING=2, EARLY_SLOTS=16
};

static const char sound_paths[Z72_SOUND_CLIP_COUNT][16]={
    "B:\\SHUTTER1.WAV","B:\\SHUTTER2.WAV","B:\\SHUTTER3.WAV","B:\\SHUTTER4.WAV","B:\\SHUTTER5.WAV"
};
static _Alignas(64) unsigned char wav[MAX_WAV+64];
static _Alignas(64) short clips[Z72_SOUND_CLIP_COUNT][MAX_FRAMES*2];
static _Alignas(64) short playback_pcm[MAX_FRAMES*2];
static z72_u32 clip_bytes[Z72_SOUND_CLIP_COUNT],playback,sound_choice,burst_state;
static z72_u32 sound_every_frame,sound_volume,prepared_clip,prepared_volume,full_down;
static z72_u32 sound_clips_available;
static _Alignas(8) unsigned long long early_sound[EARLY_SLOTS];

static z72_u32 le16(const unsigned char *p)
{
    return p[0]|((z72_u32)p[1]<<8);
}

static z72_u32 le32(const unsigned char *p)
{
    return le16(p)|(le16(p+2)<<16);
}

static z72_u32 decode_wav(z72_u32 size,short *pcm)
{
    if (size<44 || size>MAX_WAV || le32(wav)!=0x46464952u ||
        le32(wav+4)!=size-8 || le32(wav+8)!=0x45564157u) return 0;
    z72_u32 channels=0, bytes=0;
    const unsigned char *samples=0;
    for (z72_u32 off=12; off<size;) {
        if (size-off<8) return 0;
        z72_u32 id=le32(wav+off), length=le32(wav+off+4);
        off+=8;
        if (length>size-off || (length&1u)>size-off-length) return 0;
        if (id==0x20746d66u) { /* fmt */
            if (channels || length<16 || le16(wav+off)!=1) return 0;
            channels=le16(wav+off+2);
            if ((channels!=1 && channels!=2) || le32(wav+off+4)!=RATE ||
                le32(wav+off+8)!=RATE*channels*2 || le16(wav+off+12)!=channels*2 ||
                le16(wav+off+14)!=16) return 0;
        } else if (id==0x61746164u) { /* data */
            if (samples) return 0;
            samples=wav+off;
            bytes=length;
        }
        off+=length+(length&1u);
    }
    if (!samples || !channels || !bytes || (bytes&(channels*2-1)) ||
        bytes>MAX_FRAMES*channels*2) return 0;
    z72_u32 frames=channels==2 ? bytes/4 : bytes/2;
    for (z72_u32 i=0;i<frames;++i) {
        pcm[i*2]=(short)le16(samples);
        pcm[i*2+1]=channels==2 ? (short)le16(samples+2) : pcm[i*2];
        samples+=channels*2;
    }
    return frames*4;
}

static z72_u32 read_sound(const char *path,short *pcm)
{
    z72_u32 file[10],actual=0;
    for (z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    NATIVE(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    if (NATIVE(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=FILE_OK) return 0;
    z72_u32 size=file[6], result=0;
    if (!file[7] && size>=44 && size<=MAX_WAV) {
        struct { z72_u32 kind; void *buffer; z72_u32 capacity; } read={1,wav,size+1};
        NATIVE(0x1034fa29u,void,void *,z72_u32)(wav,sizeof(wav));
        result=NATIVE(0x104d3d83u,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(
            file,&read,size+1,&actual);
        NATIVE(0x1034fa29u,void,void *,z72_u32)(wav,sizeof(wav));
    }
    z72_u32 closed=NATIVE(0x104d3c91u,z72_u32,z72_u32 *)(file);
    return result==FILE_OK && closed==FILE_OK && actual==size ? decode_wav(size,pcm) : 0;
}

/* C 1.70 native SoundService start job. Only the mailbox envelope is allocated
 * by the native sender. The service copies the record before starting PCM. */
struct sound_record {
    z72_u32 play, tone, alternate, alternate_tone, right, right_tone, repeats;
    z72_u32 type;
    void (*done)(z72_u32,const void *);
};
struct sound_job {
    const struct sound_record *record;
    void (*release)(z72_u32,struct sound_job *);
    z72_u32 status;
    z72_u32 preview;
    unsigned long long photo_key;
    z72_u32 clip,volume,burst;
};

static unsigned long long photo_key(z72_u32 owner)
{
    if ((owner&3u) || owner<0xb1800000u || owner>0xb5effffcu) return 0;
    z72_u32 sequence=WORD(owner);
    return sequence ? ((unsigned long long)owner<<32)|sequence : 0;
}

static unsigned long long *early_slot(unsigned long long key)
{
    return &early_sound[(z72_u32)key%EARLY_SLOTS];
}

static z72_u32 silent_photo(void)
{
    /* A pre-capture session can generate image events during half-press. */
    return (!LOAD(in_flight) || (LOAD(control)&FROZEN)) &&
        !WORD(0xb25519b8u) && NATIVE(0x1063c7d5u,z72_u32,z72_u32)(0x057a)==1;
}

static void restore_burst(z72_u32 burst)
{
    if (!burst) return;
    z72_u32 consumed=burst-1;
    __atomic_compare_exchange_n(&burst_state,&consumed,burst,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void sound_done(z72_u32 event,const void *result)
{
    (void)result;
    if (event==SOUND_FINISHED) __atomic_store_n(&playback,IDLE,__ATOMIC_RELEASE);
}

static void release_sound_job(z72_u32 event,struct sound_job *job)
{
    (void)event;
    if (__atomic_load_n(&playback,__ATOMIC_ACQUIRE)==QUEUED) {
        restore_burst(job->burst);
        __atomic_store_n(&playback,IDLE,__ATOMIC_RELEASE);
    }
}

static const struct sound_record pcm_record={1,31,0,0,0,0,1,18,sound_done};
static struct sound_job pcm_job={.record=&pcm_record,.release=release_sound_job};

static void prepare_pcm(z72_u32 clip,z72_u32 volume)
{
    if (prepared_clip==clip && prepared_volume==volume) return;
    /* Q8 gain keeps all five levels distinct without clipping full-scale WAVs. */
    static const int gains[Z72_SOUND_VOLUME_MAX]={64,102,154,205,256};
    const short *source=clips[clip-1];
    z72_u32 bytes=clip_bytes[clip-1];
    int gain=gains[volume-1];
    for (z72_u32 i=0;i<bytes/2;++i) playback_pcm[i]=(short)((source[i]*gain)/256);
    NATIVE(0x1034f9edu,void,void *,z72_u32)(playback_pcm,bytes);
    prepared_clip=clip;
    prepared_volume=volume;
}

/* Only our PCM job takes this branch. Stock jobs retain the original SoundService
 * dispatcher. Configure its output and hand playback retirement back to the
 * original DMA/stop callbacks. */
static z72_u32 sound_start(const z72_u32 *message)
{
    struct sound_job *job=(struct sound_job *)message[5];
    if (job!=&pcm_job)
        return NATIVE(SOUND_START_NATIVE,z72_u32,const z72_u32 *)(message);

    unsigned char speaker=0;
    NATIVE(0x1036f781u,z72_u32,unsigned char *)(&speaker);
    if (speaker!=1 || NATIVE(0x1036f867u,z72_u32,void)() || BYTE(SOUND_STATE)) return 0;
    if (__atomic_load_n(&sound_choice,__ATOMIC_ACQUIRE)!=job->clip ||
        (!job->preview && !silent_photo())) return 0;

    z72_u32 pcm_bytes=clip_bytes[job->clip-1];
    prepare_pcm(job->clip,job->volume);
    NATIVE(0x10360f44u,void,void *,const void *,z72_u32)(
        (void *)SOUND_CONTEXT,job->record,sizeof(*job->record));
    WORD(SOUND_STATE+4)=WORD(SOUND_STATE+12)=WORD(SOUND_STATE+16)=0;

    const z72_u32 format[]={0x01010502u,pcm_bytes,0};
    const z72_u32 tones[]={2,1,31,0,0,0,0,0};
    const z72_u32 data[]={(z72_u32)playback_pcm,pcm_bytes,1,0,0,0};
    const z72_u32 start[]={2,0x1026118du,0};
    if (NATIVE(0x10260cb5u,z72_u32,const z72_u32 *)(format)!=1 ||
        NATIVE(0x10260b4du,z72_u32,const z72_u32 *)(tones)!=1 ||
        NATIVE(0x10260e6bu,z72_u32,const z72_u32 *)(data)!=1 ||
        NATIVE(0x10260cfdu,z72_u32,const z72_u32 *)(start)!=1) return 0;
    BYTE(SOUND_STATE+1)=1;
    /* The native right-channel flag is set only when a second playback
     * request is started. This stereo PCM clip uses one DMA stream. */
    BYTE(SOUND_STATE+2)=0;
    BYTE(SOUND_STATE)=1;
    if (job->photo_key) __atomic_store_n(early_slot(job->photo_key),job->photo_key,__ATOMIC_RELEASE);
    __atomic_store_n(&playback,PLAYING,__ATOMIC_RELEASE);
    return 1;
}

static void enqueue_sound(z72_u32 preview,unsigned long long key,z72_u32 first_only)
{
    z72_u32 clip=__atomic_load_n(&sound_choice,__ATOMIC_ACQUIRE);
    if (!clip || BYTE(SOUND_STATE) || !WORD(SOUND_MAILBOX) || !WORD(AUDIO_REPLY_EVENT) ||
        (!preview && !silent_photo())) return;
    z72_u32 idle=IDLE;
    if (!__atomic_compare_exchange_n(&playback,&idle,QUEUED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return;

    z72_u32 burst=0;
    if (first_only) {
        burst=__atomic_load_n(&burst_state,__ATOMIC_ACQUIRE);
        if (!(burst&1u) || !__atomic_compare_exchange_n(&burst_state,&burst,burst-1,0,
                                                      __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            __atomic_store_n(&playback,IDLE,__ATOMIC_RELEASE);
            return;
        }
    }
    pcm_job.status=0;
    pcm_job.preview=preview;
    pcm_job.photo_key=key;
    pcm_job.clip=clip;
    pcm_job.volume=__atomic_load_n(&sound_volume,__ATOMIC_ACQUIRE);
    pcm_job.burst=burst;
    if (NATIVE(0x102b325du,z72_u32,z72_u32,struct sound_job *)(SOUND_REQUEST,&pcm_job)==1) return;
    restore_burst(burst);
    __atomic_store_n(&playback,IDLE,__ATOMIC_RELEASE);
}

static z72_u32 burst_first_only(z72_u32 drive)
{
    return !__atomic_load_n(&sound_every_frame,__ATOMIC_ACQUIRE) &&
        (drive==2 || drive==0x8010 || drive==0x8019);
}

static void sound_shutter_event(z72_u32 fn)
{
    if (!sound_clips_available) return;
    if (fn==SHUTTER_UP_NATIVE) full_down=0;
    else if (fn==SHUTTER_FULL_NATIVE && !full_down) {
        full_down=1;
        z72_u32 prior=__atomic_load_n(&burst_state,__ATOMIC_RELAXED);
        while (!__atomic_compare_exchange_n(&burst_state,&prior,(prior+2)|1u,0,
                                            __ATOMIC_RELEASE,__ATOMIC_RELAXED)) {}
    }
}

static void sound_input(z72_u32 flags)
{
    for (z72_u32 bits=flags&0xffffu;bits;bits&=bits-1)
        sound_shutter_event(WORD(INPUT_CALLBACKS+4*__builtin_ctz(bits)));
    NATIVE(INPUT_NATIVE,void,z72_u32)(flags);
}

static z72_u32 photo_notify(z72_u32 event,z72_u32 argument,z72_u32 a2,z72_u32 a3)
{
    /* The RAW task supplies {flags, owner} at this call site. The later
     * image handoff identifies the same owner and sequence. */
    if (__atomic_load_n(&sound_choice,__ATOMIC_ACQUIRE) && event==PHOTO_RAW_READY && argument &&
        ((z72_u32)__builtin_return_address(0)&~1u)==RAW_NOTIFY_RETURN &&
        silent_photo()) {
        z72_u32 drive=NATIVE(DRIVE_MODE_NATIVE,z72_u32,void)();
        unsigned long long key=photo_key(WORD(argument+4));
        if (key && __atomic_load_n(early_slot(key),__ATOMIC_ACQUIRE)!=key) {
            if (drive==SINGLE_RELEASE) enqueue_sound(0,key,0);
            else if (burst_first_only(drive)) {
                /* Associate already-suppressed RAW frames with their own
                 * handoffs, even when the next full press arrives first. */
                if (!(__atomic_load_n(&burst_state,__ATOMIC_ACQUIRE)&1u))
                    __atomic_store_n(early_slot(key),key,__ATOMIC_RELEASE);
                else enqueue_sound(0,key,1);
            }
        }
    }
    if (event==PHOTO_INPUT_READY && argument) {
        unsigned long long key=photo_key(argument);
        if ((!key || !__atomic_compare_exchange_n(early_slot(key),&key,0,0,
                                                 __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) &&
            __atomic_load_n(&sound_choice,__ATOMIC_ACQUIRE))
            enqueue_sound(0,0,burst_first_only(NATIVE(DRIVE_MODE_NATIVE,z72_u32,void)()));
    }
    return NATIVE(PHOTO_NOTIFY_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(event,argument,a2,a3);
}

static u32 sound_ready(void)
{
    return WORD(PHOTO_NOTIFY_SLOT)==PHOTO_NOTIFY_NATIVE && WORD(SOUND_START_SLOT)==SOUND_START_NATIVE &&
        WORD(INPUT_SLOT)==INPUT_NATIVE && WORD(INPUT_NATIVE-1u)==0x41f0e92du &&
        WORD(PHOTO_NOTIFY_NATIVE-1u)==0x4ff0e92du &&
        WORD(0x10260cb4u)==0xb08cb510u && WORD(0x102b325cu)==0x460cb570u;
}

static void sound_load_clips(void)
{
    for (z72_u32 i=0;i<Z72_SOUND_CLIP_COUNT;++i) {
        clip_bytes[i]=read_sound(sound_paths[i],clips[i]);
        sound_clips_available|=clip_bytes[i];
    }
}

static void sound_install(void)
{
    if (!sound_clips_available) return;
    WORD(SOUND_START_SLOT)=(z72_u32)sound_start;
    __asm__ volatile("dmb sy" ::: "memory");
    /* Keep the pre-capture dispatcher as the slot owner when it is installed. */
    if (WORD(INPUT_SLOT)==INPUT_NATIVE) WORD(INPUT_SLOT)=(z72_u32)sound_input;
    WORD(PHOTO_NOTIFY_SLOT)=(z72_u32)photo_notify;
    if (WORD(RAW_NOTIFY_SLOT)==PHOTO_NOTIFY_NATIVE && WORD(RAW_NOTIFY_RETURN-4u)==0x47904669u &&
        WORD(DRIVE_MODE_NATIVE+1u)==0x200841f0u)
        WORD(RAW_NOTIFY_SLOT)=(z72_u32)photo_notify;
}

#include "shutter_sound_menu.h"
#undef NATIVE
