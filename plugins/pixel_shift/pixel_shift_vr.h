/* SPDX-License-Identifier: MIT */
enum {
    TELEMETRY_ADDRESS = 0x0fe0,
    TELEMETRY_SIZE = 0x2a,
    BODY_VR_STATE = 0xb1866cb0u,
    BODY_VR_QUEUE = 0xb1866bfcu,
    BODY_VR_PENDING = 0xb1866c08u,
    BODY_VR_CALLBACK = 0xb1866ab4u,
    BODY_VR_RESPONSE = 0x04300021u
};

struct vr_request {
    unsigned short opcode, reserved;
    z72_u32 address, length;
    z72_u32 reserved_words[4];
    const void *payload;
    z72_u32 reserved_word;
    void *output;
};

_Static_assert(sizeof(struct vr_request)==0x28, "BodyVr request size");
_Static_assert(__builtin_offsetof(struct vr_request,output)==0x24, "BodyVr read destination");

struct vr_sample {
    z72_u32 status,d2_cache;
    unsigned char telemetry[TELEMETRY_SIZE];
};

static unsigned char response[TELEMETRY_SIZE];
static const struct vr_request request={
    .opcode=0xf040,.address=TELEMETRY_ADDRESS,.length=TELEMETRY_SIZE,.output=response
};
static z72_u32 reply;

static z72_u32 pixel_tick(void)
{
    return CALL(0x1062e59du,z72_u32,void)();
}

static void pixel_sleep(z72_u32 interval)
{
    CALL(0x103dc539u,void,z72_u32)(interval);
}

static void read_complete(z72_u32 event,const unsigned char *result)
{
    z72_u32 status=event==BODY_VR_RESPONSE && result ? result[0] : 0x103u;
    __atomic_store_n(&reply,status,__ATOMIC_RELEASE);
}

static z72_u32 queue_depth(void)
{
    z72_u32 head=WORD(BODY_VR_QUEUE);
    return WORD(BODY_VR_QUEUE+4)-head;
}

static int bus_idle(void)
{
    return BYTE(BODY_VR_STATE)==1 && !queue_depth() &&
        !WORD(BODY_VR_PENDING) && !WORD(BODY_VR_CALLBACK) &&
        !WORD(BODY_VR_STATE+0x2c) && !WORD(BODY_VR_STATE+0x30);
}

static z72_u32 exchange(const struct vr_request *message)
{
    if (!bus_idle()) return 0x100u;
    __atomic_store_n(&reply,0x102u,__ATOMIC_RELAXED);
    if (!CALL(0x10393a2du,z72_u32,const struct vr_request *,
        void (*)(z72_u32,const unsigned char *))(message,read_complete)) return 0x101u;

    z72_u32 start=pixel_tick();
    z72_u32 status;
    do {
        status=__atomic_load_n(&reply,__ATOMIC_ACQUIRE);
        if (status!=0x102u) break;
        pixel_sleep(10);
    } while (pixel_tick()-start<5000u);
    return status;
}

static z72_u32 read_position(struct vr_sample *sample)
{
    if (!bus_idle()) return 0x100u;
    for (z72_u32 i=0;i<TELEMETRY_SIZE;++i) response[i]=0;
    z72_u32 status=exchange(&request);
    if (!status)
        for (z72_u32 i=0;i<TELEMETRY_SIZE;++i) sample->telemetry[i]=response[i];
    return status;
}

enum {
    VR_D2_CACHE = 0xb23c4770u,
    VR_D2_SETTINGS = 0xb23be28cu,
    VR_D4_CACHE = 0xb1866af0u,
    VR_CAPTURE_PENDING = BODY_VR_STATE+0x20,
    VR_REPORT = 0x22,
    VR_POSITION = 0x24,
    VR_NORMAL = 0x01,
    VR_HOLD = 0x40
};

struct d2_settings {
    unsigned char selection[2], flags;
};

static const unsigned char center_payload[10]={0,0,0,0,0,16,16,0,0,2};
static const struct vr_request center_request={.opcode=0xf1f1,.payload=center_payload};

static int magnitude(int value)
{
    return value<0 ? -value : value;
}

static int signed_word(const unsigned char *bytes)
{
    return (short)(bytes[0]|(bytes[1]<<8));
}

static int settled(const struct vr_sample *sample)
{
    for (int axis=0;axis<3;++axis) {
        int target=signed_word(sample->telemetry+2*axis);
        int measured=signed_word(sample->telemetry+VR_POSITION+2*axis);
        if (magnitude(target-measured)>48) return 0;
    }
    return 1;
}

static int near_center(const struct vr_sample *sample)
{
    for (int axis=0;axis<3;++axis)
        if (magnitude(signed_word(sample->telemetry+VR_POSITION+2*axis))>128) return 0;
    return 1;
}

