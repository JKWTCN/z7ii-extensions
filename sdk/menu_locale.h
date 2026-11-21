/* SPDX-License-Identifier: MIT */
#ifndef Z72_MENU_LOCALE_H
#define Z72_MENU_LOCALE_H
#include "menu_golden_font.h"
#ifdef Z72_FIRMWARE_INFO
#include "version.h"
#define Z72_FIRMWARE_INFO_TEXT \
    u"" Z72_MODULE_VERSION,u"Z7II Extensions",u"github.com/JKWTCN/z7ii-extensions",u"Nikon Z 7II C 1.70"
#define Z72_MENU_TEXT_UNITS 34u
#define Z72_MENU_LATIN_STYLE 4u /* Stock Latin face for all language banks. */
#else
#define Z72_MENU_TEXT_UNITS 26u
#endif

#define Z72_MENU_LANGUAGES 36u
#define Z72_MENU_STOCK_TEXTS 0x9e4u
#ifdef Z72_FTP_FULL
#define Z72_FTP_MENU_ROWS 8u
#define Z72_FTP_LABEL_UNITS 48u
#define Z72_FTP_PHOTOS_PER_LIST 64u
#define Z72_FTP_FILE_ROWS (Z72_FTP_PHOTOS_PER_LIST+2u)
#define Z72_FTP_FILE_LABEL_UNITS 26u
#endif
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
    Z72_TEXT_DISP_FOCAL,
    Z72_TEXT_BATTERY_PERCENT,
    Z72_TEXT_WIFI_REGION,
    Z72_TEXT_WIFI_CN,
    Z72_TEXT_WIFI_US,
    Z72_TEXT_WIFI_BODY,
    Z72_TEXT_WIFI_80MHZ,
    Z72_TEXT_WIFI_SETTINGS,
    Z72_TEXT_PRECAP_TITLE,
    Z72_TEXT_PRECAP_SWITCH,
    Z72_TEXT_PRECAP_TIME,
    Z72_TEXT_PRECAP_START,
    Z72_TEXT_PRECAP_ON,
    Z72_TEXT_PRECAP_OFF,
    Z72_TEXT_PRECAP_SW1,
    Z72_TEXT_PRECAP_AFON,
    Z72_TEXT_PRECAP_EITHER,
    Z72_TEXT_PRECAP_HINT1,
    Z72_TEXT_PRECAP_HINT2,
    Z72_TEXT_PRECAP_03,
    Z72_TEXT_PRECAP_05,
    Z72_TEXT_PRECAP_07,
    Z72_TEXT_PRECAP_10,
#ifdef Z72_MENU_SOUND_FONT
    Z72_TEXT_SOUND_TITLE,
    Z72_TEXT_SOUND_FILE,
    Z72_TEXT_SOUND_BURST,
    Z72_TEXT_SOUND_VOLUME,
    Z72_TEXT_SOUND_PREVIEW,
    Z72_TEXT_SOUND_WAV1,
    Z72_TEXT_SOUND_WAV2,
    Z72_TEXT_SOUND_WAV3,
    Z72_TEXT_SOUND_WAV4,
    Z72_TEXT_SOUND_WAV5,
    Z72_TEXT_SOUND_DISABLE,
    Z72_TEXT_SOUND_FIRST,
    Z72_TEXT_SOUND_EVERY,
    Z72_TEXT_SOUND_LEVEL1,
    Z72_TEXT_SOUND_LEVEL2,
    Z72_TEXT_SOUND_LEVEL3,
    Z72_TEXT_SOUND_LEVEL4,
    Z72_TEXT_SOUND_LEVEL5,
    Z72_TEXT_SOUND_MISSING1,
    Z72_TEXT_SOUND_MISSING2,
    Z72_TEXT_SOUND_MISSING3,
    Z72_TEXT_SOUND_MISSING4,
    Z72_TEXT_SOUND_MISSING5,
#endif
#ifdef Z72_FILTERED_PLAYBACK
    TEXT_FILTER,
    TEXT_CONDITIONS,
    TEXT_PROTECTION,
    TEXT_MEDIA,
    TEXT_DATE,
    TEXT_ANY,
    TEXT_ALL,
    TEXT_PROTECTED,
    TEXT_UNPROTECTED,
    TEXT_PHOTOS,
    TEXT_UNRATED,
    TEXT_ON,
    TEXT_OFF,
