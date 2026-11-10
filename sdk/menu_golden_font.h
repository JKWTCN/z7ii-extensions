/* SPDX-License-Identifier: MIT */
#ifndef Z72_MENU_GOLDEN_FONT_H
#define Z72_MENU_GOLDEN_FONT_H
#include "z72.h"

/* C 1.70 private font with five CJK glyphs and the missing-glyph outline. */
enum {
    Z72_GOLDEN_FACE=24, Z72_GOLDEN_STYLE=18,
    Z72_GOLDEN_GLYPHS=6, Z72_GOLDEN_HEADER=0x40,
    Z72_GOLDEN_INDEX=Z72_GOLDEN_HEADER+8+Z72_GOLDEN_GLYPHS*6,
    Z72_GOLDEN_OUTLINES=Z72_GOLDEN_INDEX+10+(Z72_GOLDEN_GLYPHS+1)*2,
    Z72_GOLDEN_FONT_BYTES=Z72_GOLDEN_OUTLINES+1700
};
#define Z72_MENU_FONT_ROOTS ((volatile z72_u32 *)0xb1d60b58u)
#define Z72_MENU_STYLE_CLASSES (*(volatile z72_u32 *)0xb1d60b7cu)

struct z72_golden_font {
    const unsigned char *faces[Z72_GOLDEN_FACE+1];
    z72_u32 sizes[Z72_GOLDEN_FACE+1];
    const unsigned char *groups[Z72_GOLDEN_STYLE+1];
    unsigned char classes[Z72_GOLDEN_STYLE+1];
    unsigned char group[36][8];
    _Alignas(4) unsigned char font[Z72_GOLDEN_FONT_BYTES];
};

static inline void z72_font_write32(unsigned char *at,z72_u32 value)
{
    for (z72_u32 i=0;i<4;++i) at[i]=(unsigned char)(value>>(8*i));
}

static inline void z72_font_write16(unsigned char *at,z72_u32 value)
{
    at[0]=(unsigned char)value;
    at[1]=(unsigned char)(value>>8);
}

static inline int z72_golden_font_ready(void)
{
    const volatile z72_u32 *roots=Z72_MENU_FONT_ROOTS;
    return roots[0]==0x10c0bd80u && roots[1]==0x10c0bde0u && roots[2]==0x10947290u &&
           Z72_MENU_STYLE_CLASSES==0x109472d8u;
}

static inline void z72_golden_font_prepare(struct z72_golden_font *font,
                                          void *(*copy)(void *,const void *,z72_u32),
                                          z72_u32 style)
{
    const unsigned char *const *faces=(const unsigned char *const *)0x10c0bd80u;
    const z72_u32 *sizes=(const z72_u32 *)0x10c0bde0u;
    const unsigned char *const *groups=(const unsigned char *const *)0x10947290u;
    const unsigned char *classes=(const unsigned char *)0x109472d8u;
    copy(font->faces,faces,Z72_GOLDEN_FACE*sizeof(*faces));
    copy(font->sizes,sizes,Z72_GOLDEN_FACE*sizeof(*sizes));
    copy(font->groups,groups,Z72_GOLDEN_STYLE*sizeof(*groups));
    copy(font->classes,classes,Z72_GOLDEN_STYLE);
    copy(font->group,groups[style],sizeof(font->group));
    font->group[8][0]=font->group[9][0]=Z72_GOLDEN_FACE;
    font->classes[Z72_GOLDEN_STYLE]=classes[style];
    font->groups[Z72_GOLDEN_STYLE]=font->group[0];
    font->faces[Z72_GOLDEN_FACE]=font->font;
    font->sizes[Z72_GOLDEN_FACE]=sizeof(font->font);

    /* Copy the encoded outlines from the stock faces that actually draw them.
     * A glyph listed in a fallback map may have no outline in that face. */
    static const struct {
        unsigned short character,bytes;
        z72_u32 address;
    } glyphs[Z72_GOLDEN_GLYPHS]={
        {0x0000,207,0x109e2fe2u+0x10a52u}, /* .notdef */
        {0x5206,252,0x109e2fe2u+0x1ee35u}, /* 分 */
        {0x5272,277,0x10b664adu+0x188d2u}, /* 割 */
        {0x91d1,303,0x10972c2cu+0x26691u}, /* 金 */
        {0x9ec3,349,0x10972c2cu+0x3ef35u}, /* 黃 */
        {0x9ec4,312,0x109e2fe2u+0x2444au}  /* 黄 */
    };
    unsigned char *data=font->font;
    copy(data,(const void *)0x109e2fe2u,Z72_GOLDEN_HEADER);
    z72_font_write32(data+0x28,Z72_GOLDEN_HEADER);
    z72_font_write32(data+0x2c,Z72_GOLDEN_INDEX);
    z72_font_write32(data+0x30,Z72_GOLDEN_OUTLINES);
    for (z72_u32 field=0x34;field<Z72_GOLDEN_HEADER;field+=4)
        z72_font_write32(data+field,0);

    /* One Unicode map and one continuous run of outline offsets. */
    unsigned char *map=data+Z72_GOLDEN_HEADER;
    unsigned char *index=data+Z72_GOLDEN_INDEX;
    map[0]=1;
    map[1]=0;
    z72_font_write32(map+2,6);
    z72_font_write16(map+6,Z72_GOLDEN_GLYPHS);
    z72_font_write16(index,1);
    z72_font_write16(index+2,0);
    z72_font_write32(index+4,0);
    z72_font_write16(index+8,Z72_GOLDEN_GLYPHS+1);
    z72_u32 offset=0;
    for (z72_u32 i=0;i<Z72_GOLDEN_GLYPHS;++i) {
        unsigned char *entry=map+8+i*6;
        z72_font_write16(entry,glyphs[i].character);
        z72_font_write16(entry+2,i);
        z72_font_write16(entry+4,i!=0);
        z72_font_write16(index+10+i*2,offset);
        copy(data+Z72_GOLDEN_OUTLINES+offset,(const void *)glyphs[i].address,glyphs[i].bytes);
        offset+=glyphs[i].bytes;
    }
    z72_font_write16(index+10+Z72_GOLDEN_GLYPHS*2,offset);
}

static inline void z72_golden_font_publish(const struct z72_golden_font *font)
{
    volatile z72_u32 *roots=Z72_MENU_FONT_ROOTS;
    roots[0]=(z72_u32)font->faces;
    roots[1]=(z72_u32)font->sizes;
    roots[2]=(z72_u32)font->groups;
    Z72_MENU_STYLE_CLASSES=(z72_u32)font->classes;
}
#endif
