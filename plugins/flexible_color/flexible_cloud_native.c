/* SPDX-License-Identifier: MIT */
/* Independent SD Picture Controls in the native photo carousel. */
#include "z72.h"
#define Z72_STYLE_LIMIT_PAIRED_DELTA
#include "flexible_cloud.h"
#include "flexible_live_color.h"
#include "np3_calibration.h"

#define CLOUD_PC_RED 0x080fu
#define CLOUD_PC_AUTO 0x02cfu
#define CLOUD_PC_PHOTO_DESCRIPTOR 0xb29a33d0u

#define FN(a, result, ...) ((result (*)(__VA_ARGS__))(a))

static void cloud_refresh_selection(void);
static u32 cloud_i_wheel_active(void);
static void cloud_before_capture(void);
static void cloud_note_live_update(void);
static void cloud_live_published(const u32 *root,const struct z72_style_transaction *transaction);
static const struct z72_style_fixed *cloud_active_style(void);
static enum z72_style_chroma_mode cloud_active_chroma(void);

#include "flexible_color_pipeline.h"
#include "np3_sd_scan.h"

struct cloud_pc_record {
    char name[20];
    u32 named;
    unsigned char parameters[0x38];
    unsigned short base, reserved;
};
_Static_assert(sizeof(struct cloud_pc_record)==0x54,"C 1.70 Picture Control record");

#define CLOUD_BOOT_INDEX (CLOUD_CANDIDATES+1u)
struct cloud_catalog_storage {
    struct {
        struct cloud_pc_record native;
        struct z72_np3_flexible profile;
        struct z72_style_fixed style;
        u32 file_bytes, file_hash;
        enum z72_style_chroma_mode chroma;
    } profiles[CLOUD_BOOT_INDEX];
    unsigned char style_prepared[CLOUD_BOOT_INDEX];
    char profile_filenames[CLOUD_CANDIDATES][CLOUD_FILENAME];
    char filenames[CLOUD_CANDIDATES][CLOUD_FILENAME];
    struct z72_np3_flexible scan_profile;
};
static struct cloud_catalog_storage *cloud_catalog;
static u32 cloud_picker_installed;
static unsigned char *cloud_file_data;
#define CLOUD_NP3_CAPACITY 4096u
static union z72_np3_calibration_block *cloud_calibration;
static u32 cloud_calibration_count;

static void *cloud_allocate_storage(void)
{
    /* Use the same native heap and 64-byte alignment as the SD loader. */
    struct cloud_storage {
        _Alignas(64) unsigned char file_data[CLOUD_NP3_CAPACITY];
        union z72_np3_calibration_block calibration;
        struct cloud_style_storage style;
        struct cloud_catalog_storage catalog;
    };
    void *owner=FN(0x1034fe31u,void *,u32,u32)(sizeof(struct cloud_storage)+63u,0);
    if(!owner) return 0;
    struct cloud_storage *storage=(void *)(((u32)owner+63u)&~63u);
    /* Entries are filled by the scanner. Only the cache flags and table
     * state need initialization before the first native callback. */
    for(u32 i=0;i<CLOUD_BOOT_INDEX;++i) storage->catalog.style_prepared[i]=0;
    unsigned char *states=(void *)storage->style.states;
    for(u32 i=0;i<sizeof(storage->style.states);++i) states[i]=0;
    cloud_file_data=storage->file_data;
    cloud_calibration=&storage->calibration;
    cloud_style=&storage->style;
    cloud_catalog=&storage->catalog;
    cloud_filenames=storage->catalog.filenames;
    return owner;
}
static u32 cloud_profile_count;
static u32 cloud_catalog_pending,cloud_fast_boot;

static u32 cloud_active;
static u32 cloud_rendered;
static u32 cloud_rendered_stock_id;
static u32 cloud_live_applied;
static u32 cloud_live_ready;
static struct z72_live_color cloud_color_action;
static u32 cloud_boot_profile,cloud_boot_requests;
static u32 cloud_boot_first_generated,cloud_boot_updates;

static const char cloud_selection_path[]=CLOUD_PRESET_DIRECTORY "Z72BSEL.CFG";
static const char cloud_resume_path[]=CLOUD_PRESET_DIRECTORY "Z72BSEL.IDX";
static const char cloud_calibration_path[]=CLOUD_PRESET_DIRECTORY "Z72NPCAL.BIN";
#define CLOUD_SELECTION_MAGIC 0x3142535au /* ZSB1 */
#define CLOUD_RESUME_MAGIC 0x3149535au /* ZSI1 */
struct cloud_resume {
    u32 magic,bytes,hash,slot;
    char filename[CLOUD_FILENAME];
    unsigned short reserved;
    u32 checksum;
};
_Static_assert(sizeof(struct cloud_resume)==132,"selection index layout");

enum { CLOUD_RESTORE_DONE, CLOUD_RESTORE_NEW, CLOUD_RESTORE_WAITING };
static u32 cloud_saved_bytes,cloud_saved_hash,cloud_selection_needs_write;
static u32 cloud_resume_slot;
static char cloud_resume_filename[CLOUD_FILENAME];
static u32 cloud_confirmed,cloud_restore_pending;
static u32 cloud_restore_last_attempt,cloud_restore_refreshes;

static u32 cloud_read_sd(const char *path,void *data,u32 capacity,u32 expected)
{
    u32 file[10],actual=0,status=1;
    for(u32 i=0;i<10;++i) ((volatile u32 *)file)[i]=0;
    FN(0x104d3c01u,void,u32 *,const char *)(file,path);
    u32 opened=FN(0x104d3c05u,u32,u32 *,u32,u32)(file,0,0);
    if(opened!=CLOUD_FILE_OK)
        return 0;
    u32 bytes=file[6];
    if(bytes && bytes<capacity && (!expected || expected==bytes) && !file[7]) {
        struct {u32 kind; void *buffer; u32 capacity;} request={1,data,bytes+1u};
        FN(0x1034fa29u,void,void *,u32)(data,capacity);
        status=FN(0x104d3d83u,u32,u32 *,void *,u32,u32 *)(file,&request,bytes+1u,&actual);
        FN(0x1034fa29u,void,void *,u32)(data,capacity);
    }
    u32 closed=FN(0x104d3c91u,u32,u32 *)(file);
    return status==CLOUD_FILE_OK && closed==CLOUD_FILE_OK && actual==bytes ? bytes : 0;
}

static u32 cloud_read_selection(void)
{
    _Alignas(64) u32 data[16];
    if(!cloud_read_sd(cloud_selection_path,data,sizeof(data),16))
        return 0;
    if(data[0]!=CLOUD_SELECTION_MAGIC ||
       data[3]!=~(data[1]^data[2]) || (!data[1] && data[2]))
        return 0;
    cloud_saved_bytes=data[1];
    cloud_saved_hash=data[2];
    return 1;
}

static u32 cloud_write_sd(const char *path,void *data,u32 bytes)
{
    struct {u32 kind; void *buffer; u32 capacity;} request={1,data,bytes};
    int file=FN(0x105beb2bu,int,const char *,u32)(path,0);
    if(file<0)
        return 0;
    u32 actual=0;
    FN(0x1034f9edu,void,void *,u32)(data,bytes);
    u32 status=FN(0x105bea79u,u32,int,void *,u32,u32 *)(file,&request,bytes,&actual);
    u32 closed=FN(0x105be9ddu,u32,int)(file);
    return !status && !closed && actual==bytes;
}

static void cloud_write_selection(u32 bytes,u32 hash)
{
    _Alignas(64) u32 data[4]={CLOUD_SELECTION_MAGIC,bytes,hash,~(bytes^hash)};
    if(cloud_write_sd(cloud_selection_path,data,sizeof(data))) {
        cloud_saved_bytes=bytes;
        cloud_saved_hash=hash;
        cloud_resume_slot=0;
        cloud_selection_needs_write=0;
    }
}

static u32 cloud_same_filename(const char *left,const char *right)
{
    while(*left && *left==*right) {
        ++left;
        ++right;
    }
    return *left==*right;
}

static u32 cloud_read_resume(void)
{
    _Alignas(64) union {struct cloud_resume record; unsigned char block[192];} data;
    if(!cloud_read_sd(cloud_resume_path,data.block,sizeof(data.block),sizeof(data.record)))
        return 0;
    const struct cloud_resume *record=&data.record;
    if(record->magic!=CLOUD_RESUME_MAGIC || record->bytes!=cloud_saved_bytes ||
       record->hash!=cloud_saved_hash || !record->slot || record->slot>CLOUD_CANDIDATES ||
       record->reserved || z72_np3_hash(record,sizeof(*record)-4u)!=record->checksum)
        return 0;
    u32 i=0;
    for(;i<CLOUD_FILENAME && record->filename[i];++i) {
        unsigned char ch=record->filename[i];
        if(ch<' ' || ch>'~' || ch=='/' || ch=='\\' || ch==':')
            return 0;
    }
    if(i<=4 || i==CLOUD_FILENAME || record->filename[i-4]!='.' ||
       (record->filename[i-3]|32)!='n' || (record->filename[i-2]|32)!='p' ||
       record->filename[i-1]!='3')
        return 0;
    cloud_resume_slot=record->slot;
    z72_style_copy(cloud_resume_filename,record->filename,sizeof(cloud_resume_filename));
    return 1;
}

