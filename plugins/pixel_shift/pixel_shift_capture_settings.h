/* SPDX-License-Identifier: MIT */
enum {
    PIXEL_RAW_COMPRESSION = 0x0506, PIXEL_RAW_DEPTH = 0x0507,
    PIXEL_SILENT_PHOTOGRAPHY = 0x057a, PIXEL_SILENT_ON = 1,
    PIXEL_BRACKETING_TYPE = 0x056f, PIXEL_BRACKETING_FLASH_ONLY = 2,
    PIXEL_BRACKETING_COUNT = 0x0570, PIXEL_BRACKETING_PROGRAM = 0x057d,
    PIXEL_RAW_UNCOMPRESSED = 0, PIXEL_RAW_14_BIT = 1
};

/* Photo shooting menu and PTP use the same native property writer. */
static const struct {
    z72_u32 property, value, maximum;
} pixel_capture_settings[] = {
    {PIXEL_RAW_DEPTH, PIXEL_RAW_14_BIT, 1},
    {PIXEL_RAW_COMPRESSION, PIXEL_RAW_UNCOMPRESSED, 2},
    {PIXEL_SILENT_PHOTOGRAPHY, PIXEL_SILENT_ON, 1},
};
enum { PIXEL_SETTINGS_COUNT = sizeof(pixel_capture_settings)/sizeof(pixel_capture_settings[0]) };
static z72_u32 pixel_settings_saved[PIXEL_SETTINGS_COUNT];
static unsigned pixel_settings_restore_mask;

static z72_u32 pixel_setting_get(z72_u32 property)
{
    return CALL(0x10100491u,z72_u32,z72_u32)(property);
}

static int pixel_setting_set(z72_u32 property,z72_u32 value)
{
    return !CALL(0x106342dfu,z72_u32,z72_u32,z72_u32)(property,value) &&
        pixel_setting_get(property)==value;
}

static int pixel_settings_restore(void)
{
    if (!pixel_settings_restore_mask) return 1;
    if (PIXEL_LOAD(pixel_job)==2 || !pixel_capture_idle()) return 0;
    for (unsigned i=PIXEL_SETTINGS_COUNT;i--;) {
        unsigned bit=1u<<i;
        if (!(pixel_settings_restore_mask&bit)) continue;
        z72_u32 property=pixel_capture_settings[i].property;
        /* Keep any setting that has changed since this group set it. */
        if (pixel_setting_get(property)!=pixel_capture_settings[i].value ||
            pixel_setting_set(property,pixel_settings_saved[i]))
            pixel_settings_restore_mask&=~bit;
    }
    return !pixel_settings_restore_mask;
}

static int pixel_settings_prepare(void)
{
    if (!pixel_settings_restore()) return 0;
    /* Stock silent-photography transitions clear a flash-only bracket. */
    if (pixel_setting_get(PIXEL_SILENT_PHOTOGRAPHY)!=PIXEL_SILENT_ON &&
        pixel_setting_get(PIXEL_BRACKETING_TYPE)==PIXEL_BRACKETING_FLASH_ONLY &&
        (pixel_setting_get(PIXEL_BRACKETING_COUNT) || pixel_setting_get(PIXEL_BRACKETING_PROGRAM)))
        return 0;
    for (unsigned i=0;i<PIXEL_SETTINGS_COUNT;++i) {
        pixel_settings_saved[i]=pixel_setting_get(pixel_capture_settings[i].property);
        if (pixel_settings_saved[i]>pixel_capture_settings[i].maximum) return 0;
    }
    for (unsigned i=0;i<PIXEL_SETTINGS_COUNT;++i) {
        z72_u32 value=pixel_capture_settings[i].value;
        if (pixel_settings_saved[i]==value) continue;
        pixel_settings_restore_mask|=1u<<i;
        if (!pixel_setting_set(pixel_capture_settings[i].property,value)) return 0;
    }
    return 1;
}

static int pixel_settings_match(void)
{
    for (unsigned i=0;i<PIXEL_SETTINGS_COUNT;++i)
        if (pixel_setting_get(pixel_capture_settings[i].property)!=pixel_capture_settings[i].value)
            return 0;
    return 1;
}