#endif
#ifdef Z72_WIDE_AF
    Z72_TEXT_WIDE_TITLE,
    Z72_TEXT_WIDE_C1,
    Z72_TEXT_WIDE_C2,
    Z72_TEXT_WIDE_SUBJECT,
    Z72_TEXT_WIDE_PEOPLE,
    Z72_TEXT_WIDE_ANIMALS,
    Z72_TEXT_WIDE_OFF,
    Z72_TEXT_WIDE_SIZE,
    Z72_TEXT_WIDE_SAVE,
    Z72_TEXT_WIDE_TILE_C1,
    Z72_TEXT_WIDE_TILE_C2,
#endif
#ifdef Z72_SUBJECT_TRACKING
    Z72_TEXT_SUBJECT_TRACKING,
    Z72_TEXT_AF_IMPROVEMENTS,
    Z72_TEXT_AF_REACQUIRE,
#endif
#ifdef Z72_PIXEL_SHIFT
    Z72_TEXT_PIXEL_TITLE,Z72_TEXT_PIXEL_MODE,Z72_TEXT_PIXEL_COUNT,Z72_TEXT_PIXEL_DELAY,Z72_TEXT_PIXEL_GAP,
    Z72_TEXT_PIXEL_REPEAT,Z72_TEXT_PIXEL_ONE,Z72_TEXT_PIXEL_OFF,
    Z72_TEXT_PIXEL_4,Z72_TEXT_PIXEL_8,Z72_TEXT_PIXEL_16,Z72_TEXT_PIXEL_32,
    Z72_TEXT_PIXEL_0S,Z72_TEXT_PIXEL_1S,Z72_TEXT_PIXEL_2S,Z72_TEXT_PIXEL_3S,Z72_TEXT_PIXEL_5S,Z72_TEXT_PIXEL_10S,
#endif
#ifdef Z72_FTP_FULL
    Z72_TEXT_FTP_MARK,
    Z72_TEXT_FTP_STATUS,
    Z72_TEXT_FTP_EDIT,
    Z72_TEXT_FTP_AUTO,
    Z72_TEXT_FTP_FORMAT,
    Z72_TEXT_FTP_OVERWRITE,
    Z72_TEXT_FTP_4M_BUFFER,
#endif
#ifdef Z72_FIRMWARE_INFO
    Z72_TEXT_FIRMWARE_INFO,
    Z72_TEXT_FIRMWARE_INFO_VERSION,
    Z72_TEXT_FIRMWARE_INFO_PROJECT,
    Z72_TEXT_FIRMWARE_INFO_URL,
    Z72_TEXT_FIRMWARE_INFO_TARGET,
#endif
#ifdef Z72_FTP_FULL
    Z72_TEXT_FTP_ROW,
    Z72_TEXT_FTP_LAST_ROW=Z72_TEXT_FTP_ROW+Z72_FTP_FILE_ROWS-1,
#endif
    Z72_MENU_TEXT_END
};

#ifdef Z72_FTP_FULL
#define Z72_MENU_STATIC_TEXT_END Z72_TEXT_FTP_ROW
#else
#define Z72_MENU_STATIC_TEXT_END Z72_MENU_TEXT_END
#endif

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