static void cloud_write_resume(u32 index)
{
    const char *filename=cloud_catalog->profile_filenames[index-1u];
    if(index==cloud_resume_slot && cloud_same_filename(filename,cloud_resume_filename))
        return;
    _Alignas(64) struct cloud_resume record;
    record.magic=CLOUD_RESUME_MAGIC;
    record.bytes=cloud_saved_bytes;
    record.hash=cloud_saved_hash;
    record.slot=index;
    z72_style_copy(record.filename,filename,sizeof(record.filename));
    record.reserved=0;
    record.checksum=z72_np3_hash(&record,sizeof(record)-4u);
    if(cloud_write_sd(cloud_resume_path,&record,sizeof(record))) {
        cloud_resume_slot=index;
        z72_style_copy(cloud_resume_filename,filename,sizeof(cloud_resume_filename));
    }
}

static void cloud_save_selection(u32 index)
{
    u32 bytes=index ? cloud_catalog->profiles[index-1u].file_bytes : 0;
    u32 hash=index ? cloud_catalog->profiles[index-1u].file_hash : 0;
    if(cloud_selection_needs_write || bytes!=cloud_saved_bytes || hash!=cloud_saved_hash)
        cloud_write_selection(bytes,hash);
    if(index && index<=CLOUD_CANDIDATES && !cloud_selection_needs_write &&
       bytes==cloud_saved_bytes && hash==cloud_saved_hash)
        cloud_write_resume(index);
}

static u32 cloud_prepare_style(u32 index)
{
    u32 slot=index-1u;
    if(cloud_catalog->profiles[slot].profile.base!=Z72_NP3_FLEXIBLE_BASE ||
       __atomic_load_n(&cloud_catalog->style_prepared[slot],__ATOMIC_ACQUIRE))
        return 0;
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    u32 failed=0;
    if(!cloud_catalog->style_prepared[slot]) {
        failed=z72_cloud_prepare(&cloud_catalog->profiles[slot].profile,&cloud_catalog->profiles[slot].style);
        if(!failed) {
            const struct z72_np3_calibration *record=z72_cal_find(&cloud_calibration->file,
                cloud_calibration_count,cloud_catalog->profiles[slot].file_bytes,
                cloud_catalog->profiles[slot].file_hash);
            if(record)
                z72_cal_apply(&cloud_catalog->profiles[slot].style,record);
            __atomic_store_n(&cloud_catalog->style_prepared[slot],1,__ATOMIC_RELEASE);
        }
    }
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
    return failed;
}

static u32 cloud_read_np3(const char *path,struct z72_np3_flexible *profile,u32 *size)
{
    u32 file[10],actual=0,result=0;
    for(u32 i=0;i<10;++i) ((volatile u32 *)file)[i]=0;
    FN(0x104d3c01u,void,u32 *,const char *)(file,path);
    if(FN(0x104d3c05u,u32,u32 *,u32,u32)(file,0,0)!=CLOUD_FILE_OK)
        return Z72_NP3_MISSING;
    *size=file[6];
    if(!file[7] && *size>=8 && *size<CLOUD_NP3_CAPACITY) {
        /* Complete cache lines covering the transfer, including the EOF probe. */
        u32 capacity=(*size+64u)&~63u;
        struct {u32 kind; void *buffer; u32 capacity;} request={1,cloud_file_data,CLOUD_NP3_CAPACITY};
        FN(0x1034fa29u,void,void *,u32)(cloud_file_data,capacity);
        result=FN(0x104d3d83u,u32,u32 *,void *,u32,u32 *)(file,&request,*size+1u,&actual);
        FN(0x1034fa29u,void,void *,u32)(cloud_file_data,capacity);
    }
    u32 closed=FN(0x104d3c91u,u32,u32 *)(file);
    if(result!=CLOUD_FILE_OK || closed!=CLOUD_FILE_OK || actual!=*size)
        return Z72_NP3_MISSING;
    return z72_np3_parse(cloud_file_data,*size,profile,CLOUD_PC_RED);
}

static u32 cloud_load_entry(const char *path,u32 index)
{
    struct z72_np3_flexible *parsed=&cloud_catalog->scan_profile;
    u32 bytes;
    if(cloud_read_np3(path,parsed,&bytes)!=Z72_NP3_OK || !cloud_english_name(parsed->name))
        return 0;
    u32 file_hash=z72_np3_hash(cloud_file_data,bytes);
    if(index==CLOUD_BOOT_INDEX-1u && (bytes!=cloud_saved_bytes || file_hash!=cloud_saved_hash))
        return 0;
    struct cloud_pc_record *native=&cloud_catalog->profiles[index].native;
    if(parsed->base==CLOUD_PC_RED) {
        /* Camera-compatible Creative Red has its own native parameters.
         * The first 0x54 bytes are the native runtime descriptor input. */
        u32 imported[0x2a4/sizeof(u32)];
        if(parsed->has_curve || FN(0x10360f19u,u32,u32)(0xb188b2e4u)!=1)
            return 0;
        FN(0x10361035u,void,void *,u32)(imported,sizeof(imported));
        u32 ok=FN(0x103d7eebu,u32,const char *,void *)(path,imported);
        FN(0x10360f13u,void,u32)(0xb188b2e4u);
        if(ok!=1 || (imported[20]&0xffffu)!=CLOUD_PC_RED)
            return 0;
        const char *name=(const char *)imported;
        for(u32 i=0;i<sizeof(native->name);++i)
            if(name[i]!=parsed->name[i])
                return 0;
        z72_style_copy(native,imported,sizeof(*native));
    } else {
        if(!z72_cloud_profile_supported(parsed))
            return 0;
        if(FN(0x103d72d1u,u32,u32,struct cloud_pc_record *)(0,native)!=1 ||
           native->base!=CLOUD_PC_AUTO)
            return 0;
    }
    z72_style_copy(native->name,parsed->name,sizeof(native->name));
    native->named=1;
    z72_style_copy(&cloud_catalog->profiles[index].profile,parsed,sizeof(*parsed));
    cloud_catalog->profiles[index].file_bytes=bytes;
    cloud_catalog->profiles[index].file_hash=file_hash;
    cloud_catalog->profiles[index].chroma=z72_cloud_chroma_mode(parsed);
    return 1;
}

static u32 cloud_load_candidate(const char *path)
{
    u32 index=cloud_profile_count;
    if(!cloud_load_entry(path,index))
        return 0;
    z72_style_copy(cloud_catalog->profile_filenames[index],path+sizeof(cloud_directory)-1u,CLOUD_FILENAME);
    ++cloud_profile_count;
    return 1;
}

static u32 cloud_catalog_match(void)
{
    u32 first=0;
    for(u32 i=0;i<cloud_profile_count;++i)
        if(cloud_catalog->profiles[i].file_bytes==cloud_saved_bytes && cloud_catalog->profiles[i].file_hash==cloud_saved_hash) {
            if(cloud_same_filename(cloud_catalog->profile_filenames[i],cloud_resume_filename))
                return i+1u;
            if(!first)
                first=i+1u;
        }
    return first;
}

static void cloud_scan_profiles(void)
{
    u32 (*directory_exists)(const char *)=FN(0x104d3ef1u,u32,const char *);
    u32 (*make_directory)(const char *,u32)=FN(0x104d3ea5u,u32,const char *,u32);
    if(!directory_exists(cloud_directory) && make_directory(cloud_directory,0x10u)!=CLOUD_FILE_OK)
        return;
    u32 bytes=cloud_read_sd(cloud_calibration_path,cloud_calibration->data,sizeof(cloud_calibration->data),0);
    cloud_calibration_count=z72_cal_count(cloud_calibration,bytes);
    cloud_selection_needs_write=!cloud_read_selection();
    if(cloud_saved_bytes && cloud_read_resume()) {
        char path[sizeof(cloud_directory)+CLOUD_FILENAME-1u];
        z72_style_copy(path,cloud_directory,sizeof(cloud_directory)-1u);
        z72_style_copy(path+sizeof(cloud_directory)-1u,cloud_resume_filename,CLOUD_FILENAME);
        if(cloud_load_entry(path,CLOUD_BOOT_INDEX-1u) && !cloud_prepare_style(CLOUD_BOOT_INDEX)) {
            cloud_confirmed=CLOUD_BOOT_INDEX;
            cloud_catalog_pending=cloud_fast_boot=1;
            return;
        }
    }
    cloud_scan(CLOUD_CANDIDATES,cloud_load_candidate);
    u32 index=cloud_catalog_match();
    if(index && !cloud_prepare_style(index)) {
        cloud_confirmed=index;
        cloud_save_selection(index);
    }
}

