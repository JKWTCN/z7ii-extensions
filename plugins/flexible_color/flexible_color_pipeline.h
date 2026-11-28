/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_COLOR_PIPELINE_H
#define Z72_FLEXIBLE_COLOR_PIPELINE_H

#include "flexible_publish_action.h"

#define CLOUD_WORKER_SLOT0 0xb1d5295cu
#define CLOUD_WORKER_SLOT1 0xb1d52ab0u
#define CLOUD_WORKER_STOCK0 0x102e5169u
#define CLOUD_WORKER_STOCK1 0x102e505du
#define CLOUD_CAPTURE_POLICY (*(volatile unsigned char *)0xb20d2cc2u)
#define CLOUD_CAPTURE_ACTION 0x10268d6bu
#define CLOUD_FILE_OK 0x043a0080u
#define CLOUD_ROOT_WORDS 53u

static const u32 cloud_capture_slots[]={
    0xb2bc85d0u,0xb2bd1568u,0xb2bd1d74u,0xb2bd2698u,0xb2bd4894u
};
static struct z72_flexible_action cloud_live_action;
static struct z72_flexible_workers cloud_workers;
struct cloud_style_storage {
    struct z72_style_state states[2];
    struct z72_style_transaction transaction;
};
static struct cloud_style_storage *cloud_style;
/* Controllers are locked before this mutex. Publication never locks them. */
static u32 cloud_style_mutex[2];
/* Zero is unowned. Values 1..3 encode the previous native value plus one. */
enum { CLOUD_POLICY_CHANGING=4u };
static u32 cloud_capture_policy_owner;
static u32 cloud_style_enabled;

/* Shared video callbacks can give the native router back without waiting
 * for a photo publisher that still holds the style mutex. */