#ifdef Z72_FTP_FULL
static struct z72_menu_locale *z72_ftp_locale;
#endif

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
    static const unsigned short translations[][Z72_MENU_STATIC_TEXT_END-Z72_MENU_STOCK_TEXTS][Z72_MENU_TEXT_UNITS]={
        {u"Golden ratio",u"Auto rotate info",u"Photo shooting highlights",u"Highlight threshold",u"Display pattern",
         u"Custom DISP pages",u"Custom pages",u"Standard",u"Minimal",u"Composition",u"Exposure",
         u"Page enabled",u"Shooting info",u"Framing grid",u"Histogram",u"Virtual horizon",
         u"Photo highlights",u"Reset preset",u"Enable",u"Disable",u"Information display",
         u"Shutter speed",u"Aperture",u"ISO sensitivity",u"Shooting mode",u"Release mode",
         u"Focus information",u"Other information",u"Focal length display",u"Battery percentage display",
         u"Wi-Fi region",u"Mainland China",u"United States",u"Follow camera",u"Enable 80 MHz WiFi",u"Wi-Fi settings",
         u"JPEG pre-capture",u"Pre-capture switch",u"Pre-capture time",u"Start pre-capture",
         u"Enable",u"Disable",u"Half-press only",u"AF-ON only",u"Half-press or AF-ON",
         u"Silent photography and H+",u"release mode required",u"0.3 s",u"0.5 s",u"0.7 s",u"1.0 s",
#ifdef Z72_MENU_SOUND_FONT
         u"Silent shutter sound",u"Custom shutter sound",u"Continuous release",u"Volume",u"Preview current sound",
         u"SHUTTER1.WAV",u"SHUTTER2.WAV",u"SHUTTER3.WAV",u"SHUTTER4.WAV",u"SHUTTER5.WAV",u"Off",
         u"First shot only",u"Every shot",u"1",u"2",u"3",u"4",u"5",
         u"SHUTTER1.WAV (unavailable)",u"SHUTTER2.WAV (unavailable)",u"SHUTTER3.WAV (unavailable)",
         u"SHUTTER4.WAV (unavailable)",u"SHUTTER5.WAV (unavailable)",
#endif
#ifdef Z72_FILTERED_PLAYBACK
         u"Filtered playback",u"Filtered playback criteria",u"Protection status",u"Image type",
         u"Shooting date",u"Any",u"All",u"Protected only",u"Unprotected only",u"Photos only",
         u"Unrated",u"On",u"Off",
#endif
#ifdef Z72_WIDE_AF
         u"Custom wide-area AF",u"Wide-area AF (C1)",u"Wide-area AF (C2)",u"Subject detection",u"People",u"Animals",u"Off",
         u"Focus area size",u"MENU Done",u"C1",u"C2",
#endif
#ifdef Z72_SUBJECT_TRACKING
         u"Subject tracking",u"Focus improvements",u"Reacquire after occlusion",
#endif
#ifdef Z72_PIXEL_SHIFT
         u"Pixel shift shooting",u"Pixel shift mode",u"Number of shots",u"Start delay",u"Shot interval",
         u"On (continuous)",u"On (single set)",u"Off",u"4 shots",u"8 shots",u"16 shots",u"32 shots",
         u"0 s",u"1 s",u"2 s",u"3 s",u"5 s",u"10 s",
#endif
#ifdef Z72_FTP_FULL
         u"Mark/unmark for FTP",u"Transfer status",u"Edit FTP profile",
         u"Auto send",u"Send file as",u"Overwrite if same name",u"Use 4 MiB buffer",
#endif
#ifdef Z72_FIRMWARE_INFO
         u"Custom firmware version",Z72_FIRMWARE_INFO_TEXT,
#endif
        },
        {u"黄金分割",u"自动旋转信息显示",u"照片拍摄加亮显示",u"加亮阈值",u"显示图案",
         u"自定义DISP页面",u"自定义页面",u"标准",u"极简",u"构图",u"曝光",u"页面启用",u"拍摄信息",
         u"取景网格",u"直方图",u"水平仪",u"照片加亮",u"恢复预设",u"启用",u"禁用",u"参数信息页",
         u"快门速度",u"光圈",u"ISO感光度",u"拍摄模式",u"释放模式",u"对焦信息",u"其他信息",u"焦距显示",u"电量百分比显示",
         u"Wi-Fi 地区",u"中国大陆",u"美国",u"跟随机身",u"开启 80 MHz WiFi",u"Wi-Fi 设置",
         u"JPEG 预拍摄",u"预拍摄开关",u"预拍摄时间",u"预拍摄启动方式",
         u"开启",u"关闭",u"仅半按快门",u"仅 AF-ON",u"半按快门或 AF-ON",
         u"只有在静音拍摄和 H+",u"连拍模式下生效",u"0.3 s",u"0.5 s",u"0.7 s",u"1.0 s",
#ifdef Z72_MENU_SOUND_FONT
         u"静音拍摄快门声音",u"自定义快门声",u"连拍时播放",u"音量大小",u"试听当前音效",
         u"SHUTTER1.WAV",u"SHUTTER2.WAV",u"SHUTTER3.WAV",u"SHUTTER4.WAV",u"SHUTTER5.WAV",u"关闭",
         u"仅第一张",u"每张照片",u"1",u"2",u"3",u"4",u"5",
         u"SHUTTER1.WAV（不可用）",u"SHUTTER2.WAV（不可用）",u"SHUTTER3.WAV（不可用）",
         u"SHUTTER4.WAV（不可用）",u"SHUTTER5.WAV（不可用）",
#endif
#ifdef Z72_FILTERED_PLAYBACK
         u"筛选的播放",u"筛选的播放条件",u"保护状态",u"影像类型",
         u"拍摄日期",u"不限",u"全部",u"仅已保护",u"仅未保护",u"仅照片",
         u"无评级",u"开启",u"关闭",
#endif
#ifdef Z72_WIDE_AF
         u"自定义宽区域 AF",u"宽区域 AF（C1）",u"宽区域 AF（C2）",u"主体识别",u"人物",u"动物",u"关闭",
         u"对焦区域大小",u"MENU 完成",u"C1",u"C2",
#endif
#ifdef Z72_SUBJECT_TRACKING
         u"主体跟踪",u"对焦改进",u"遮挡后找回主体",
#endif
#ifdef Z72_PIXEL_SHIFT
         u"像素变化拍摄",u"像素变化拍摄模式",u"拍摄张数",u"开始拍摄延迟",u"拍摄间隔",
         u"开启（连续）",u"开启（单组）",u"关闭",u"4 张",u"8 张",u"16 张",u"32 张",
         u"0 秒",u"1 秒",u"2 秒",u"3 秒",u"5 秒",u"10 秒",
#endif
#ifdef Z72_FTP_FULL
         u"选择以FTP发送/取消",u"传输状态",u"编辑 FTP 配置",
         u"自动发送",u"传送文件为",u"同名时重写",u"使用 4 MiB 内存",
#endif
#ifdef Z72_FIRMWARE_INFO
         u"自定义固件版本",Z72_FIRMWARE_INFO_TEXT,
#endif
        },
        {u"黃金分割",u"自動旋轉資訊顯示",u"照片拍攝加亮顯示",u"加亮閾值",u"顯示圖案",
         u"自訂DISP頁面",u"自訂頁面",u"標準",u"精簡",u"構圖",u"曝光",u"頁面啟用",u"拍攝資訊",
         u"取景網格",u"直方圖",u"水平儀",u"照片加亮",u"恢復預設",u"啟用",u"停用",u"參數資訊頁",
         u"快門速度",u"光圈",u"ISO感光度",u"拍攝模式",u"釋放模式",u"對焦資訊",u"其他資訊",u"焦距顯示",u"電量百分比顯示",
         u"Wi-Fi 地區",u"中國大陸",u"美國",u"跟隨機身",u"開啟 80 MHz WiFi",u"Wi-Fi 設定",
         u"JPEG 預拍攝",u"預拍攝開關",u"預拍攝時間",u"預拍攝啟動方式",
         u"開啟",u"關閉",u"僅半按快門",u"僅 AF-ON",u"半按快門或 AF-ON",
         u"僅在靜音拍攝和 H+",u"連拍模式下生效",u"0.3 s",u"0.5 s",u"0.7 s",u"1.0 s",
#ifdef Z72_MENU_SOUND_FONT
         u"靜音拍攝快門聲音",u"自訂快門聲",u"連拍時播放",u"音量大小",u"試聽目前音效",
         u"SHUTTER1.WAV",u"SHUTTER2.WAV",u"SHUTTER3.WAV",u"SHUTTER4.WAV",u"SHUTTER5.WAV",u"關閉",
         u"僅第一張",u"每張照片",u"1",u"2",u"3",u"4",u"5",
         u"SHUTTER1.WAV（無法使用）",u"SHUTTER2.WAV（無法使用）",u"SHUTTER3.WAV（無法使用）",
         u"SHUTTER4.WAV（無法使用）",u"SHUTTER5.WAV（無法使用）",
#endif
#ifdef Z72_FILTERED_PLAYBACK
         /* Filtered playback resolves its labels from the English or Simplified Chinese bank. */
         [TEXT_OFF-Z72_MENU_STOCK_TEXTS]=u"",
#endif
#ifdef Z72_WIDE_AF
         u"自訂寬區域 AF",u"寬區域 AF（C1）",u"寬區域 AF（C2）",u"主體偵測",u"人物",u"動物",u"關閉",
         u"對焦區域大小",u"MENU 完成",u"C1",u"C2",
#endif
#ifdef Z72_SUBJECT_TRACKING
         u"主體追蹤",u"對焦改進",u"遮擋後找回主體",
#endif
#ifdef Z72_PIXEL_SHIFT
         u"像素變化拍攝",u"像素變化拍攝模式",u"拍攝張數",u"開始拍攝延遲",u"拍攝間隔",
         u"開啟（連續）",u"開啟（單組）",u"關閉",u"4 張",u"8 張",u"16 張",u"32 張",
         u"0 秒",u"1 秒",u"2 秒",u"3 秒",u"5 秒",u"10 秒",
#endif
#ifdef Z72_FTP_FULL
         u"選擇以FTP傳送/取消",u"傳輸狀態",u"編輯 FTP 設定",
         u"自動傳送",u"傳送檔案為",u"同名時覆寫",u"使用 4 MiB 記憶體",
#endif
#ifdef Z72_FIRMWARE_INFO
         u"自訂韌體版本",Z72_FIRMWARE_INFO_TEXT,
#endif
        }
    };
    z72_u32 bytes=sizeof(struct z72_menu_locale)+Z72_MENU_LANGUAGES*sizeof(translations[0]);