static u32 cloud_photo_mode(void)
{
    u32 mode=*(volatile unsigned char *)0xb2018820u;
    return mode<5u || mode>7u;
}

static u32 cloud_requested_profile(void)
{
    if(!cloud_photo_mode())
        return 0;
    u32 id=__atomic_load_n(&cloud_rendered,__ATOMIC_ACQUIRE);
    if(!id || *(volatile unsigned short *)0xb188b2dau!=
              __atomic_load_n(&cloud_rendered_stock_id,__ATOMIC_RELAXED))
        return 0;
    return id;
}

static u32 cloud_selected_profile(void)
{
    u32 id=cloud_requested_profile();
    if(!id)
        return 0;
    const struct cloud_pc_record *owned=&cloud_catalog->profiles[id-1u].native;
    const volatile struct cloud_pc_record *current=(const void *)CLOUD_PC_PHOTO_DESCRIPTOR;
    if(current->base!=owned->base)
        return 0;
    for(u32 i=0;i<sizeof(owned->name);++i)
        if(current->name[i]!=owned->name[i])
            return 0;
    return id;
}

/* Called with cloud_style_mutex held. Native Red goes through stock table generation
 * and takes the same software bypass as an ordinary Picture Control. */
static void cloud_refresh_selection(void)
{
    u32 selected=cloud_selected_profile();
    u32 enabled=selected && cloud_catalog->profiles[selected-1u].profile.base==Z72_NP3_FLEXIBLE_BASE;
    if(cloud_set_capture_policy(enabled)) {
        selected=0;
        enabled=0;
    }
    cloud_active=selected;
    cloud_style_enabled=enabled;
}

static void cloud_before_capture(void)
{
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    cloud_refresh_selection();
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
}

static const struct z72_style_fixed *cloud_active_style(void)
{
    return cloud_style_enabled ? &cloud_catalog->profiles[cloud_active-1u].style : 0;
}

static enum z72_style_chroma_mode cloud_active_chroma(void)
{
    return cloud_style_enabled ? cloud_catalog->profiles[cloud_active-1u].chroma : Z72_STYLE_CHROMA_NORMAL;
}

/* C 1.70 photo carousel. B pages reuse the five stock card widgets. The
 * profile index stays in the module and is never passed to a native setter. */
#define CLOUD_PC_APP 0xb1823b30u
#define CLOUD_PC_STATE 0xb1823aecu
#define CLOUD_PC_PENDING 0xb20316d4u
#define CLOUD_PC_OPEN 0x10590563u
#define CLOUD_PC_CLOSE 0x1059079du
#define CLOUD_PC_KEY 0x105915bbu
#define CLOUD_PC_EVENT 0x10591763u
#define CLOUD_PC_ARROW 0x1058fa1fu
#define CLOUD_PC_DRAG 0x10590279u
#define CLOUD_PC_DRAG_SLOT 0xb181c544u
#define CLOUD_LIVE_IDLE_SLOT 0xb1821b5cu
#define CLOUD_LIVE_IDLE_OTHER_SLOT 0xb1821b80u
#define CLOUD_LIVE_IDLE 0x104e7177u
#define CLOUD_LIVE_CONTROLLER 0xb238d458u
#define CLOUD_PC_BASE_VALUE 0xb2551958u
#define CLOUD_PC_EFFECTIVE_BASE 0xb2551b9cu
#define CLOUD_RESTORE_INTERVAL 250u
#define CLOUD_RESTORE_REFRESH_LIMIT 4u
#define CLOUD_I_APP 0xb1821a08u
#define CLOUD_I_STATE 0xb1821964u
#define CLOUD_I_OPEN 0x105c13cdu
#define CLOUD_I_CLOSE 0x105c1517u
#define CLOUD_I_INPUT 0x105c1639u
#define CLOUD_I_UPDATE 0x105c0a19u
/* FTP keyboard uses slots 41 through 87. The shared Setup labels start at 255. */
#define CLOUD_I_TEXT 88u
#define CLOUD_PAGE_TEXT 89u
#define CLOUD_I_WHEEL_SCENE 0xb202f644u
#define CLOUD_I_WHEEL_SCENE_SLOT 0xb1823aacu
#define CLOUD_I_WHEEL_MODEL 0xb202ee24u
#define CLOUD_I_WHEEL_ICONS 0xb20983a0u
#define CLOUD_I_WHEEL_SLOTS 0xb2031518u
#define CLOUD_I_WHEEL_STOCK_BASES 28u
#define CLOUD_PC_CARDS 5u
#define CLOUD_PC_NATIVE_ROWS 37u
#define CLOUD_PAGE_WIDGET 0x8050u

#define CLOUD_BYTE(address) (*(volatile unsigned char *)(address))
#define CLOUD_HALF(address) (*(volatile unsigned short *)(address))

static u32 cloud_browse,cloud_page_first,cloud_picker_active;
static unsigned short cloud_native_start[2],cloud_native_focus[2];

static u32 cloud_photo_picker(void)
{
    return CLOUD_BYTE(CLOUD_PC_STATE+5)==1 && !CLOUD_BYTE(CLOUD_PC_STATE+6) &&
           CLOUD_BYTE(CLOUD_PC_STATE+7)!=2;
}

static u32 cloud_view(void)
{
    return FN(0x10591911u,u32,void)();
}

static u32 cloud_widget(u32 view,u32 id)
{
    return FN(0x10635cefu,u32,u32,u32)(id,WORD(view+0xc));
}

static u32 cloud_card(u32 view,u32 index)
{
    return FN(0x10634623u,u32,u32,u32,u32)(WORD(view+0x24),1,index);
}

static u32 cloud_view_ready(u32 view)
{
    if(!view || !WORD(view+0xc) || !WORD(view+0x24))
        return 0;
    u32 layout=WORD(view+0x28);
    if(!layout || CLOUD_HALF(layout+0x14)!=CLOUD_PC_NATIVE_ROWS || CLOUD_HALF(layout+0x16)!=CLOUD_PC_CARDS)
        return 0;
    for(u32 i=0;i<CLOUD_PC_CARDS;++i) {
        u32 card=cloud_card(view,i);
        if(!card || CLOUD_HALF(card)!=0x8000u+i || WORD(card+8)!=0x8cu+i)
            return 0;
    }
    return 1;
}

static u32 cloud_b_edge(u32 view,int direction,u32 page_step)
{
    if(!view || !cloud_profile_count)
        return 0;
    u32 layout=WORD(view+0x28);
    if(!layout || CLOUD_HALF(layout+0x14)!=CLOUD_PC_NATIVE_ROWS)
        return 0;
    u32 first=CLOUD_HALF(view+0x18);
    u32 focus=first+CLOUD_HALF(view+0x16);
    if(focus>=CLOUD_PC_NATIVE_ROWS)
        return 0;
    if(direction<0)
        return (page_step ? first : focus)==0 ? cloud_profile_count : 0;
    u32 next=page_step ? first : focus+1u;
    u32 visible=page_step ? CLOUD_PC_CARDS : 0;
    for(u32 i=next;i<CLOUD_PC_NATIVE_ROWS;++i) {
        u32 card=cloud_card(view,i);
        if(!card || !WORD(card+0x28))
            continue;
        if(!visible)
            return 0;
        --visible;
    }
    return 1;
}

static u32 cloud_native_base(u32 index)
{
    return cloud_catalog->profiles[index-1u].native.base==CLOUD_PC_RED ? 22u : 0u;
}

/* Wait for four running updates after the first rebuild before retrying. */
static void cloud_note_live_update(void)
{
    if(__atomic_load_n(&cloud_boot_first_generated,__ATOMIC_ACQUIRE) &&
       __atomic_load_n(&cloud_boot_updates,__ATOMIC_RELAXED)<4u)
        __atomic_fetch_add(&cloud_boot_updates,1,__ATOMIC_RELAXED);
}

