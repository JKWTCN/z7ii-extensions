/* SPDX-License-Identifier: MIT */
#ifndef Z72_NP3_CALIBRATION_H
#define Z72_NP3_CALIBRATION_H

#include "flexible_style_tables.h"

#define Z72_CAL_MAGIC 0x3143504eu /* NPC1 */
#define Z72_CAL_MAX_RECORDS 99u
#define Z72_CAL_RGB_LIMIT 512
#define Z72_CAL_CPXS_LIMIT 128

struct z72_np3_calibration {
    uint32_t bytes, hash;
    int16_t rgb[3][9];
    int16_t cpxs[6][4];
    uint16_t reserved;
};
_Static_assert(sizeof(struct z72_np3_calibration)==112,"NPC1 record layout");

struct z72_np3_calibration_file {
    uint32_t magic, count;
    struct z72_np3_calibration records[Z72_CAL_MAX_RECORDS];
};
_Static_assert(offsetof(struct z72_np3_calibration_file,records)==8,"NPC1 header layout");

/* Allow an EOF probe and keep the SD read within aligned cache lines. */
#define Z72_CAL_CAPACITY ((sizeof(struct z72_np3_calibration_file)+4u+64u)&~63u)
union z72_np3_calibration_block {
    _Alignas(64) unsigned char data[Z72_CAL_CAPACITY];
    struct z72_np3_calibration_file file;
};

static inline uint32_t z72_np3_hash(const void *data,unsigned bytes)
{
    const unsigned char *source=data;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<bytes;++i) hash=(hash^source[i])*16777619u;
    return hash;
}

static inline unsigned z72_cal_count(const union z72_np3_calibration_block *block,unsigned bytes)
{
    const struct z72_np3_calibration_file *file=&block->file;
    if(bytes<12u || file->magic!=Z72_CAL_MAGIC || file->count>Z72_CAL_MAX_RECORDS ||
       bytes!=12u+file->count*sizeof(file->records[0]))
        return 0;
    const unsigned char *tail=block->data+bytes-4u;
    uint32_t checksum=(uint32_t)tail[0]|((uint32_t)tail[1]<<8)|
                      ((uint32_t)tail[2]<<16)|((uint32_t)tail[3]<<24);
    if(checksum!=z72_np3_hash(block->data,bytes-4u))
        return 0;
    for(unsigned i=0;i<file->count;++i) {
        const struct z72_np3_calibration *record=&file->records[i];
        if(record->bytes<8u || record->bytes>=4096u || record->reserved)
            return 0;
        for(unsigned prior=0;prior<i;++prior)
            if(file->records[prior].bytes==record->bytes && file->records[prior].hash==record->hash)
                return 0;
        for(unsigned c=0;c<3;++c)
            for(unsigned j=0;j<9;++j)
                if(record->rgb[c][j]<-Z72_CAL_RGB_LIMIT || record->rgb[c][j]>Z72_CAL_RGB_LIMIT)
                    return 0;
        for(unsigned c=0;c<6;++c)
            for(unsigned j=0;j<4;++j)
                if(record->cpxs[c][j]<-Z72_CAL_CPXS_LIMIT || record->cpxs[c][j]>Z72_CAL_CPXS_LIMIT)
                    return 0;
    }
    return file->count;
}

static inline const struct z72_np3_calibration *z72_cal_find(
    const struct z72_np3_calibration_file *file,unsigned count,uint32_t bytes,uint32_t hash)
{
    for(unsigned i=0;i<count;++i)
        if(file->records[i].bytes==bytes && file->records[i].hash==hash)
            return &file->records[i];
    return 0;
}

static inline void z72_cal_apply(struct z72_style_fixed *style,const struct z72_np3_calibration *record)
{
    for(unsigned c=0;c<3;++c) {
        int previous=0;
        for(unsigned i=0;i<129;++i) {
            unsigned j=i/16u;
            int left=record->rgb[c][j], offset=left;
            if(i<128u)
                offset+=(record->rgb[c][j+1u]-left)*(int)(i%16u)/16;
            int value=style->rgb[c][i]+offset;
            previous=value<previous ? previous : value>4095 ? 4095 : value;
            style->rgb[c][i]=(uint16_t)previous;
        }
    }
    for(unsigned c=0;c<6;++c)
        for(unsigned i=0;i<32;++i) {
            unsigned j=i/8u;
            int left=record->cpxs[c][j], right=record->cpxs[c][(j+1u)%4u];
            int value=style->cpxs_delta[c][i]+left+(right-left)*(int)(i%8u)/8;
            style->cpxs_delta[c][i]=(int16_t)(value<-1023 ? -1023 : value>1023 ? 1023 : value);
        }
}

#endif
