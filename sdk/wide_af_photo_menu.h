/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_PHOTO_MENU_H
#define Z72_WIDE_AF_PHOTO_MENU_H

/* C 1.70 photo shooting menu, including the eight-row scrolling layout. */
#define WIDE_PHOTO_PAGE ((volatile z72_u32 *)0xb2bb8eecu)
#define WIDE_PHOTO_HEADER ((const z72_u32 *)0xb185600cu)
#define WIDE_PHOTO_CALLBACKS ((const z72_u32 *)0x10967c84u)
#define WIDE_MENU_CALL(address,type,...) ((type (*)(__VA_ARGS__))(address))

enum {
    WIDE_STOCK_ROWS=10, WIDE_INSERT_ROW=7, WIDE_PHOTO_ROWS=12,
    WIDE_PHOTO_EVENTS=16, WIDE_PHOTO_C1_ID=0x2eb0
};

typedef z72_u32 (*wide_photo_action)(z72_u32,z72_u32,z72_u32,z72_u32);

static const z72_u32 wide_photo_native_confirm[WIDE_STOCK_ROWS]={
    0x106a7b0d,0x106a7b17,0x106a7b21,0x106a7b2b,0x106a7b35,
    0x106a7b4f,0x106a7b69,0x106a7b83,0x106a7b9d,0x106a7bb7
};

static struct {
    z72_u32 header[6],rows[WIDE_PHOTO_ROWS][7],maps[WIDE_PHOTO_ROWS],callbacks[5];
    z72_u32 events[WIDE_PHOTO_ROWS][WIDE_PHOTO_EVENTS][4];
} wide_photo_page;

#define WIDE_PHOTO_STOCK_PICK(row) \
    static z72_u32 wide_photo_stock##row(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { \
        if (wide_af_choose(0)) return 1; \
        return ((wide_photo_action)wide_photo_native_confirm[row])(a,b,c,d); \
    }
WIDE_PHOTO_STOCK_PICK(0)
WIDE_PHOTO_STOCK_PICK(1)
WIDE_PHOTO_STOCK_PICK(2)
WIDE_PHOTO_STOCK_PICK(3)
WIDE_PHOTO_STOCK_PICK(4)
WIDE_PHOTO_STOCK_PICK(5)
WIDE_PHOTO_STOCK_PICK(6)
WIDE_PHOTO_STOCK_PICK(7)
WIDE_PHOTO_STOCK_PICK(8)
WIDE_PHOTO_STOCK_PICK(9)
#undef WIDE_PHOTO_STOCK_PICK

static z72_u32 wide_photo_c1(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return wide_af_choose(1);
}

static z72_u32 wide_photo_c2(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return wide_af_choose(2);
}

static void wide_photo_page_init(const z72_u32 *page)
{
    WIDE_MENU_CALL(0x106a7a39u,void,const z72_u32 *)(page);
    z72_u32 active=wide_af_active();
    if (!active) return;
    z72_u32 subject=WIDE_MENU_CALL(0x1063c7d5u,z72_u32,z72_u32)(0x058a);
    z72_u32 stock_id=subject==1 ? 0x1953 : subject==2 ? 0x1954 : 0x188c;
    z72_u32 custom_id=WIDE_PHOTO_C1_ID+active-1;
    WIDE_MENU_CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,stock_id,0);
    if (WIDE_MENU_CALL(0x1063bf77u,z72_u32,void)()!=1)
        WIDE_MENU_CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,custom_id);
    WIDE_MENU_CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,custom_id,1);
}