static u32 cloud_generate_live_color(const u32 *source,const u32 *output)
{
    u32 index=__atomic_load_n(&cloud_boot_profile,__ATOMIC_ACQUIRE);
    u32 requests=__atomic_load_n(&cloud_boot_requests,__ATOMIC_RELAXED);
    const unsigned char *flags=(const unsigned char *)source+0x14;
    if(!index || requests>=2 || flags[0] || (flags[1]!=0 && flags[1]!=3) ||
       (requests && (!__atomic_load_n(&cloud_boot_first_generated,__ATOMIC_ACQUIRE) ||
        __atomic_load_n(&cloud_boot_updates,__ATOMIC_RELAXED)<4u)) ||
       WORD(CLOUD_PC_BASE_VALUE) || WORD(CLOUD_PC_EFFECTIVE_BASE) ||
       __atomic_load_n(&cloud_live_ready,__ATOMIC_ACQUIRE)==index ||
       cloud_selected_profile()!=index ||
       !__atomic_compare_exchange_n(&cloud_boot_requests,&requests,requests+1,0,
                                    __ATOMIC_RELAXED,__ATOMIC_RELAXED))
        return FN(Z72_LIVE_COLOR_GENERATE,u32,const u32 *,const u32 *)(source,output);
    /* The native caller owns this 24-byte record on its stack. Its +0x14
     * reaches the generators as the full-rebuild flag. Mode 3 skips CPXS
     * even with that flag set. Use full mode for this one local request. */
    u32 input[6];
    z72_style_copy(input,source,sizeof(input));
    ((unsigned char *)input)[0x14]=1;
    ((unsigned char *)input)[0x15]=0;
    u32 result=FN(Z72_LIVE_COLOR_GENERATE,u32,const u32 *,const u32 *)(input,output);
    if(!requests)
        __atomic_store_n(&cloud_boot_first_generated,1,__ATOMIC_RELEASE);
    return result;
}

static u32 cloud_live_color_action(void *context)
{
    return FN((u32)cloud_color_action.code+3u,u32,void *)(context);
}

static void cloud_live_published(const u32 *root,const struct z72_style_transaction *transaction)
{
    u32 applied=transaction && transaction->applied;
    __atomic_store_n(&cloud_live_applied,applied ? cloud_active : 0,__ATOMIC_RELEASE);
    if(!cloud_active)
        return;
    u32 base=cloud_native_base(cloud_active);
    /* Native Red needs its own color data. Rejected transactions cannot
     * acknowledge a preset even if the native uploader succeeds. */
    if(WORD(CLOUD_PC_EFFECTIVE_BASE)==base &&
       (base ? (transaction && !applied && !transaction->deferred && root[25] && root[26]) : applied))
        __atomic_store_n(&cloud_live_ready,cloud_active,__ATOMIC_RELEASE);
}

static void cloud_publish_live(u32 previous,u32 index)
{
    if(!previous || previous==index || !index || cloud_native_base(previous) ||
       cloud_native_base(index) || !WORD(CLOUD_LIVE_CONTROLLER+4))
        return;
    /* The previous software preset already used Auto on this live target.
     * Recompose only RGB and CPXS from its cached native originals. */
    FN(0x103d5919u,void,u32)(CLOUD_LIVE_CONTROLLER+8);
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    u32 handle=z72_flexible_publish_target(0);
    if(WORD(CLOUD_LIVE_CONTROLLER+4) && z72_style_ram(handle,8) &&
       !CLOUD_BYTE(handle+4) && !WORD(CLOUD_PC_EFFECTIVE_BASE) &&
       cloud_live_applied==previous && cloud_active==index &&
       cloud_selected_profile()==index && cloud_style_enabled) {
        u32 root[CLOUD_ROOT_WORDS];
        FN(0x10361035u,void,void *,u32)(root,sizeof(root));
        if(z72_style_prepare(&cloud_style->states[0],&cloud_style->transaction,root,cloud_active_style(),
                              1,cloud_active_chroma())==1 && cloud_style->transaction.applied) {
            FN(0x102e56c9u,void,u32,const u32 *)(handle,cloud_style->transaction.rgb_pointers);
            for(u32 i=0;i<12;++i)
                FN(0x1036bf89u,void,u32,u32,u32)(handle,i,cloud_style->transaction.cpxs_pointers[i]);
            /* Stock live action finishes with this target's commit. Do not
             * send a new frame, touch the capture target, or change routing. */
            FN(0x102e5489u,void,void)();
            z72_style_commit(&cloud_style->states[0],&cloud_style->transaction);
            __atomic_store_n(&cloud_live_applied,index,__ATOMIC_RELEASE);
            __atomic_store_n(&cloud_live_ready,index,__ATOMIC_RELEASE);
        }
    }
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
    FN(0x103d58fbu,void,u32)(CLOUD_LIVE_CONTROLLER+8);
}

static u32 cloud_set_preview_descriptor(u32 index,u32 base,u32 stock_id)
{
    const struct cloud_pc_record *native=&cloud_catalog->profiles[index-1u].native;
    /* Bank 0 is photo. The native builder leaves the active ID and C slots
     * alone and serializes the descriptor update with its own mutex. */
    if(FN(0x103d6799u,u32,u32,const struct cloud_pc_record *,u32)(base,native,0)!=1)
        return 0;
    __atomic_store_n(&cloud_rendered_stock_id,stock_id,__ATOMIC_RELAXED);
    __atomic_store_n(&cloud_rendered,index,__ATOMIC_RELEASE);
    return 1;
}

static void cloud_preview(u32 index)
{
    if(index && cloud_prepare_style(index))
        index=0;
    u32 previous=cloud_selected_profile();
    u32 wheel=cloud_i_wheel_active();
    if(!wheel)
        FN(0x106b91a1u,void,u32)(0x5f);
    __atomic_store_n(&cloud_rendered,0,__ATOMIC_RELEASE);
    u32 surrogate=0;
    if(index) {
        surrogate=cloud_native_base(index);
        if(!cloud_set_preview_descriptor(index,surrogate,CLOUD_HALF(0xb188b2dau)))
            index=0;
    }
    if(!index)
        FN(0x1063cc75u,u32,u32)(FN(0x1063cc51u,u32,void)());
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    __atomic_store_n(&cloud_live_ready,0,__ATOMIC_RELEASE);
    cloud_restore_refreshes=0;
    cloud_refresh_selection();
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
    if(index) {
        /* The descriptor builder only publishes the edited flag (0x1684).
         * The photo graph also reads the runtime base selector (0x1677). */
        FN(0x10100641u,u32,u32,u32,u32)(0x1677u,surrogate,1);
    }
    if(!wheel)
        FN(0x106b91c5u,void,u32)(0x5f);
    cloud_publish_live(previous,index);
}

/* Run before publishing the UI callbacks. An already active Auto graph can
 * accept the saved descriptor from the SD task. Its native builder queues
 * the edit notification; no scene transition or base change is needed. */
static void cloud_restore_at_startup(void)
{
    u32 index=cloud_confirmed;
    if(!cloud_fast_boot || !index ||
       __atomic_load_n(&cloud_boot_profile,__ATOMIC_ACQUIRE)!=index ||
       !cloud_photo_mode() || !WORD(CLOUD_LIVE_CONTROLLER+4) ||
       WORD(CLOUD_PC_BASE_VALUE) || WORD(CLOUD_PC_EFFECTIVE_BASE) ||
       CLOUD_HALF(CLOUD_PC_PHOTO_DESCRIPTOR+0x50)!=CLOUD_PC_AUTO ||
       !WORD(CLOUD_PC_PHOTO_DESCRIPTOR+0x54))
        return;
    u32 stock_id=CLOUD_HALF(0xb188b2dau);
    if(stock_id!=(u32)FN(0x1063cc51u,u32,void)())
        return;
    u32 now=FN(0x1062e59du,u32,void)();
    if(!cloud_set_preview_descriptor(index,0,stock_id))
        return;
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    cloud_refresh_selection();
    if(cloud_active==index) {
        cloud_restore_pending=CLOUD_RESTORE_WAITING;
        cloud_restore_last_attempt=now;
    }
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
}

static void cloud_ensure_catalog(void)
{
    if(!cloud_catalog_pending || !cloud_scan(CLOUD_CANDIDATES,cloud_load_candidate))
        return;
    cloud_catalog_pending=0;
    u32 index=cloud_catalog_match();
    FN(0x103d5919u,void,u32 *)(cloud_style_mutex);
    if(index && cloud_catalog->style_prepared[CLOUD_BOOT_INDEX-1u]) {
        z72_style_copy(&cloud_catalog->profiles[index-1u].style,&cloud_catalog->profiles[CLOUD_BOOT_INDEX-1u].style,
                       sizeof(cloud_catalog->profiles[0].style));
        __atomic_store_n(&cloud_catalog->style_prepared[index-1u],1,__ATOMIC_RELEASE);
    }
    if(cloud_active==CLOUD_BOOT_INDEX && index)
        cloud_active=index;
    if(cloud_confirmed==CLOUD_BOOT_INDEX)
        cloud_confirmed=index;
    if(cloud_rendered==CLOUD_BOOT_INDEX)
        __atomic_store_n(&cloud_rendered,index,__ATOMIC_RELEASE);
    if(cloud_live_applied==CLOUD_BOOT_INDEX)
        __atomic_store_n(&cloud_live_applied,index,__ATOMIC_RELEASE);
    if(__atomic_load_n(&cloud_live_ready,__ATOMIC_ACQUIRE)==CLOUD_BOOT_INDEX)
        __atomic_store_n(&cloud_live_ready,index,__ATOMIC_RELEASE);
    if(__atomic_load_n(&cloud_boot_profile,__ATOMIC_ACQUIRE)==CLOUD_BOOT_INDEX)
        __atomic_store_n(&cloud_boot_profile,index,__ATOMIC_RELEASE);
    FN(0x103d58fbu,void,u32 *)(cloud_style_mutex);
    if(index) {
        cloud_save_selection(index);
    } else {
        cloud_restore_pending=CLOUD_RESTORE_DONE;
        cloud_preview(0);
    }
}