static void cloud_release_capture_policy(void)
{
    u32 owner=__atomic_load_n(&cloud_capture_policy_owner,__ATOMIC_ACQUIRE);
    if(!owner || owner==CLOUD_POLICY_CHANGING ||
       !__atomic_compare_exchange_n(&cloud_capture_policy_owner,&owner,CLOUD_POLICY_CHANGING,0,
                                    __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))
        return;
    unsigned char expected=1;
    __atomic_compare_exchange_n(&CLOUD_CAPTURE_POLICY,&expected,(unsigned char)(owner-1u),0,
                                __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    __atomic_store_n(&cloud_capture_policy_owner,0,__ATOMIC_RELEASE);
}

static u32 cloud_pipeline_photo(void)
{
    if(cloud_photo_mode()) return 1;
    cloud_release_capture_policy();
    return 0;
}

/* Keep the original local photo path so presets reach every still image. */
static u32 cloud_set_capture_policy(u32 enabled)
{
    if(!enabled || !cloud_pipeline_photo()) {
        cloud_release_capture_policy();
        return enabled;
    }
    u32 owner=__atomic_load_n(&cloud_capture_policy_owner,__ATOMIC_ACQUIRE);
    if(owner)
        return owner==CLOUD_POLICY_CHANGING || CLOUD_CAPTURE_POLICY!=1;
    if(!__atomic_compare_exchange_n(&cloud_capture_policy_owner,&owner,CLOUD_POLICY_CHANGING,0,
                                    __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))
        return 1;
    unsigned char current=CLOUD_CAPTURE_POLICY;
    if(current>2 || !cloud_photo_mode() ||
       !__atomic_compare_exchange_n(&CLOUD_CAPTURE_POLICY,&current,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        __atomic_store_n(&cloud_capture_policy_owner,0,__ATOMIC_RELEASE);
        return 1;
    }
    __atomic_store_n(&cloud_capture_policy_owner,(u32)current+1u,__ATOMIC_RELEASE);
    return !cloud_pipeline_photo();
}

static u32 cloud_capture_action(void *context,u32 a,u32 b,u32 c)
{
    u32 cell=(u32)context;
    if(cloud_pipeline_photo() && z72_style_ram(cell,0x78)) {
        u32 root=WORD(cell+12);
        if(z72_style_ram(root,0x274) && !WORD(cell+4) &&
           !*(const unsigned char *)(cell+0x74) &&
           (*(const unsigned char *)(root+13)!=0x12 || WORD(root+0x270)))
            cloud_before_capture();
    }
    return FN(CLOUD_CAPTURE_ACTION,u32,void *,u32,u32,u32)(context,a,b,c);
}

static u32 cloud_publish(u32 handle,const u32 *root,u32 a,u32 b)
{
    if(!cloud_pipeline_photo())
        return FN(0x102e49ffu,u32,u32,const u32 *,u32,u32)(handle,root,a,b);
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    if(!cloud_pipeline_photo()) {
        FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
        return FN(0x102e49ffu,u32,u32,const u32 *,u32,u32)(handle,root,a,b);
    }
    u32 target=2;
    if((handle==z72_flexible_publish_target(0) || handle==z72_flexible_publish_target(1)) &&
       z72_style_ram(handle,8))
        target=*(const unsigned char *)(handle+4);

    int prepared=-1;
    if(target<2) {
        cloud_refresh_selection();
        /* The shared secondary branch only uploads unrelated native data. */
        if(!(target==1 && z72_style_ram((u32)root,CLOUD_ROOT_WORDS*4) && root[51]))
            prepared=z72_style_prepare(&cloud_style->states[target],&cloud_style->transaction,
                root,cloud_active_style(),cloud_style_enabled,cloud_active_chroma());
    }
    const u32 *forward=prepared==1 ? cloud_style->transaction.root : root;
    u32 result=FN(0x102e49ffu,u32,u32,const u32 *,u32,u32)(handle,forward,a,b);
    if(!result && prepared>=0) {
        z72_style_commit(&cloud_style->states[target],&cloud_style->transaction);
        if(!target) cloud_live_published(root,&cloud_style->transaction);
    } else if(!result && !target) {
        cloud_live_published(root,0);
    }
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
    return result;
}

static u32 cloud_primary_worker(void *message,u32 a,u32 b,u32 c)
{
    u32 entry=cloud_pipeline_photo() ? z72_flexible_workers_entry(&cloud_workers,0) : CLOUD_WORKER_STOCK0;
    return FN(entry,u32,void *,u32,u32,u32)(message,a,b,c);
}

static u32 cloud_secondary_worker(void *message,u32 a,u32 b,u32 c)
{
    u32 entry=cloud_pipeline_photo() ? z72_flexible_workers_entry(&cloud_workers,1) : CLOUD_WORKER_STOCK1;
    return FN(entry,u32,void *,u32,u32,u32)(message,a,b,c);
}

/* Preserve the original running update's controller and publication ordering. */
static u32 cloud_running_update(u32 bank,const void *request)
{
    if(!cloud_pipeline_photo() || bank>1)
        return FN(0x102c04c7u,u32,u32,const void *)(bank,request);
    cloud_note_live_update();
    u32 controller=0xb238d458u+bank*0x8e8u,result=0;
    FN(0x103d5919u,void,u32)(controller+8);
    if(WORD(controller+4)) {
        u32 root=controller+0x2c,backing=controller+0x100;
        FN(0x102c0b69u,void,const void *,u32,u32)(request,root,backing);
        FN(0x1034fa5du,void,u32,u32)(backing,0x7e8);
        if(!bank) cloud_publish(z72_flexible_publish_target(0),(const u32 *)root,0,0);
        if(bank || WORD(0xb238d550u))
            cloud_publish(z72_flexible_publish_target(1),(const u32 *)root,0,0);
        result=1;
    }
    FN(0x103d58fbu,void,u32)(controller+8);
    return result;
}

static u32 cloud_online_action(void *context)
{
    u32 entry=cloud_pipeline_photo() ? z72_flexible_action_entry(&cloud_live_action)
                                     : Z72_FLEXIBLE_ACTION_STOCK;
    return FN(entry,u32,void *)(context);
}

static u32 cloud_pipeline_prepare(void)
{
    if(WORD(CLOUD_WORKER_SLOT0)!=CLOUD_WORKER_STOCK0 || WORD(CLOUD_WORKER_SLOT1)!=CLOUD_WORKER_STOCK1 ||
       CLOUD_CAPTURE_POLICY>2 || z72_np3_hash((const void *)0x103d1e5cu,58)!=0x3099b367u)
        return 1;
    for(u32 i=0;i<sizeof(cloud_capture_slots)/sizeof(cloud_capture_slots[0]);++i)
        if(WORD(cloud_capture_slots[i])!=CLOUD_CAPTURE_ACTION) return 1;
    if(z72_flexible_action_prepare(&cloud_live_action,(u32)cloud_running_update) ||
       z72_flexible_workers_prepare(&cloud_workers,(u32)cloud_publish)) return 1;
    FN(0x103d58d1u,void,u32 *)(cloud_style_mutex);
    return !cloud_style_mutex[0] || (int)cloud_style_mutex[0]<0 || cloud_style_mutex[1]!=0xffffffffu;
}

static void cloud_pipeline_install(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(CLOUD_WORKER_SLOT0)=(u32)cloud_primary_worker;
    WORD(CLOUD_WORKER_SLOT1)=(u32)cloud_secondary_worker;
    for(u32 i=0;i<Z72_FLEXIBLE_ACTION_SLOT_COUNT;++i)
        WORD(z72_flexible_action_slots[i])=(u32)cloud_online_action;
    for(u32 i=0;i<sizeof(cloud_capture_slots)/sizeof(cloud_capture_slots[0]);++i)
        WORD(cloud_capture_slots[i])=(u32)cloud_capture_action;
}
#endif