#ifdef Z72_FTP_FULL
    _Static_assert(Z72_FTP_FILE_ROWS*Z72_FTP_FILE_LABEL_UNITS>=Z72_FTP_MENU_ROWS*Z72_FTP_LABEL_UNITS,
                   "FTP label buffer must hold either list");
    const z72_u32 ftp_label_bytes=Z72_FTP_FILE_ROWS*Z72_FTP_FILE_LABEL_UNITS*sizeof(unsigned short);
    bytes+=Z72_MENU_LANGUAGES*ftp_label_bytes;
#endif
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
    locale->styles[Z72_TEXT_BATTERY_PERCENT]=Z72_GOLDEN_STYLE;
#ifdef Z72_MENU_SOUND_FONT
    locale->styles[Z72_TEXT_SOUND_PREVIEW]=Z72_GOLDEN_STYLE;
#endif
    for (z72_u32 id=Z72_TEXT_WIFI_CN;id<=Z72_TEXT_WIFI_BODY;++id)
        locale->styles[id]=Z72_GOLDEN_STYLE;
#ifdef Z72_FILTERED_PLAYBACK
    for (z72_u32 id=TEXT_FILTER;id<=TEXT_OFF;++id)
        locale->styles[id]=id<=TEXT_CONDITIONS ? Z72_PLAYBACK_TITLE_STYLE : Z72_PLAYBACK_ROW_STYLE;