static void cloud_restore_selection(void)
{
    if(!cloud_confirmed || cloud_picker_active ||
       !cloud_photo_mode() || !WORD(CLOUD_LIVE_CONTROLLER+4))
        return;
    /* A deliberate stock selection can move the native slot. Recovery after
     * a successful restore is only for a rebuild of the same native slot. */
    if(!cloud_restore_pending && cloud_requested_profile()!=cloud_confirmed)
        return;
    u32 base=cloud_native_base(cloud_confirmed);
    u32 rebuild=cloud_restore_pending==CLOUD_RESTORE_NEW || cloud_selected_profile()!=cloud_confirmed ||
                WORD(CLOUD_PC_BASE_VALUE)!=base;
    u32 base_ready=!rebuild && WORD(CLOUD_PC_EFFECTIVE_BASE)==base;
    if(base_ready) {
        if(__atomic_load_n(&cloud_live_ready,__ATOMIC_ACQUIRE)==cloud_confirmed) {
            cloud_restore_pending=CLOUD_RESTORE_DONE;
            return;
        }
        if(base || cloud_restore_refreshes>=CLOUD_RESTORE_REFRESH_LIMIT)
            return;
    }
    u32 now=FN(0x1062e59du,u32,void)();
    if(cloud_restore_pending==CLOUD_RESTORE_WAITING &&
       now-cloud_restore_last_attempt<CLOUD_RESTORE_INTERVAL)
        return;
    /* Startup can activate the controller before loading the camera's slot. */
    if(CLOUD_HALF(0xb188b2dau)!=(u32)FN(0x1063cc51u,u32,void)())
        return;
    cloud_restore_pending=CLOUD_RESTORE_WAITING;
    cloud_restore_last_attempt=now;
    if(rebuild) {
        cloud_preview(cloud_confirmed);
    } else {
        /* The Auto base can already match while startup publications still
         * omit both color tables. Re-notify the current descriptor and base
         * through the native queue, without changing either property. */
        u32 event=FN(0x101010dfu,u32,u32)(1);
        if(event) {
            FN(0x10101001u,void,u32,u32,u32)(event,0x1677u,base);
            if(base_ready) {
                u32 edited=FN(0x1061d413u,u32,u32)(0x1684u);
                FN(0x10101001u,void,u32,u32,u32)(event,0x1684u,edited);
                ++cloud_restore_refreshes;
            }
            FN(0x10100ee1u,u32,u32)(event);
        }
    }
}

static void cloud_before_live_update(void)
{
    if(cloud_restore_pending==CLOUD_RESTORE_NEW)
        cloud_restore_selection();
}

static u32 cloud_live_idle(u32 a,u32 b,u32 c,u32 d)
{
    cloud_before_live_update();
    u32 result=FN(CLOUD_LIVE_IDLE,u32,u32,u32,u32,u32)(a,b,c,d);
    cloud_restore_selection();
    return result;
}

static char *cloud_number(char *label,u32 index)
{
    if(index>=10)
        *label++=(char)('0'+index/10);
    *label++=(char)('0'+index%10);
    *label=0;
    return label;
}

static void cloud_b_label(char *label,u32 index)
{
    label[0]='B';
    label[1]='-';
    cloud_number(label+2,index);
}

static void cloud_widget_content(u32 widget,u32 slot,u32 kind)
{
    if(WORD(widget+0xc)==kind) {
        FN(0x106361c9u,u32,u32,u32,u32)(widget,slot,kind);
    } else {
        FN(0x1063636du,void,u32)(widget);
        WORD(widget+0x44)=1;
        WORD(widget+8)=slot;
        WORD(widget+0xc)=kind;
        FN(0x10636369u,void,u32)(widget);
    }
}

static void cloud_draw_title(u32 view,const char *title)
{
    u32 widget=cloud_widget(view,0x8001u);
    if(widget) {
        FN(0x10636a07u,void,const char *,u32,u32)(title,0xc8,0x2a);
        cloud_widget_content(widget,0xc8,2);
    }
}

/* Ordinary widgets stay owned by the native view manager even while detached
 * from its drawing list. The stock close path releases the whole pool. */
static u32 cloud_managed_widget(u32 manager,u32 id)
{
    u32 table=WORD(manager+8),count=CLOUD_HALF(manager+4);
    for(u32 i=0;i<count;++i) {
        u32 widget=WORD(table+4*i);
        if(widget && WORD(widget)==id)
            return widget;
    }
    return 0;
}

static void cloud_page_indicator(u32 view,u32 page)
{
    u32 root=WORD(view+0xc),manager=WORD(view+0x24);
    u32 widget=cloud_widget(view,CLOUD_PAGE_WIDGET);
    if(page) {
        if(!WORD(0xb20287b4u+CLOUD_PAGE_TEXT*12u+4u))
            return;
        char label[6];
        char *end=cloud_number(label,page);
        *end++='/';
        cloud_number(end,(cloud_profile_count+CLOUD_PC_CARDS-1u)/CLOUD_PC_CARDS);
        FN(0x10636a07u,void,const char *,u32,u32)(label,CLOUD_PAGE_TEXT,11);
        if(!widget) {
            widget=cloud_managed_widget(manager,CLOUD_PAGE_WIDGET);
            if(!widget) {
                u32 left=cloud_widget(view,0x800fu),right=cloud_widget(view,0x8010u);
                u32 slot=FN(0x106345fbu,u32,u32,u32)(manager,0);
                if(!left || !right || slot>=CLOUD_HALF(manager+4))
                    return;
                unsigned short center=(CLOUD_HALF(left+0x10)+CLOUD_HALF(right+0x14))/2u;
                unsigned short y=CLOUD_HALF(left+0x16);
                unsigned short rect[8]={center-100u,y,center+100u,y+34u,
                                        center-100u,y,center+100u,y+34u};
                widget=FN(0x10634623u,u32,u32,u32,u32)(manager,0,slot);
                if(!widget)
                    return;
                FN(0x1059204fu,void,u32,u32,const unsigned short *,u32)(
                    widget,CLOUD_PAGE_TEXT,rect,CLOUD_PAGE_WIDGET);
            }
            FN(0x10635cebu,u32,u32,u32)(root,widget);
        }
        cloud_widget_content(widget,CLOUD_PAGE_TEXT,2);
    } else if(widget) {
        FN(0x10635cf3u,u32,u32,u32)(root,widget);
    } else {
        return;
    }
    u32 dots=WORD(CLOUD_PC_STATE+0x30);
    for(u32 i=0;i<dots && i<8;++i) {
        u32 id=0x8011u+i;
        u32 dot=cloud_widget(view,id);
        if(page && dot) {
            FN(0x10635cf3u,u32,u32,u32)(root,dot);
        } else if(!page && !dot) {
            dot=cloud_managed_widget(manager,id);
            if(dot)
                FN(0x10635cebu,u32,u32,u32)(root,dot);
        }
    }
}

static u32 cloud_child_attached(u32 parent,u32 child)
{
    return FN(0x1034fc39u,u32,u32,u32,u32)(parent+0xec,child,0)!=0;
}

static struct cloud_i_tile {
    u32 icon,painter,index;
    u32 text[0x120/4];
} cloud_i_tiles[2];

static unsigned short cloud_i_wheel_icons[CLOUD_PC_NATIVE_ROWS+CLOUD_CANDIDATES];
static u32 cloud_i_wheel_stock,cloud_i_wheel_position;
static u32 cloud_i_wheel_dirty,cloud_i_wheel_changed_at;

static u32 cloud_i_draw(u32 widget,u32 predicate);

static u32 cloud_i_wheel_active(void)
{
    return CLOUD_BYTE(CLOUD_I_STATE)==2 && WORD(CLOUD_I_WHEEL_SCENE_SLOT)==CLOUD_I_WHEEL_SCENE &&
           WORD(CLOUD_I_WHEEL_MODEL+0x14)==(u32)cloud_i_wheel_icons;
}

