/* SPDX-License-Identifier: MIT */
enum { MOV_DIR_BYTES=17, MOV_NAME_BYTES=32,
#ifdef Z72_RAW_MOVIE_STEM
       /* Longest stem fitting the native path, plus the longest part suffix. */
       MOV_STEM_BYTES=MOV_NAME_BYTES-MOV_DIR_BYTES-4,
       RAW_PATH_BYTES=MOV_DIR_BYTES+MOV_STEM_BYTES+PART_PATH_BYTES-PART_PREFIX,
#else
       RAW_PATH_BYTES=MOV_DIR_BYTES+PART_PATH_BYTES-3,
#endif
};
static char recording_dir[MOV_DIR_BYTES+1];
#ifdef Z72_RAW_MOVIE_STEM
static char movie_stem[MOV_STEM_BYTES];
#endif

/* C 1.70 prepares the primary DCF path at 10475354..10475384 before the
 * muxer's open callback, including when that callback returns a virtual handle. */
static int select_movie_directory(char target)
{
    char path[MOV_NAME_BYTES];
    const u32 row=0xb2989228u;
    CALL(0x10360f19u,void,u32)(0xb188ac4cu);
    const u32 kind=BYTE(row);
    for (u32 i=0;i<sizeof(path);++i) path[i]=(char)BYTE(row+0x38u+i);
    CALL(0x10360f13u,void,u32)(0xb188ac4cu);
    if (kind!=1u || log.route.descriptor_valid!=1u || log.route.descriptor_slot>1u ||
        path[0]!='A'+log.route.descriptor_slot) return 0;
    const char prefix[]=":\\DCIM\\";
    for (u32 i=0;i<sizeof(prefix)-1u;++i) if (path[1+i]!=prefix[i]) return 0;
    if (path[8]<'1' || path[8]>'9' || path[9]<'0' || path[9]>'9' ||
        path[10]<'0' || path[10]>'9' || path[16]!='\\') return 0;
    for (u32 i=11;i<16;++i)
        if ((unsigned char)path[i]<0x20 || path[i]=='\\' || path[i]=='/' || path[i]==':') return 0;
    u32 end=MOV_DIR_BYTES;
    while (end<sizeof(path) && path[end]) {
        if (path[end]=='\\' || path[end]=='/') return 0;
        ++end;
    }
    if (end==sizeof(path) || end<MOV_DIR_BYTES+5u || path[end-4]!='.' || path[end-3]!='M' ||
        !((path[end-2]=='O' && path[end-1]=='V') || (path[end-2]=='P' && path[end-1]=='4'))) return 0;
#ifdef Z72_RAW_MOVIE_STEM
    u32 n=0;
    if (!RECORD_ONLY()) {
        for (u32 i=MOV_DIR_BYTES;i<end-4u;++i) movie_stem[n++]=path[i];
    }
    movie_stem[n]=0;
#endif
    for (u32 i=0;i<MOV_DIR_BYTES;++i) recording_dir[i]=path[i];
    recording_dir[0]=target;
    recording_dir[MOV_DIR_BYTES]=0;
    return 1;
}

/* Native stat returns the FAT attributes at +0x20e for both FAT and exFAT.
 * Query before mkdir and confirm the directory if another task created it. */
static int ensure_movie_directory(const char *path)
{
    static _Alignas(64) unsigned char stat[0x220];
    u32 status=CALL(0x104711ddu,u32,const char *,void *)(path,stat);
    if (status && CALL(0x104d3a19u,u32,u32)(status)==FILE_MISSING) {
        const u32 created=CALL(0x10471289u,u32,const char *,void *)(path,0);
        status=CALL(0x104711ddu,u32,const char *,void *)(path,stat);
        if (status && created) status=created;
    }
    if (!status && (stat[0x20e]&0x10u)) return 1;
    segment_file_error(7,status);
    return 0;
}

static int prepare_movie_directory(char target)
{
    if (!select_movie_directory(target)) {
        segment_file_error(6,0);
        return 0;
    }
    /* A freshly formatted RAW card may lack the MOV card's selected folder. */
    recording_dir[7]=0;
    const int root_ok=ensure_movie_directory(recording_dir);
    recording_dir[7]='\\';
    recording_dir[16]=0;
    const int ok=root_ok && ensure_movie_directory(recording_dir);
    recording_dir[16]='\\';
    return ok;
}

static const char *movie_raw_path(char out[RAW_PATH_BYTES],const char *root_name)
{
    for (u32 i=0;i<MOV_DIR_BYTES;++i) out[i]=recording_dir[i];
    char *p=out+MOV_DIR_BYTES;
    const char *name=root_name+3;
#ifdef Z72_RAW_MOVIE_STEM
    if (movie_stem[0]) {
        for (const char *s=movie_stem;*s;++s) *p++=*s;
        name=root_name+PART_PREFIX-1u; /* dot and extension, including numbered parts */
    }
#endif
    while (*name) *p++=*name++;
    *p=0;
    return out;
}

static u32 raw_file_status(const char *name)
{
    char path[RAW_PATH_BYTES];
    return file_status(movie_raw_path(path,name));
}
