/* SPDX-License-Identifier: MIT */
#ifndef Z72_MENU_LOCALE_H
#define Z72_MENU_LOCALE_H
#include "menu_golden_font.h"

#define Z72_MENU_LANGUAGES 36u
#define Z72_MENU_STOCK_TEXTS 0x9e4u
enum {
    Z72_TEXT_GRID_GOLDEN=Z72_MENU_STOCK_TEXTS,
    Z72_TEXT_AUTO_ROTATE,
    Z72_TEXT_PHOTO_ZEBRA,
    Z72_TEXT_PHOTO_THRESHOLD,
    Z72_TEXT_PHOTO_PATTERN,
    Z72_TEXT_DISP_TITLE,
    Z72_TEXT_DISP_MASTER,
    Z72_TEXT_DISP_STANDARD,
    Z72_TEXT_DISP_MINIMAL,
    Z72_TEXT_DISP_COMPOSITION,
    Z72_TEXT_DISP_EXPOSURE,
    Z72_TEXT_DISP_PAGE_ENABLED,
    Z72_TEXT_DISP_SHOOTING_INFO,
    Z72_TEXT_DISP_FRAMING_GRID,
    Z72_TEXT_DISP_HISTOGRAM,
    Z72_TEXT_DISP_HORIZON,
    Z72_TEXT_DISP_PHOTO_HIGHLIGHTS,
    Z72_TEXT_DISP_RESET,
    Z72_TEXT_DISP_ENABLE,
    Z72_TEXT_DISP_DISABLE,
    Z72_TEXT_DISP_INFORMATION,
    Z72_TEXT_DISP_SHUTTER,
    Z72_TEXT_DISP_APERTURE,
    Z72_TEXT_DISP_ISO,
    Z72_TEXT_DISP_SHOOTING,
    Z72_TEXT_DISP_RELEASE,
    Z72_TEXT_DISP_FOCUS,
    Z72_TEXT_DISP_STATUS,
    Z72_MENU_TEXT_END
};

struct z72_menu_locale {
    const unsigned char *texts[Z72_MENU_LANGUAGES];
    const z72_u32 *offset_tables[Z72_MENU_LANGUAGES];
    const unsigned char *attribute_tables[Z72_MENU_LANGUAGES];
    z72_u32 offsets[Z72_MENU_LANGUAGES][Z72_MENU_TEXT_END+1];
    unsigned char attributes[Z72_MENU_LANGUAGES][Z72_MENU_TEXT_END];
    unsigned char styles[Z72_MENU_TEXT_END];
    struct z72_golden_font golden;
    unsigned char text[];
};