static u32 cloud_i_reset(struct cloud_i_tile *tile)
{
    u32 icon=tile->icon;
    if(!icon)
        return 0;
    if(WORD(icon+0x4c)==(u32)cloud_i_draw) {
        WORD(icon+0x4c)=tile->painter;
        WORD(icon+0x44)=1;
    }
    FN(0x1063636du,void,u32 *)(tile->text);
    tile->icon=0;
    return 1;
}

static u32 cloud_i_draw(u32 widget,u32 predicate)
{
    for(u32 plane=0;plane<2;++plane) {
        struct cloud_i_tile *tile=&cloud_i_tiles[plane];
        if(tile->icon!=widget)
            continue;
        if(!WORD(widget+0x28) || cloud_browse ||
           cloud_requested_profile()!=tile->index)
            return FN(tile->painter,u32,u32,u32)(widget,predicate);
        /* Draw in the native icon's pass with its current focus palette. */
        u32 *text=tile->text;
        z72_style_copy(text+0x34/4,(const void *)(widget+0x34),16);
        text[0x44/4]=1;
        return FN(0x10650bd1u,u32,u32 *,u32)(text,predicate);
    }
    return 0;
}

static u32 cloud_i_apply(u32 root,struct cloud_i_tile *tile,u32 record,u32 index)
{
    u32 parent=WORD(record),icon=WORD(record+8);
    if(!z72_style_ram(parent,0x100) || !z72_style_ram(icon,0x100) ||
       WORD(parent+0x48)!=root || WORD(icon+0x48)!=parent ||
       FN(0x10635cefu,u32,u32,u32)(WORD(parent),root)!=parent ||
       !cloud_child_attached(parent,icon))
        return cloud_i_reset(tile);
    u32 painter=WORD(icon+0x4c),left=WORD(icon+0x10),right=WORD(icon+0x14);
    if(icon==tile->icon && painter==(u32)cloud_i_draw && index==tile->index &&
       left==tile->text[0x98/4] && right==tile->text[0x9c/4])
        return 0;
    cloud_i_reset(tile);
    painter=WORD(icon+0x4c);
    if(!painter || painter==(u32)cloud_i_draw)
        return 0;
    char label[5];
    cloud_b_label(label,index==CLOUD_BOOT_INDEX ? cloud_resume_slot : index);
    FN(0x10636a07u,void,const char *,u32,u32)(label,CLOUD_I_TEXT,4);
    u32 *text=tile->text;
    FN(0x10361035u,void,void *,u32)(text,sizeof(tile->text));
    text[2]=CLOUD_I_TEXT;
    text[3]=2;
    text[4]=text[0x98/4]=left;
    text[5]=text[0x9c/4]=right;
    text[0x28/4]=text[0x2c/4]=1;
    text[0x48/4]=parent;
    FN(0x10636369u,void,u32 *)(text);
    FN(0x1065109bu,u32,u32 *,u32)(text,1);
    tile->icon=icon;
    tile->painter=painter;
    tile->index=index;
    WORD(icon+0x4c)=(u32)cloud_i_draw;
    WORD(icon+0x44)=WORD(parent+0x44)=1;
    return 1;
}

static void cloud_i_refresh(void)
{
    u32 app=CLOUD_BYTE(0xb1821b18u),state=CLOUD_BYTE(CLOUD_I_STATE);
    u32 variant=CLOUD_BYTE(CLOUD_I_STATE+1);
    if((app!=3 && app!=4) || WORD(0xb202f728u+app*4)!=CLOUD_I_APP ||
       (state!=3 && !cloud_i_wheel_active() && (state!=0 || (variant!=0 && variant!=0x31))))
        return;
    u32 root=FN(0x1060d027u,u32,void)();
    if(!root)
        return;
    u32 plane=root==WORD(0xb18242ecu);
    struct cloud_i_tile *tile=&cloud_i_tiles[plane];
    u32 index=cloud_browse ? 0 : cloud_requested_profile();
    u32 changed=0;
    if(index && index==cloud_confirmed && WORD(0xb20287b4u+CLOUD_I_TEXT*12u+4u)) {
        for(u32 i=0;i<12;++i)
            if(FN(0x10604c3bu,u32,u32)(i)==0x15u) {
                changed=cloud_i_apply(root,tile,(plane ? 0xb2097a10u : 0xb20970b0u)+200*i,index);
                break;
            }
    } else {
        changed=cloud_i_reset(tile);
    }
    if(changed)
        FN(0x10635cf7u,void,u32,u32)(root,0x106355dbu);
}

static void cloud_i_wheel_title(u32 position,u32 unused)
{
    (void)unused;
    if(position<cloud_i_wheel_stock) {
        FN(0x105c3e97u,void,u32)(position);
        return;
    }
    u32 index=position-cloud_i_wheel_stock+1;
    if(index>cloud_profile_count)
        return;
    char label[27];
    cloud_b_label(label,index);
    char *end=label+(index>=10 ? 4 : 3);
    *end++=' ';
    const char *name=cloud_catalog->profiles[index-1].native.name;
    for(u32 i=0;i<sizeof(cloud_catalog->profiles[0].native.name) && name[i];++i)
        *end++=name[i];
    *end=0;

    /* Use the native named-C title painter for the current B preset. */
    for(u32 plane=0;plane<2;++plane) {
        FN(0x1058c215u,void,u32)(plane*2);
        u32 root=FN(0x1058c209u,u32,void)();
        if(root) {
            FN(0x105c3e4fu,void,const char *,u32,u32,u32)(
                label,CLOUD_BYTE(0xb1821ac0u+5*plane),plane ? 0x2f4u : 0x2edu,root);
            if(FN(0x10631947u,u32,void)()==(plane ? 4u : 1u) ||
               (!plane && FN(0x105c180fu,u32,void)()))
                FN(0x10635cf7u,void,u32,u32)(root,0x106355dbu);
        }
    }
    FN(0x1058c215u,void,u32)(0);
}

static void cloud_i_wheel_save(void)
{
    if(cloud_i_wheel_dirty) {
        cloud_save_selection(cloud_confirmed);
        cloud_i_wheel_dirty=0;
    }
}

static u32 cloud_i_wheel_step(u32 key,u32 steps)
{
    u32 count=cloud_i_wheel_stock+cloud_profile_count;
    while(steps>=count)
        steps-=count;
    if(!steps)
        return 0;
    u32 forward=key==0x42u || key==0x43u;
    u32 next=cloud_i_wheel_position+(forward ? steps : count-steps);
    if(next>=count)
        next-=count;
    u32 index=next<cloud_i_wheel_stock ? 0 : next-cloud_i_wheel_stock+1;
    if(!index) {
        u32 slot=next<CLOUD_I_WHEEL_STOCK_BASES ? next :
                 CLOUD_HALF(CLOUD_I_WHEEL_SLOTS+2*(next-CLOUD_I_WHEEL_STOCK_BASES));
        FN(0x1063c7e1u,u32,u32,u32)(0x525u,slot);
    }
    cloud_confirmed=index;
    cloud_preview(index);
    cloud_restore_pending=index ? CLOUD_RESTORE_WAITING : CLOUD_RESTORE_DONE;
    cloud_i_wheel_changed_at=FN(0x1062e59du,u32,void)();
    cloud_restore_last_attempt=cloud_i_wheel_changed_at;
    cloud_i_wheel_dirty=1;
    cloud_i_wheel_position=next;

    for(u32 i=0;i<steps;++i)
        FN(0x1058be99u,void,u32)(key);
    FN(0x1058b30bu,void,void)();
    cloud_i_wheel_title(next,0);
    cloud_i_refresh();
    return 1;
}