static int same_d2_settings(const struct d2_settings *saved)
{
    return !WORD(VR_D4_CACHE) && !WORD(VR_CAPTURE_PENDING) &&
        BYTE(VR_D2_SETTINGS)==saved->selection[0] &&
        BYTE(VR_D2_SETTINGS+1)==saved->selection[1] &&
        BYTE(VR_D2_SETTINGS+8)==saved->flags;
}

static int snapshot_d2(struct d2_settings *saved)
{
    if (WORD(VR_D2_CACHE)!=1 || !bus_idle()) return 0;
    const struct d2_settings current={
        {BYTE(VR_D2_CACHE+4), BYTE(VR_D2_CACHE+5)}, BYTE(VR_D2_CACHE+0xc)
    };
    if (current.selection[0]<1 || current.selection[0]>3 ||
        !same_d2_settings(&current) || WORD(VR_D2_CACHE)!=1) return 0;
    *saved=current;
    return 1;
}

static void read_sample(struct vr_sample *sample)
{
    sample->d2_cache=WORD(VR_D2_CACHE);
    sample->status=read_position(sample);
}

static int wait_bus(z72_u32 timeout)
{
    z72_u32 start=pixel_tick();
    do {
        if (bus_idle()) return 1;
        pixel_sleep(10);
    } while (pixel_tick()-start<timeout);
    return 0;
}

static z72_u32 send_position(const struct vr_request *command,const struct d2_settings *saved)
{
    return wait_bus(250) && same_d2_settings(saved) ? exchange(command) : 0x104u;
}

static int may_have_applied(z72_u32 status)
{
    return status!=0x100u && status!=0x101u && status!=0x104u;
}

static int position_matches(const struct vr_sample *sample,const struct vr_sample *center,
                            int x_target,int y_target)
{
    if (sample->status || sample->telemetry[VR_REPORT]!=VR_HOLD || !settled(sample)) return 0;
    /* With a small step the large-step tolerance can accept no movement.
     * F040 reports actuator coordinates. Y moves both vertical actuators;
     * their difference represents rotation, which pixel shift leaves at zero.
     */
    for (unsigned i=0;i<3;++i) {
        int target=i==0 ? x_target : y_target;
        int measured=signed_word(sample->telemetry+VR_POSITION+2*i);
        if (signed_word(sample->telemetry+2*i)!=target ||
            magnitude(measured-target)>24) return 0;
        if (center && target && i<2) {
            int origin=signed_word(center->telemetry+VR_POSITION+2*i);
            if (magnitude(measured-origin-target)>28) return 0;
        }
    }
    return 1;
}

static int wait_position(const struct vr_sample *center,int x_target,int y_target,
                         struct vr_sample *position)
{
    int consecutive=0;
    struct vr_sample sample;
    for (z72_u32 i=0;i<12;++i) {
        pixel_sleep(100);
        read_sample(&sample);
        if (sample.status==0x102u) break;
        consecutive=position_matches(&sample,center,x_target,y_target) ? consecutive+1 : 0;
        if (consecutive==2) {
            if (position) *position=sample;
            return 1;
        }
    }
    return 0;
}

static int restore_d2(const struct d2_settings *saved)
{
    if (!wait_bus(1500)) return 0;
    struct vr_sample sample;
    read_sample(&sample);
    if (!sample.status && sample.telemetry[VR_REPORT]==VR_NORMAL &&
        sample.d2_cache==1 && settled(&sample)) return 1;
    if (!wait_bus(1000) || !same_d2_settings(saved)) return 0;
    /* Clear the duplicate filters before restoring the native D2 parameters. */
    CALL(0x103b7cfbu,void,z72_u32,z72_u32)(0xa,0x80);
    if (!CALL(0x103939a1u,z72_u32,const unsigned char *,z72_u32,void *)
            (saved->selection,saved->flags,0)) return 0;
    for (z72_u32 i=0;i<14;++i) {
        pixel_sleep(100);
        read_sample(&sample);
        if (!sample.status && sample.telemetry[VR_REPORT]==VR_NORMAL &&
            sample.d2_cache==1 && settled(&sample)) return 1;
        if (sample.status==0x102u) break;
    }
    return 0;
}

static int wait_baseline(struct d2_settings *saved)
{
    int consecutive=0;
    for (z72_u32 i=0;i<24;++i) {
        struct vr_sample sample;
        read_sample(&sample);
        if (sample.status==0x102u) break;
        int match=!sample.status && sample.telemetry[VR_REPORT]==VR_NORMAL &&
            sample.d2_cache==1 && near_center(&sample) && settled(&sample);
        consecutive=match ? consecutive+1 : 0;
        if (consecutive>=2 && wait_bus(100) && snapshot_d2(saved)) return 1;
        pixel_sleep(125);
    }
    return 0;
}