static z72_u32 wide_photo_menu_ready(void)
{
    if (WIDE_PHOTO_PAGE[0]!=0x00a80002u || WIDE_PHOTO_PAGE[1]!=0xb2bb7238u ||
        WIDE_PHOTO_PAGE[4]!=(z72_u32)WIDE_PHOTO_CALLBACKS || WIDE_PHOTO_PAGE[9] ||
        WIDE_PHOTO_PAGE[10]!=(z72_u32)WIDE_PHOTO_HEADER ||
        WIDE_PHOTO_CALLBACKS[0]!=0x106a7a39u ||
        WIDE_PHOTO_HEADER[0]!=0x1000u || WIDE_PHOTO_HEADER[1]!=0x1187c570u ||
        WIDE_PHOTO_HEADER[2]!=0x1181c988u || WIDE_PHOTO_HEADER[3]!=0x11824624u ||
        WIDE_PHOTO_HEADER[4]!=0x11873198u || WIDE_PHOTO_HEADER[5]!=0x8000au) return 0;
    const z72_u32 *maps=(const z72_u32 *)WIDE_PHOTO_HEADER[4];
    for (z72_u32 row=0;row<WIDE_STOCK_ROWS;++row) {
        if (maps[row]!=0x1185f5d4u+row*0x100u) return 0;
        const z72_u32 (*events)[4]=(const z72_u32 (*)[4])maps[row];
        if (events[6][0]!=0x257 || events[11][0]!=0x27d ||
            events[6][1]!=wide_photo_native_confirm[row] || events[11][1]!=wide_photo_native_confirm[row] ||
            events[6][2] || events[11][2] || events[6][3]!=0x431 || events[11][3]!=0x431 ||
            events[WIDE_PHOTO_EVENTS-1][0]!=0x31a) return 0;
    }
    return 1;
}

static void wide_photo_menu_install(void)
{
    static const wide_photo_action actions[WIDE_PHOTO_ROWS]={
        wide_photo_stock0,wide_photo_stock1,wide_photo_stock2,wide_photo_stock3,
        wide_photo_stock4,wide_photo_stock5,wide_photo_stock6,wide_photo_c1,wide_photo_c2,
        wide_photo_stock7,wide_photo_stock8,wide_photo_stock9
    };
    const z72_u32 (*rows)[7]=(const z72_u32 (*)[7])WIDE_PHOTO_HEADER[3];
    const z72_u32 *maps=(const z72_u32 *)WIDE_PHOTO_HEADER[4];
    for (z72_u32 i=0;i<6;++i) wide_photo_page.header[i]=WIDE_PHOTO_HEADER[i];
    for (z72_u32 i=0;i<5;++i) wide_photo_page.callbacks[i]=WIDE_PHOTO_CALLBACKS[i];
    wide_photo_page.callbacks[0]=(z72_u32)wide_photo_page_init;
    for (z72_u32 row=0;row<WIDE_PHOTO_ROWS;++row) {
        z72_u32 custom=row>=WIDE_INSERT_ROW && row<WIDE_INSERT_ROW+2;
        /* Use the stock wide L symbol and help page for both custom rows. */
        z72_u32 source=custom ? 4 : row>=WIDE_INSERT_ROW+2 ? row-2 : row;
        z72_u32 *item=wide_photo_page.rows[row];
        for (z72_u32 col=0;col<7;++col) item[col]=rows[source][col];
        if (custom) {
            item[0]=row==WIDE_INSERT_ROW ? Z72_TEXT_WIDE_C1 : Z72_TEXT_WIDE_C2;
            item[6]=WIDE_PHOTO_C1_ID+row-WIDE_INSERT_ROW;
        }
        const z72_u32 (*events)[4]=(const z72_u32 (*)[4])maps[source];
        for (z72_u32 event=0;event<WIDE_PHOTO_EVENTS;++event)
            for (z72_u32 col=0;col<4;++col)
                wide_photo_page.events[row][event][col]=events[event][col];
        wide_photo_page.events[row][6][1]=(z72_u32)actions[row];
        wide_photo_page.events[row][11][1]=(z72_u32)actions[row];
        wide_photo_page.maps[row]=(z72_u32)wide_photo_page.events[row];
    }
    wide_photo_page.header[3]=(z72_u32)wide_photo_page.rows;
    wide_photo_page.header[4]=(z72_u32)wide_photo_page.maps;
    wide_photo_page.header[5]=(WIDE_PHOTO_HEADER[5]&0xffff0000u)|WIDE_PHOTO_ROWS;
    __asm__ volatile("dmb sy" ::: "memory");
    WIDE_PHOTO_PAGE[4]=(z72_u32)wide_photo_page.callbacks;
    WIDE_PHOTO_PAGE[10]=(z72_u32)wide_photo_page.header;
}

#undef WIDE_MENU_CALL
#undef WIDE_PHOTO_CALLBACKS
#undef WIDE_PHOTO_HEADER
#undef WIDE_PHOTO_PAGE
#endif