static u32 cloud_i_input(u32 key)
{
    if(cloud_picker_installed && key>=0x41u && key<=0x44u &&
       cloud_photo_mode() && !WORD(0xb25519b8u) &&
       !FN(0x104a96ffu,u32,void)()) {
        if(cloud_i_wheel_active())
            return cloud_i_wheel_step(key,FN(0x10542629u,u32,void)());
        if(!CLOUD_BYTE(CLOUD_I_STATE) && FN(0x106020edu,u32,void)()==0x15u &&
           FN(0x10604b0du,u32,void)()) {
            cloud_ensure_catalog();
            if(!cloud_profile_count)
                return FN(CLOUD_I_INPUT,u32,u32)(key);
            /* Enter the native dial scene with no initial stock adjustment. */
            WORD(CLOUD_I_STATE+0x38)=0;
            FN(0x105c0263u,void,u32)(2);
            u32 stock=WORD(CLOUD_I_WHEEL_MODEL),position=WORD(CLOUD_I_WHEEL_MODEL+8);
            if(WORD(CLOUD_I_WHEEL_SCENE_SLOT)==CLOUD_I_WHEEL_SCENE &&
               WORD(CLOUD_I_WHEEL_MODEL+0x14)==CLOUD_I_WHEEL_ICONS &&
               stock>=CLOUD_I_WHEEL_STOCK_BASES && stock<=CLOUD_PC_NATIVE_ROWS && position<stock) {
                cloud_i_wheel_stock=stock;
                for(u32 i=0;i<stock;++i)
                    cloud_i_wheel_icons[i]=CLOUD_HALF(CLOUD_I_WHEEL_ICONS+2*i);
                for(u32 i=0;i<cloud_profile_count;++i)
                    cloud_i_wheel_icons[stock+i]=cloud_i_wheel_icons[cloud_native_base(i+1)];
                u32 index=cloud_requested_profile();
                if(!index && cloud_restore_pending)
                    index=cloud_confirmed;
                cloud_i_wheel_position=index ? stock+index-1 : position;
                WORD(CLOUD_I_STATE+0x80)=cloud_i_wheel_position;
                FN(0x105c73d3u,void,u32,u32,u32,u32,u32)((u32)cloud_i_wheel_title,0,0,0,0);
                FN(0x105c2a5du,void,u32,const unsigned short *,u32,u32)(
                    stock+cloud_profile_count,cloud_i_wheel_icons,0,0);
                FN(0x1058b8c7u,void,u32)(CLOUD_I_WHEEL_SCENE);
                cloud_i_wheel_step(key,1);
                return 0;
            }
        }
    }
    u32 result=FN(CLOUD_I_INPUT,u32,u32)(key);
    if(!cloud_i_wheel_active())
        cloud_i_wheel_save();
    return result;
}

static u32 cloud_i_open(void)
{
    u32 result=FN(CLOUD_I_OPEN,u32,void)();
    cloud_restore_selection();
    cloud_i_refresh();
    return result;
}

static u32 cloud_i_close(void)
{
    cloud_i_wheel_save();
    cloud_i_reset(&cloud_i_tiles[0]);
    cloud_i_reset(&cloud_i_tiles[1]);
    return FN(CLOUD_I_CLOSE,u32,void)();
}

static u32 cloud_i_update(void)
{
    u32 result=FN(CLOUD_I_UPDATE,u32,void)();
    if(cloud_i_wheel_dirty && (!cloud_i_wheel_active() ||
       FN(0x1062e59du,u32,void)()-cloud_i_wheel_changed_at>=300u))
        cloud_i_wheel_save();
    cloud_restore_selection();
    cloud_i_refresh();
    return result;
}

static void cloud_update_cards(u32 view,u32 first)
{
    u32 manager=WORD(view+0x24);
    for(u32 i=0;i<CLOUD_PC_CARDS;++i) {
        u32 label=cloud_card(view,i);
        /* The slot contents changed. The native text setter invalidates the
         * card's rendered glyphs even when the resource ID stays the same. */
        FN(0x106361c9u,u32,u32,u32,u32)(label,WORD(label+8),WORD(label+0xc));
        if(!FN(0x1063464fu,u32,u32,u32,u32)(manager,i,0))
            continue;
        u32 thumb=FN(0x1063466du,u32,u32,u32,u32)(manager,i,0);
        u32 attached=cloud_child_attached(label,thumb);
        if(first && first+i>cloud_profile_count) {
            /* The native thumbnail draws a black fill when disabled. Remove
             * the child instead and keep the card frame and label. */
            if(attached)
                FN(0x10636331u,void,u32,u32)(label,thumb);
            continue;
        }
        u32 image;
        if(first) {
            image=cloud_catalog->profiles[first+i-1u].native.base==CLOUD_PC_RED ? 0x1418u : 0x1423u;
        } else {
            u32 record=WORD(WORD(view+0x28)+0xc)+28*i;
            image=WORD(record+0x14);
        }
        FN(0x106361c9u,u32,u32,u32,u32)(thumb,image,WORD(thumb+0xc));
        if(!attached)
            FN(0x10636329u,void,u32,u32)(label,thumb);
        FN(0x1063618bu,void,u32,u32)(thumb,1);
    }
}

static void cloud_show_page(u32 index)
{
    u32 first=(index-1u)/CLOUD_PC_CARDS*CLOUD_PC_CARDS+1u;
    for(u32 i=0;i<CLOUD_PC_CARDS;++i) {
        char label[5]=" ";
        if(first+i<=cloud_profile_count)
            cloud_b_label(label,first+i);
        FN(0x10636a07u,void,const char *,u32,u32)(label,0x8cu+i,4);
    }

    cloud_browse=index;
    cloud_page_first=first;
    for(u32 plane=0;plane<2;++plane) {
        FN(0x10591993u,void,u32,u32)(plane,1);
        u32 view=cloud_view();
        if(!cloud_view_ready(view))
            continue;
        FN(0x1058f487u,void,u32,u32,u32)(view,0,index-first);
        cloud_update_cards(view,first);
        cloud_draw_title(view,cloud_catalog->profiles[index-1u].profile.name);
        cloud_page_indicator(view,(index-1u)/CLOUD_PC_CARDS+1u);
        FN(0x1058f9efu,void,u32)(view);
        FN(0x1058f9cbu,void,u32)(view);
        FN(0x10635cf7u,void,u32,u32)(WORD(view+0xc),0x106355dbu);
    }
    FN(0x1058c793u,void,void)();
    cloud_preview(index);
}

static u32 cloud_begin_page(u32 index)
{
    if(!index || index>cloud_profile_count || !cloud_view_ready(cloud_view()))
        return 0;
    for(u32 plane=0;plane<2;++plane) {
        FN(0x10591993u,void,u32,u32)(plane,1);
        u32 view=cloud_view();
        cloud_native_start[plane]=CLOUD_HALF(view+0x18);
        cloud_native_focus[plane]=CLOUD_HALF(view+0x16);
    }
    FN(0x1058c793u,void,void)();
    cloud_show_page(index);
    return 1;
}

static void cloud_leave_page(int direction)
{
    if(!cloud_browse)
        return;
    cloud_browse=0;
    for(u32 plane=0;plane<2;++plane) {
        FN(0x10591993u,void,u32,u32)(plane,1);
        u32 view=cloud_view();
        if(!cloud_view_ready(view))
            continue;
        FN(0x105901a3u,void,void)();
        FN(0x1058f487u,void,u32,u32,u32)(view,direction ? 0 : cloud_native_start[plane],
                                       direction ? 0 : cloud_native_focus[plane]);
        /* Start at Auto and use the stock backward step to find the last
         * selectable native preset, including when the C slots are sparse. */
        if(direction<0)
            FN(0x1058f3f7u,void,u32,int,u32)(view,-1,2);
        cloud_update_cards(view,0);
        cloud_page_indicator(view,0);
        FN(0x1058f8cfu,void,void)();
        FN(0x1058f971u,void,u32)(view);
        FN(0x1058f9efu,void,u32)(view);
        FN(0x1058f9cbu,void,u32)(view);
        FN(0x10635cf7u,void,u32,u32)(WORD(view+0xc),0x106355dbu);
    }
    FN(0x1058c793u,void,void)();
}

static void cloud_move(int direction,u32 step)
{
    if(!cloud_browse)
        return;
    int anchor=(int)(step==1 ? cloud_browse : cloud_page_first);
    int next=anchor+direction*(int)step;
    if(next<1 || next>(int)cloud_profile_count) {
        cloud_leave_page(direction);
        FN(0x1058f88fu,void,void)();
        cloud_preview(0);
        return;
    }
    cloud_show_page((u32)next);
}

static void cloud_confirm(void)
{
    cloud_confirmed=cloud_browse;
    cloud_restore_pending=CLOUD_RESTORE_NEW;
    cloud_save_selection(cloud_confirmed);
    cloud_leave_page(0);
    /* Restore the original 0x525 value, then let native exit clean up. */
    WORD(CLOUD_PC_PENDING+8)=0;
    FN(0x10590765u,void,void)();
    WORD(CLOUD_PC_PENDING)=0;
    FN(0x1058c54bu,void,u32)(2);
}

