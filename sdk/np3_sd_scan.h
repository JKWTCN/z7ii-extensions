/* SPDX-License-Identifier: MIT */
#ifndef Z72_NP3_SD_SCAN_H
#define Z72_NP3_SD_SCAN_H

#define CLOUD_CANDIDATES 99u
#define CLOUD_FILENAME 110u
#define CLOUD_PRESET_DIRECTORY "B:\\CUSTOMPC\\"
static const char cloud_directory[] = CLOUD_PRESET_DIRECTORY;
static char (*cloud_filenames)[CLOUD_FILENAME];
static z72_u32 cloud_file_count;
/* C 1.70 directory callback record. The original scanner reads the ASCII
 * alias at +0, the UTF-16 long filename at +0x0e and attributes at +0x20e. */
struct cloud_directory_entry {
    char alias[14];
    unsigned short name[256];
    unsigned char attributes;
    unsigned char remainder[17];
};
typedef u32 (*cloud_directory_callback)(const struct cloud_directory_entry *,u32);

_Static_assert(sizeof(struct cloud_directory_entry)==0x220,"directory record size");

static u32 cloud_collect(const struct cloud_directory_entry *entries,u32 count)
{
    for (u32 i = 0; i < count && cloud_file_count < CLOUD_CANDIDATES; ++i) {
        const struct cloud_directory_entry *entry = &entries[i];
        if (entry->attributes & 0x18u)
            continue;
        char *name = cloud_filenames[cloud_file_count];
        u32 long_name = entry->name[0] != 0;
        u32 size = long_name ? CLOUD_FILENAME : sizeof(entry->alias);
        u32 n = 0;
        for (; n < size; ++n) {
            u32 ch = long_name ? entry->name[n] : (unsigned char)entry->alias[n];
            if (!ch)
                break;
            if (ch < ' ' || ch > '~' || ch == '/' || ch == '\\' || ch == ':') {
                n = size;
                break;
            }
            name[n] = (char)ch;
        }
        if (n >= size || n <= 4 || name[n-4] != '.' ||
            (name[n-3] | 32) != 'n' || (name[n-2] | 32) != 'p' || name[n-1] != '3')
            continue;
        for (; n < CLOUD_FILENAME; ++n) name[n] = 0;
        ++cloud_file_count;
    }
    return cloud_file_count == CLOUD_CANDIDATES;
}

static u32 cloud_english_name(const char *name)
{
    u32 visible = 0;
    for (u32 i = 0; name[i]; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (ch < ' ' || ch > '~')
            return 0;
        visible |= ch != ' ';
    }
    return visible;
}

static u32 cloud_scan(u32 limit,u32 (*add)(const char *path))
{
    if (!limit || !FN(0x104d3ef1u,u32,const char *)(cloud_directory))
        return 0;
    /* Opening a profile is deferred until the original scanner returns. */
    cloud_file_count=0;
    u32 status=FN(0x104d3e93u,u32,const char *,u32,cloud_directory_callback,u32)(
        cloud_directory,0x27u,cloud_collect,0xf00u);
    /* When enumeration reaches EOF, the callbacks have already delivered
     * the records. Stock directory callers accept this terminal status. */
    if (status!=CLOUD_FILE_OK && status!=0x043a0084u)
        return 0;

    char path[sizeof(cloud_directory)+CLOUD_FILENAME-1u];
    z72_style_copy(path,cloud_directory,sizeof(cloud_directory)-1u);
    for (u32 i=0; i<cloud_file_count && limit; ++i) {
        z72_style_copy(path+sizeof(cloud_directory)-1u,cloud_filenames[i],CLOUD_FILENAME);
        limit-=add(path)!=0;
    }
    return 1;
}

#endif