/* Register labels before exposing any menu rows that reference them. */
static inline int z72_menu_locale_install(void)
{
    volatile z72_u32 *const tables=(volatile z72_u32 *)0xb1d60b68u;
    const unsigned char *const *const texts=(const unsigned char *const *)0x116bd1c0u;
    const unsigned char *const styles=(const unsigned char *)0x1107cb04u;
    const z72_u32 *const *const offsets=(const z72_u32 *const *)0x116bd130u;
    const unsigned char *const *const attrs=(const unsigned char *const *)0x116bd250u;
    if (tables[0]!=(z72_u32)texts || tables[1]!=(z72_u32)styles ||
        tables[2]!=(z72_u32)offsets || tables[3]!=(z72_u32)attrs ||
        !z72_golden_font_ready()) return 0;

    /* The firmware stores consecutive UTF-16LE strings with byte offsets. */
    static const unsigned short translations[][Z72_MENU_TEXT_END-Z72_MENU_STOCK_TEXTS][26]={
        {u"Golden ratio",u"Auto rotate info",u"Photo shooting highlights",u"Highlight threshold",u"Display pattern",
         u"Custom DISP pages",u"Custom pages",u"Standard",u"Minimal",u"Composition",u"Exposure",
         u"Page enabled",u"Shooting info",u"Framing grid",u"Histogram",u"Virtual horizon",
         u"Photo highlights",u"Reset preset",u"Enable",u"Disable",u"Information display",
         u"Shutter speed",u"Aperture",u"ISO sensitivity",u"Shooting mode",u"Release mode",
         u"Focus information",u"Other information"},
        {u"黄金分割",u"自动旋转信息显示",u"照片拍摄加亮显示",u"加亮阈值",u"显示图案",
         u"自定义DISP页面",u"自定义页面",u"标准",u"极简",u"构图",u"曝光",u"页面启用",u"拍摄信息",
         u"取景网格",u"直方图",u"水平仪",u"照片加亮",u"恢复预设",u"启用",u"禁用",u"参数信息页",
         u"快门速度",u"光圈",u"ISO感光度",u"拍摄模式",u"释放模式",u"对焦信息",u"其他信息"},
        {u"黃金分割",u"自動旋轉資訊顯示",u"照片拍攝加亮顯示",u"加亮閾值",u"顯示圖案",
         u"自訂DISP頁面",u"自訂頁面",u"標準",u"精簡",u"構圖",u"曝光",u"頁面啟用",u"拍攝資訊",
         u"取景網格",u"直方圖",u"水平儀",u"照片加亮",u"恢復預設",u"啟用",u"停用",u"參數資訊頁",
         u"快門速度",u"光圈",u"ISO感光度",u"拍攝模式",u"釋放模式",u"對焦資訊",u"其他資訊"}
    };
    z72_u32 bytes=sizeof(struct z72_menu_locale)+Z72_MENU_LANGUAGES*sizeof(translations[0]);
    for (z72_u32 lang=0;lang<Z72_MENU_LANGUAGES;++lang)
        bytes+=offsets[lang][Z72_MENU_STOCK_TEXTS];
    struct z72_menu_locale *locale=((void *(*)(z72_u32,z72_u32))0x1034fe31u)(bytes,0);
    if (!locale) return 0;

    void *(*copy)(void *,const void *,z72_u32)=(void *)0x10360facu;
    const z72_u32 template=0x2cbu; /* Format memory card, with localized fonts. */
    copy(locale->styles,styles,Z72_MENU_STOCK_TEXTS);
    locale->styles[Z72_TEXT_GRID_GOLDEN]=Z72_GOLDEN_STYLE;
    for (z72_u32 id=Z72_TEXT_AUTO_ROTATE;id<Z72_MENU_TEXT_END;++id)
        locale->styles[id]=styles[template];
    locale->styles[Z72_TEXT_DISP_HORIZON]=Z72_GOLDEN_STYLE;
    z72_golden_font_prepare(&locale->golden,copy,styles[template]);
    unsigned char *text=locale->text;
    for (z72_u32 lang=0;lang<Z72_MENU_LANGUAGES;++lang) {
        z72_u32 translation=lang==8 ? 1u : lang==9 ? 2u : 0u;
        z72_u32 end=offsets[lang][Z72_MENU_STOCK_TEXTS];
        locale->texts[lang]=text;
        locale->offset_tables[lang]=locale->offsets[lang];
        locale->attribute_tables[lang]=locale->attributes[lang];
        copy(text,texts[lang],end);
        copy(locale->offsets[lang],offsets[lang],(Z72_MENU_STOCK_TEXTS+1)*sizeof(z72_u32));
        copy(locale->attributes[lang],attrs[lang],Z72_MENU_STOCK_TEXTS);
        for (z72_u32 id=Z72_MENU_STOCK_TEXTS;id<Z72_MENU_TEXT_END;++id) {
            const unsigned short *label=translations[translation][id-Z72_MENU_STOCK_TEXTS];
            z72_u32 length=0;
            while (length<sizeof(translations[0][0])/sizeof(*label) && label[length]) ++length;
            length*=sizeof(*label);
            copy(text+end,label,length);
            end+=length;
            locale->offsets[lang][id+1]=end;
            locale->attributes[lang][id]=attrs[lang][template];
        }
        text+=end;
    }

    /* Both generations resolve identical data for all stock IDs. */
    __asm__ volatile("dmb sy" ::: "memory");
    z72_golden_font_publish(&locale->golden);
    __asm__ volatile("dmb sy" ::: "memory");
    tables[0]=(z72_u32)locale->texts;
    tables[1]=(z72_u32)locale->styles;
    tables[2]=(z72_u32)locale->offset_tables;
    tables[3]=(z72_u32)locale->attribute_tables;
    __asm__ volatile("dmb sy" ::: "memory");
    return 1;
}
#endif