#endif
#ifdef Z72_FIRMWARE_INFO
    for (z72_u32 id=Z72_TEXT_FIRMWARE_INFO_VERSION;id<=Z72_TEXT_FIRMWARE_INFO_TARGET;++id)
        locale->styles[id]=Z72_MENU_LATIN_STYLE;
#endif
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
        for (z72_u32 id=Z72_MENU_STOCK_TEXTS;id<Z72_MENU_STATIC_TEXT_END;++id) {
            z72_u32 label_language=translation,attribute_template=template;
#ifdef Z72_FILTERED_PLAYBACK
            if (id>=TEXT_FILTER && id<=TEXT_OFF) {
                label_language=lang==8;
                attribute_template=0x367;
            }
#endif
            const unsigned short *label=translations[label_language][id-Z72_MENU_STOCK_TEXTS];
#ifdef Z72_FIRMWARE_INFO
            if (lang==10 && id==Z72_TEXT_FIRMWARE_INFO) label=u"사용자 펌웨어 버전";
#endif
            z72_u32 length=0;
            while (length<sizeof(translations[0][0])/sizeof(*label) && label[length]) ++length;
            length*=sizeof(*label);
            copy(text+end,label,length);
            end+=length;
            locale->offsets[lang][id+1]=end;
            locale->attributes[lang][id]=attrs[lang][attribute_template];
        }
#ifdef Z72_FTP_FULL
        /* Keep the mutable FTP rows at the end of each language text block. */
        for (z72_u32 id=Z72_TEXT_FTP_ROW;id<Z72_MENU_TEXT_END;++id) {
            locale->offsets[lang][id+1]=end;
            locale->attributes[lang][id]=attrs[lang][template];
        }
        end+=ftp_label_bytes;
#endif
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
#ifdef Z72_FTP_FULL
    z72_ftp_locale=locale;
#endif
    return 1;
}
#endif