static void cloud_picker_key(u32 key)
{
    if(cloud_photo_picker()) {
        int direction=0;
        switch(key) {
        case 0x02: case 0x06: case 0x08: case 0x39: case 0x3d: case 0x3e: case 0x42:
            direction=1;
            break;
        case 0x03: case 0x07: case 0x09: case 0x3a: case 0x3f: case 0x40: case 0x41:
            direction=-1;
            break;
        }
        if(direction && cloud_browse) {
            cloud_move(direction,1);
            return;
        }
        if(direction && cloud_begin_page(cloud_b_edge(cloud_view(),direction,0)))
            return;
        if(key==0x1b) {
            if(cloud_browse) {
                cloud_confirm();
                return;
            }
            cloud_confirmed=0;
            cloud_restore_pending=CLOUD_RESTORE_DONE;
            cloud_save_selection(0);
        }
        if(cloud_browse && key==0x52) {
            u32 view=cloud_view();
            u32 cell=CLOUD_HALF(view+0x16);
            u32 id=cloud_page_first+cell;
            cloud_show_page(cell<CLOUD_PC_CARDS && id<=cloud_profile_count ? id : cloud_browse);
            return;
        }
        /* B exposes the NP3 settings as stored on SD. Stock Adjust would edit
         * the placeholder native card instead. */
        if(cloud_browse && (key==0x05 || key==0x3c || key==0x43))
            return;
    }
    FN(CLOUD_PC_KEY,void,u32)(key);
}

static void cloud_combined_picture_key(u32 key)
{
    if(cloud_picker_active)
        cloud_picker_key(key);
    else
        FN(CLOUD_PC_KEY,void,u32)(key);
}

static void cloud_picker_event(u32 event)
{
    if(cloud_photo_picker()) {
        if(event==5) {
            if(cloud_browse) {
                cloud_confirm();
                return;
            }
            cloud_confirmed=0;
            cloud_restore_pending=CLOUD_RESTORE_DONE;
            cloud_save_selection(0);
        } else if(event==3 && cloud_browse) {
            /* The stock suspend action saves the native carousel position. */
            cloud_leave_page(0);
            FN(CLOUD_PC_EVENT,void,u32)(event);
            cloud_preview(cloud_confirmed);
            return;
        }
    }
    FN(CLOUD_PC_EVENT,void,u32)(event);
}

static void cloud_picker_drag(u32 view,u32 direction,u32 finalized,u32 position);

static void cloud_picker_close(void)
{
    u32 photo=cloud_photo_picker();
    u32 had_preview=cloud_browse;
    if(photo) {
        if(WORD(CLOUD_PC_DRAG_SLOT)==(u32)cloud_picker_drag)
            WORD(CLOUD_PC_DRAG_SLOT)=CLOUD_PC_DRAG;
        cloud_leave_page(0);
    }
    FN(CLOUD_PC_CLOSE,void,void)();
    cloud_picker_active=0;
    if(photo && (cloud_confirmed || had_preview)) {
        cloud_preview(cloud_confirmed);
        if(cloud_confirmed) {
            cloud_restore_pending=CLOUD_RESTORE_WAITING;
            cloud_restore_last_attempt=FN(0x1062e59du,u32,void)();
        }
    }
    if(photo)
        cloud_i_refresh();
}

static void cloud_picker_arrow(void)
{
    u32 touch=0,widget=0;
    FN(0x10623521u,void,u32 *,u32 *)(&touch,&widget);
    if(cloud_photo_picker() && touch && widget) {
        int direction=WORD(widget)==0x800fu ? -1 : 1;
        if(cloud_browse) {
            if(CLOUD_BYTE(touch+8)!=1)
                cloud_move(direction,CLOUD_PC_CARDS);
            return;
        }
        if(CLOUD_BYTE(touch+8)!=1 &&
           cloud_begin_page(cloud_b_edge(cloud_view(),direction,1)))
            return;
    }
    FN(CLOUD_PC_ARROW,void,void)();
}

static void cloud_picker_drag(u32 view,u32 direction,u32 finalized,u32 position)
{
    if(cloud_photo_picker()) {
        if(cloud_browse) {
            if(!finalized && position)
                FN(0x1059024du,void,u32,u32,u32,int)(view,position,0,-1);
            if(finalized && direction!=1)
                cloud_move(direction==2 ? 1 : -1,CLOUD_PC_CARDS);
            return;
        }
        u32 edge=direction!=1 ? cloud_b_edge(view,direction==2 ? 1 : -1,1) : 0;
        if(edge) {
            if(finalized && cloud_begin_page(edge))
                return;
            if(!finalized && position) {
                FN(0x1059024du,void,u32,u32,u32,int)(view,position,0,-1);
                return;
            }
        }
    }
    FN(CLOUD_PC_DRAG,void,u32,u32,u32,u32)(view,direction,finalized,position);
}

static void cloud_install_touch(void)
{
    if(WORD(CLOUD_PC_DRAG_SLOT)==CLOUD_PC_DRAG)
        WORD(CLOUD_PC_DRAG_SLOT)=(u32)cloud_picker_drag;
    for(u32 plane=0;plane<2;++plane) {
        FN(0x10591993u,void,u32,u32)(plane,1);
        u32 view=cloud_view();
        for(u32 id=0x800fu;id<=0x8010u;++id) {
            u32 widget=cloud_widget(view,id);
            if(widget && WORD(widget+0x8c)==CLOUD_PC_ARROW)
                WORD(widget+0x8c)=(u32)cloud_picker_arrow;
        }
    }
    FN(0x1058c793u,void,void)();
}

static void cloud_picker_open(void)
{
    cloud_picker_active=1;
    u32 previously_browsing=cloud_browse;
    u32 selected=previously_browsing ? previously_browsing : cloud_requested_profile();
    if(!selected && cloud_restore_pending)
        selected=cloud_confirmed;
    cloud_leave_page(0);
    FN(CLOUD_PC_OPEN,void,void)();
    if(cloud_photo_picker()) {
        cloud_ensure_catalog();
        if(selected==CLOUD_BOOT_INDEX)
            selected=cloud_confirmed;
        cloud_install_touch();
        if(selected && !cloud_begin_page(selected))
            cloud_preview(selected);
        if(!previously_browsing)
            cloud_confirmed=selected;
    }
}

static void cloud_install_picker(void)
{
    cloud_restore_pending=cloud_confirmed ? CLOUD_RESTORE_NEW : CLOUD_RESTORE_DONE;
    if(cloud_confirmed && cloud_catalog->profiles[cloud_confirmed-1].profile.base==Z72_NP3_FLEXIBLE_BASE &&
       !z72_live_color_prepare(&cloud_color_action,(u32)cloud_generate_live_color)) {
        __atomic_store_n(&cloud_boot_profile,cloud_confirmed,__ATOMIC_RELEASE);
        for(u32 i=0;i<sizeof(z72_live_color_slots)/sizeof(z72_live_color_slots[0]);++i)
            WORD(z72_live_color_slots[i])=(u32)cloud_live_color_action;
    }
    cloud_restore_at_startup();
    cloud_picker_installed=1;
    WORD(CLOUD_PC_APP+0xc)=(u32)cloud_picker_open;
    WORD(CLOUD_PC_APP+0x10)=(u32)cloud_picker_close;
    WORD(CLOUD_PC_APP+0x18)=(u32)cloud_picker_event;
    if(WORD(CLOUD_LIVE_IDLE_SLOT)==CLOUD_LIVE_IDLE)
        WORD(CLOUD_LIVE_IDLE_SLOT)=(u32)cloud_live_idle;
    if(WORD(CLOUD_LIVE_IDLE_OTHER_SLOT)==CLOUD_LIVE_IDLE)
        WORD(CLOUD_LIVE_IDLE_OTHER_SLOT)=(u32)cloud_live_idle;
    if(WORD(CLOUD_I_APP+0xc)==CLOUD_I_OPEN && WORD(CLOUD_I_APP+0x10)==CLOUD_I_CLOSE &&
       WORD(CLOUD_I_APP+0x1c)==CLOUD_I_UPDATE) {
        WORD(CLOUD_I_APP+0xc)=(u32)cloud_i_open;
        WORD(CLOUD_I_APP+0x10)=(u32)cloud_i_close;
        WORD(CLOUD_I_APP+0x1c)=(u32)cloud_i_update;
        if(WORD(CLOUD_I_APP+0x14)==CLOUD_I_INPUT)
            WORD(CLOUD_I_APP+0x14)=(u32)cloud_i_input;
    }
}

static void cloud_init(void)
{
    if(WORD(CLOUD_PC_APP+0xc)!=CLOUD_PC_OPEN ||
       WORD(CLOUD_PC_APP+0x10)!=CLOUD_PC_CLOSE ||
       WORD(CLOUD_PC_APP+0x14)!=CLOUD_PC_KEY ||
       WORD(CLOUD_PC_APP+0x18)!=CLOUD_PC_EVENT || cloud_pipeline_prepare())
        return;
    void *owner=cloud_allocate_storage();
    if(!owner) return;
    cloud_scan_profiles();
    if(!cloud_profile_count && !cloud_catalog_pending) {
        FN(0x1034fe7fu,void,void *)(owner);
        return;
    }
    cloud_pipeline_install();
    cloud_install_picker();
}
