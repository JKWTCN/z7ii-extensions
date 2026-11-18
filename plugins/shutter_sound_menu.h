/* SPDX-License-Identifier: MIT */
#ifndef Z72_SHUTTER_SOUND_MENU_H
#define Z72_SHUTTER_SOUND_MENU_H

enum { SOUND_ROW_ID=0x2ec0u,
       ROOT_VIEW=0, FILE_VIEW, BURST_VIEW, VOLUME_VIEW, VIEW_COUNT,
       SOUND_MENU_ROWS=Z72_SOUND_CLIP_COUNT+1, VALUE_ACTION=0xd7u };

static struct {
    z72_u32 header[6],title[24],rows[SOUND_MENU_ROWS][7],maps[SOUND_MENU_ROWS];
    z72_u32 events[SOUND_MENU_ROWS][ROTATION_EVENTS][4];
} sound_views[VIEW_COUNT];
static z72_u32 sound_view,sound_root_row,sound_root_layout[4],sound_callbacks[5];

static u32 sound_header_owned(u32 header)
{
    for (u32 i=0;i<VIEW_COUNT;++i) if (header==(u32)sound_views[i].header) return 1;
    return 0;
}

static void sound_page_init(const z72_u32 *page)
{
    z72_u32 row=sound_root_row;
    if (sound_view==FILE_VIEW) {
        z72_u32 choice=__atomic_load_n(&sound_choice,__ATOMIC_ACQUIRE);
        row=choice ? choice-1 : Z72_SOUND_CLIP_COUNT;
    } else if (sound_view==BURST_VIEW) row=__atomic_load_n(&sound_every_frame,__ATOMIC_ACQUIRE);
    else if (sound_view==VOLUME_VIEW) row=__atomic_load_n(&sound_volume,__ATOMIC_ACQUIRE)-1;
    z72_u32 id=SOUND_ROW_ID+sound_view*8+row;
    if (NATIVE(0x1063bf77u,z72_u32,void)()!=1)
        NATIVE(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
    NATIVE(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,id,1);
}

static z72_u32 sound_show(z72_u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    if (WORD(ROTATION_PAGE)!=2 ||
        (!sound_header_owned(header) && (view || WORD(ROTATION_PAGE+0x24) ||
          (!pre_capture_menu_source(header) && !jpeg_menu_owned(header))))) return 1;
    sound_view=view;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)sound_callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)sound_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    NATIVE(0x1063ab75u,void,z72_u32,z72_u32)(2,0);
    return 0;
}

static z72_u32 open_sound_menu(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    sound_root_row=0;
    return sound_show(ROOT_VIEW);
}

static z72_u32 sound_back(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return sound_show(ROOT_VIEW);
}

static z72_u32 sound_pick(z72_u32 row)
{
    if (sound_view==ROOT_VIEW) {
        if (row==3) { enqueue_sound(1,0,0); return 0; }
        sound_root_row=row;
        return sound_show(FILE_VIEW+row);
    }
    struct z72_config config=z72_config_current;
    switch (sound_view) {
    case FILE_VIEW:
        if (row<Z72_SOUND_CLIP_COUNT && !clip_bytes[row]) return 0;
        config.sound_choice=row==Z72_SOUND_CLIP_COUNT ? 0 : row+1;
        __atomic_store_n(&sound_choice,config.sound_choice,__ATOMIC_RELEASE);
        sound_views[ROOT_VIEW].rows[0][5]=rotation_value(config.sound_choice);
        break;
    case BURST_VIEW:
        config.sound_every_frame=row;
        __atomic_store_n(&sound_every_frame,config.sound_every_frame,__ATOMIC_RELEASE);
        break;
    case VOLUME_VIEW:
        config.sound_volume=row+1;
        __atomic_store_n(&sound_volume,config.sound_volume,__ATOMIC_RELEASE);
        break;
    }
    z72_config_save(config);
    return sound_show(ROOT_VIEW);
}

#define SOUND_PICK(row) \
    static z72_u32 sound_pick##row(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; return sound_pick(row); }
SOUND_PICK(0)
SOUND_PICK(1)
SOUND_PICK(2)
SOUND_PICK(3)
SOUND_PICK(4)
SOUND_PICK(5)
#undef SOUND_PICK

static z72_u32 sound_menu_prepare(void)
{
    const z72_u32 *setup=(const z72_u32 *)Z72_SETUP_MENU;
    const z72_u32 *choice=(const z72_u32 *)0xb18545fcu;
    /* portrait_install_menu has validated the shared page and choice template. */
    if (setup[1]!=0x1187c3d0u || WORD(0x1187c550u)!=6 || WORD(0x1187c540u)!=5) return 1;
    const z72_u32 *events=(const z72_u32 *)WORD(choice[4]);
    if (events[(ROTATION_EVENTS-1)*4]!=0x31au) return 1;
    sound_load_clips();
    z72_u32 choice_saved=z72_config_current.sound_choice;
    sound_choice=choice_saved && clip_bytes[choice_saved-1] ? choice_saved : 0;
    sound_every_frame=z72_config_current.sound_every_frame;
    sound_volume=z72_config_current.sound_volume;

    const z72_u32 counts[]={4,SOUND_MENU_ROWS,2,Z72_SOUND_VOLUME_MAX};
    const z72_u32 title_ids[]={Z72_TEXT_SOUND_TITLE,Z72_TEXT_SOUND_FILE,Z72_TEXT_SOUND_BURST,Z72_TEXT_SOUND_VOLUME};
    const z72_u32 label_ids[]={Z72_TEXT_SOUND_FILE,Z72_TEXT_SOUND_WAV1,Z72_TEXT_SOUND_FIRST,Z72_TEXT_SOUND_LEVEL1};
    const z72_u32 layouts[]={(z72_u32)sound_root_layout,0x1187c550u,0x1187c500u,0x1187c540u};
    const z72_menu_callback actions[]={sound_pick0,sound_pick1,sound_pick2,sound_pick3,sound_pick4,sound_pick5};
    sound_root_layout[0]=counts[ROOT_VIEW];
    for (z72_u32 i=1;i<4;++i) sound_root_layout[i]=WORD(setup[1]+i*4);
    for (z72_u32 view=0;view<VIEW_COUNT;++view) {
        z72_u32 *header=sound_views[view].header,*title=sound_views[view].title;
        const z72_u32 *template=view==ROOT_VIEW ? setup : choice;
        for (z72_u32 i=0;i<6;++i) header[i]=template[i];
        header[1]=layouts[view];
        for (z72_u32 i=0;i<24;++i) title[i]=WORD(choice[2]+i*4);
        title[0]=title_ids[view];
        title[1]=1;
        header[2]=(z72_u32)title;
        header[3]=(z72_u32)sound_views[view].rows;
        header[4]=(z72_u32)sound_views[view].maps;
        header[5]=(counts[view]<<16)|counts[view];
        for (z72_u32 row=0;row<counts[view];++row) {
            z72_u32 *item=sound_views[view].rows[row];
            for (z72_u32 i=0;i<7;++i) item[i]=WORD(template[3]+i*4);
            item[0]=view==FILE_VIEW && row<Z72_SOUND_CLIP_COUNT && !clip_bytes[row] ?
                Z72_TEXT_SOUND_MISSING1+row : label_ids[view]+row;
            item[1]=item[2]=0xffffu;
            item[3]=0;
            item[4]=view==ROOT_VIEW ? 0x1001u : 0x1000u;
            item[5]=view==ROOT_VIEW ? (row ? VALUE_ACTION : rotation_value(sound_choice)) : 0xffffu;
            item[6]=SOUND_ROW_ID+view*8+row;
            for (z72_u32 i=0;i<ROTATION_EVENTS;++i) {
                z72_u32 *map=sound_views[view].events[row][i];
                for (z72_u32 j=0;j<4;++j) map[j]=events[i*4+j];
                if (map[0]==0x257 || map[0]==0x25c || map[0]==0x27d || map[0]==0x252) {
                    map[1]=(z72_u32)actions[row]; map[2]=0; map[3]=0x430;
                }
                if (map[0]==0x256 || map[0]==0x271 || map[0]==0x2cf || map[0]==0x27f) {
                    map[1]=view==ROOT_VIEW ? 0 : (z72_u32)sound_back; map[2]=0;
                    map[3]=view==ROOT_VIEW ? 0x431 : 0x430;
                }
            }
            sound_views[view].maps[row]=(z72_u32)sound_views[view].events[row];
        }
    }

    for (z72_u32 i=1;i<5;++i) sound_callbacks[i]=rotation_menu.callbacks[i];
    sound_callbacks[0]=(z72_u32)sound_page_init;
    return 0;
}
#endif
