/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_MENU_H
#define Z72_FTP_MENU_H

enum { FTP_MENU_ROOT, FTP_MENU_FILES, FTP_MENU_SERVERS, FTP_MENU_STATUS,
       FTP_MENU_EDIT, FTP_MENU_OPTIONS, FTP_MENU_FOLDERS, FTP_MENU_CLEAR,
       FTP_MENU_VIEW_COUNT, FTP_MENU_INPUT=FTP_MENU_VIEW_COUNT };
enum { FTP_ROW_SLOT=291, FTP_MENU_ID=0x2d80, FTP_MENU_ROWS=Z72_FTP_MENU_ROWS };
enum { FTP_WT7_UPLOAD_TEXT=0x488, FTP_SELECT_PHOTOS_TEXT=0x166 };
enum { FTP_FIRST_PHOTO=2, FTP_PLAYBACK_ROW_ID=0x2ef0u };
enum { FTP_STATUS_REFRESH=3, FTP_STATUS_ACTION, FTP_STATUS_PENDING, FTP_STATUS_SENT, FTP_STATUS_ROWS };
static struct {
    u32 header[6],title[24],rows[FTP_MENU_ROWS][7],maps[FTP_MENU_ROWS];
    u32 events[FTP_MENU_ROWS][ROTATION_EVENTS][4];
} ftp_pages[FTP_MENU_VIEW_COUNT];
static u32 ftp_callbacks[5],ftp_view,ftp_focus,ftp_has_scan,ftp_status_parent;
static u32 ftp_edit_waiting;
#include "ftp_keyboard.h"

static u32 ftp_header_owned(u32 header)
{
    if (header==(u32)ftp_keyboard.header) return 1;
    for (u32 v=0;v<FTP_MENU_VIEW_COUNT;++v) if (header==(u32)ftp_pages[v].header) return 1;
    return 0;
}

static char *ftp_decimal(char *out,u32 n)
{
    char digits[10];
    u32 count=0;
    do { digits[count++]=(char)('0'+n%10u); n/=10u; } while (n);
    while (count) *out++=digits[--count];
    *out=0;
    return out;
}

struct ftp_row_text {
    u32 translation;
    unsigned char *resources;
    u32 *offsets;
};

static unsigned short *ftp_wide(unsigned short *out,const unsigned short *in)
{
    while (*in) *out++=*in++;
    *out=0;
    return out;
}

static unsigned short *ftp_ascii(unsigned short *out,const char *in)
{
    while (*in) *out++=(unsigned char)*in++;
    *out=0;
    return out;
}

static unsigned short *ftp_number(unsigned short *out,u32 n)
{
    char text[11];
    ftp_decimal(text,n);
    return ftp_ascii(out,text);
}

static unsigned short *ftp_mib(unsigned short *out,u32 bytes)
{
    out=ftp_number(out,bytes>>20);
    *out++='.';
    return ftp_number(out,((bytes&0xfffffu)*10u)>>20);
}

static void ftp_label(struct ftp_row_text *output,u32 row,const unsigned short *label)
{
    u32 n=0,ascii=1;
    while (n<Z72_FTP_LABEL_UNITS && label[n]) {
        ascii&=label[n]<128u;
        ++n;
    }
    u32 localized=!ascii;
    u32 *item=ftp_pages[ftp_view].rows[row];
    item[0]=(localized ? Z72_TEXT_FTP_ROW : FTP_ROW_SLOT)+row;
    item[4]=localized ? 0x1001u : 0x1201u;
    u32 offset=output->offsets[row];
    if (localized) {
        unsigned short *text=(unsigned short *)(output->resources+offset);
        for (u32 i=0;i<n;++i) text[i]=label[i];
        offset+=n*sizeof(*text);
    } else {
        char text[Z72_FTP_LABEL_UNITS+1];
        for (u32 i=0;i<n;++i) text[i]=(char)label[i];
        text[n]=0;
        CALL(0x10636a07u,void,const char *,u32,u32)(text,FTP_ROW_SLOT+row,0);
    }
    output->offsets[row+1]=offset;
}

static int ftp_transferring(u32 job)
{
    return job==FTP_JOB_UPLOAD || job==FTP_JOB_FOLDER_SEND || job==FTP_JOB_AUTO;
}

#include "ftp_wifi_hold.h"

static void ftp_labels(void)
{
    unsigned short text[Z72_FTP_LABEL_UNITS+1];
    u32 lang=CALL(0x1073b8a1u,u32,void)();
    if (lang>=Z72_MENU_LANGUAGES) lang=1;
    struct ftp_row_text output={
        lang==8 ? 1u : lang==9 ? 2u : 0u,
        (unsigned char *)z72_ftp_locale->texts[lang],
        &z72_ftp_locale->offsets[lang][Z72_TEXT_FTP_ROW]
    };
#define FTP_TEXT(en,cn,tw) (output.translation==1 ? u##cn : output.translation==2 ? u##tw : u##en)
#define FTP_LABEL(row,en,cn,tw) ftp_label(&output,row,FTP_TEXT(en,cn,tw))
    u32 count=0,job=ftp_load(&ftp_job);
    if (ftp_view==FTP_MENU_ROOT) {
        unsigned short *end=ftp_wide(text,FTP_TEXT("Select photos","选择影像","選擇影像"));
        u32 pending=ftp_pending();
        if (pending) ftp_number(ftp_wide(end,u"  "),pending);
        ftp_label(&output,0,text);
        ftp_number(ftp_wide(text,FTP_TEXT("Network settings: ","网络设定：配置 ","網路設定：設定 ")),ftp_profile+1);
        ftp_label(&output,1,text);
        FTP_LABEL(2,"Start upload","开始上传","開始上傳");
        FTP_LABEL(3,"Transfer status","传输状态","傳輸狀態");
        FTP_LABEL(4,"Options","选项","選項");
        count=5;
    } else if (ftp_view==FTP_MENU_FILES) {
        FTP_LABEL(0,"Newest photos","最新照片","最新照片");
        if (ftp_more) FTP_LABEL(1,"Older photos","更多照片","更多照片");
        else if (ftp_photo_count) FTP_LABEL(1,"End of photo list","没有更多照片","沒有更多照片");
        else FTP_LABEL(1,"No photos found","未找到照片","未找到照片");
        count=FTP_FIRST_PHOTO+ftp_photo_count;
        for (u32 i=0;i<ftp_photo_count;++i) {
            unsigned short *end=ftp_ascii(text,ftp_photos[i].path+8);
            if (job!=FTP_JOB_FOLDER_SEND && ftp_marked(ftp_photos[i].id)) ftp_wide(end,u" *");
            ftp_label(&output,FTP_FIRST_PHOTO+i,text);
        }
    } else if (ftp_view==FTP_MENU_OPTIONS) {
        if (ftp_load(&ftp_auto_epoch)&1u) {
            ftp_number(ftp_wide(text,FTP_TEXT("Auto send: On / ","自动发送：开启 / 配置 ","自動傳送：開啟 / 設定 ")),
                       ftp_load(&ftp_auto_profile)+1u);
            ftp_label(&output,0,text);
        } else if (ftp_auto_error) FTP_LABEL(0,"Auto send: Unavailable","自动发送：暂不可用","自動傳送：暫無法使用");
        else FTP_LABEL(0,"Auto send: Off","自动发送：关闭","自動傳送：關閉");
        if (ftp_load(&ftp_jpeg_only)) FTP_LABEL(1,"Send file as: JPEG only","传送文件为：仅JPEG","傳送檔案為：僅JPEG");
        else FTP_LABEL(1,"Send file as: NEF + JPEG","传送文件为：NEF + JPEG","傳送檔案為：NEF + JPEG");
        FTP_LABEL(2,"Send folder","发送文件夹","傳送檔案夾");
        FTP_LABEL(3,"Deselect all?","取消全部选择？","取消全部選擇？");
        if (ftp_load(&ftp_overwrite)) FTP_LABEL(4,"Overwrite if same name: Yes","同名时重写：是","同名時覆寫：是");
        else FTP_LABEL(4,"Overwrite if same name: No","同名时重写：否","同名時覆寫：否");
        count=5;
    } else if (ftp_view==FTP_MENU_FOLDERS) {
        FTP_LABEL(0,"Newest folders","最新文件夹","最新檔案夾");
        if (ftp_folder_more) FTP_LABEL(1,"Older folders","更多文件夹","更多檔案夾");
        else if (ftp_folder_count) FTP_LABEL(1,"End of folder list","没有更多文件夹","沒有更多檔案夾");
        else FTP_LABEL(1,"No folders found","未找到文件夹","未找到檔案夾");
        count=FTP_FIRST_PHOTO;
        for (u32 i=0;i<ftp_folder_count;++i) {
            ftp_number(ftp_wide(text,FTP_TEXT("Folder ","文件夹 ","檔案夾 ")),ftp_folders[i]);
            ftp_label(&output,count++,text);
        }
    } else if (ftp_view==FTP_MENU_CLEAR) {
        FTP_LABEL(0,"No","否","否");
        FTP_LABEL(1,"Yes","是","是");
        count=2;
    } else if (ftp_view==FTP_MENU_SERVERS) {
        for (u32 i=0;i<FTP_CONFIGS;++i) {
            unsigned short *end=ftp_wide(ftp_number(ftp_wide(text,FTP_TEXT("Profile ","配置 ","設定 ")),i+1),u": ");
            end=ftp_ascii(end,ftp_profiles[i]+3);
            if (ftp_profile==i) ftp_wide(end,u" *");
            ftp_label(&output,i,text);
        }
        count=FTP_CONFIGS;
        if (job==FTP_JOB_NONE) {
            FTP_LABEL(3,"Edit active profile","编辑当前配置","編輯目前設定");
            ++count;
        }
    } else if (ftp_view==FTP_MENU_EDIT) {
        ftp_edit_waiting=job==FTP_JOB_CONFIG_SAVE;
        if (ftp_edit_waiting) {
            FTP_LABEL(0,"Saving profile","正在保存配置","正在儲存設定");
            FTP_LABEL(1,"Refresh","更新状态","重新整理狀態");
            count=2;
        } else {
            unsigned short *end=ftp_wide(text,FTP_TEXT("Server: ","服务器：","伺服器："));
            ftp_ascii(end,ftp_draft.address[0] ? ftp_draft.address : "--");
            ftp_label(&output,0,text);
            FTP_LABEL(1,"User name","用户名","使用者名稱");
            if (ftp_draft.password[0]) FTP_LABEL(2,"Password *","密码 *","密碼 *");
            else FTP_LABEL(2,"Password","密码","密碼");
            end=ftp_wide(text,FTP_TEXT("Destination folder: ","目标文件夹：","目的檔案夾："));
            if (ftp_draft.folder[0]) {
                for (const char *dir=ftp_draft.folder;*dir && end<text+Z72_FTP_LABEL_UNITS;++dir)
                    *end++=(unsigned char)*dir;
                *end=0;
            } else ftp_wide(end,FTP_TEXT("Login folder","登录目录","登入目錄"));
            ftp_label(&output,3,text);
            FTP_LABEL(4,"Save profile","保存配置","儲存設定");
            count=5;
            switch (ftp_edit_status) {
            case FTP_EDIT_NEW: FTP_LABEL(5,"Enter server and user name","请输入地址和用户名","請輸入位址和使用者名稱"); break;
            case FTP_EDIT_BAD_ADDRESS: FTP_LABEL(5,"Check IPv4:port","请检查 IPv4:端口","請檢查 IPv4:連接埠"); break;
            case FTP_EDIT_BAD_USER: FTP_LABEL(5,"Enter a user name","请输入用户名","請輸入使用者名稱"); break;
            case FTP_EDIT_SAVED: FTP_LABEL(5,"Profile saved","配置已保存","設定已儲存"); break;
            case FTP_EDIT_SAVE_FAILED: FTP_LABEL(5,"Save failed. Try again","保存失败，请重试","儲存失敗，請重試"); break;
            default: break;
            }
            if (ftp_edit_status!=FTP_EDIT_READY) ++count;
        }
    } else {
        static const unsigned short *const states[][3]={
            {u"Ready",u"准备上传",u"準備上傳"},
            {u"Check FTP config",u"请检查 FTP 配置",u"請檢查 FTP 設定"},
            {u"Cannot open photo",u"无法打开照片",u"無法開啟照片"},
            {u"Connecting to FTP server",u"正在连接 FTP 服务器",u"正在連線至 FTP 伺服器"},
            {u"Memory card read error",u"存储卡读取错误",u"記憶卡讀取錯誤"},
            {u"Timed out",u"传输超时",u"傳輸超時"},
            {u"Upload complete",u"上传完成",u"上傳完成"},
            {u"FTP upload failed",u"FTP 上传失败",u"FTP 上傳失敗"},
            {u"FTP unavailable",u"FTP 不可用",u"FTP 無法使用"},
            {u"Cancelled",u"已取消",u"已取消"},
            {u"Waiting for Wi-Fi",u"等待 Wi-Fi 连接",u"等待 Wi-Fi 連線"}
        };
        struct ftp_result s;
        u32 state=ftp_load(&ftp_local),total=ftp_load(&ftp_total),has=ftp_snapshot(&s);
        const unsigned short *status=states[state<=FTP_LOCAL_WAIT_WIFI ? state : FTP_LOCAL_TASK][output.translation];
        if (ftp_load(&ftp_discard) || (ftp_transferring(job) && ftp_load(&ftp_cancel)))
            status=FTP_TEXT("Cancelling","正在取消","正在取消");
        else if (state==FTP_LOCAL_BUSY && has) {
            if (s.stage>=FTP_READ_CARD && s.stage<=FTP_FINISH) status=FTP_TEXT("Uploading","正在上传","正在上傳");
            else if (s.stage==FTP_WAIT_AP) status=states[FTP_LOCAL_WAIT_WIFI][output.translation];
        }
        ftp_label(&output,0,status);
        u32 sent=has ? s.sent : 0;
        /* 256 MiB fits when scaled by 100 in KiB. Bounded to 100 steps. */
        u32 percent=0,kib=(total+1023u)>>10,scaled=(sent>>10)*100u;
        if (total && sent>=total) percent=100;
        else if (kib) for (;scaled>=kib && percent<100;scaled-=kib) ++percent;
        unsigned short *end=ftp_wide(ftp_number(ftp_wide(text,FTP_TEXT("Progress: ","进度：","進度：")),percent),u"%");
        if (total>=1048576u) {
            end=ftp_mib(ftp_wide(end,u"  "),sent);
            ftp_wide(ftp_mib(ftp_wide(end,u"/"),total),u" MiB");
        }
        ftp_label(&output,1,text);
        end=ftp_wide(text,FTP_TEXT("FTP reply: ","FTP 回复码：","FTP 回覆碼："));
        if (has && s.reply) ftp_number(end,s.reply);
        else ftp_wide(end,u"--");
        ftp_label(&output,2,text);
        FTP_LABEL(FTP_STATUS_REFRESH,"Refresh status","更新状态","重新整理狀態");
        if (ftp_transferring(job)) {
            FTP_LABEL(FTP_STATUS_ACTION,"Cancel upload","取消上传","取消上傳");
        } else if (ftp_pending()) FTP_LABEL(FTP_STATUS_ACTION,"Send pending photos","发送待传影像","傳送待傳影像");
        else FTP_LABEL(FTP_STATUS_ACTION,"Select photos","选择影像","選擇影像");
        u32 auto_read=ftp_load(&ftp_auto_read),auto_count=ftp_load(&ftp_auto_write)-auto_read;
        ftp_number(ftp_wide(text,FTP_TEXT("Pending: ","待发送：","待傳送：")),ftp_pending()+auto_count);
        ftp_label(&output,FTP_STATUS_PENDING,text);
        u32 dropped=ftp_load(&ftp_auto_dropped);
        if (dropped || ftp_load(&ftp_queue_full)) {
            end=ftp_wide(text,FTP_TEXT("Queue full","待传已满","待傳已滿"));
            if (dropped) ftp_number(ftp_wide(end,u" / "),dropped);
        } else ftp_number(ftp_wide(text,FTP_TEXT("Sent: ","已发送：","已傳送：")),ftp_load(&ftp_completed));
        ftp_label(&output,FTP_STATUS_SENT,text);
        count=FTP_STATUS_ROWS;
    }
#undef FTP_LABEL
#undef FTP_TEXT
    for (u32 row=count;row<FTP_MENU_ROWS;++row) output.offsets[row+1]=output.offsets[count];
    ftp_pages[ftp_view].header[5]=(count<<16)|count;
    if (ftp_focus>=count) ftp_focus=0;
}

static void ftp_page_init(const u32 *page)
{
    if (ftp_view==FTP_MENU_INPUT) { ftp_keyboard_init(page); return; }
    u32 id=FTP_MENU_ID+ftp_view*16+ftp_focus;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 ftp_show(u32 view,u32 row)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    int owned=ftp_header_owned(header);
    int known=header==(u32)rotation_menu.header || zebra_header_owned(header);
#ifdef Z72_JPEG_PRE_CAPTURE
    known=known || pre_capture_menu_source(header) || jpeg_menu_owned(header);
#endif
    if (WORD(ROTATION_PAGE)!=2 || !(owned || known) ||
        (WORD(ROTATION_PAGE+0x24) && !owned)) return 1;
    ftp_install_wifi_hold();
    ftp_view=view;
    ftp_focus=row;
    if (view!=FTP_MENU_INPUT) ftp_labels();
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)ftp_callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)(view==FTP_MENU_INPUT ? ftp_keyboard.header : ftp_pages[view].header);
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

/* The native catalogue reads are bounded to one page. Claim the FTP slot so
 * the GUI and the transfer worker never share the card/FTP scratch state. */
static int ftp_read_photos(u32 reset)
{
    if (!ftp_claim_job()) return 0;
    if (reset) { ftp_cursor.folder=1000; ftp_cursor.id=0; }
    ftp_scan();
    ftp_has_scan=1;
    ftp_store(&ftp_job,FTP_JOB_NONE);
    return 1;
}

static u32 ftp_open_files(u32 job)
{
    if (!ftp_has_scan && !ftp_read_photos(1)) return 1;
    u32 focus=ftp_photo_count ? FTP_FIRST_PHOTO : 0u;
    if (job!=FTP_JOB_FOLDER_SEND)
        for (u32 i=0;i<ftp_photo_count;++i)
            if (ftp_marked(ftp_photos[i].id)) { focus=FTP_FIRST_PHOTO+i; break; }
    return ftp_show(FTP_MENU_FILES,focus);
}

static int ftp_read_folders(u32 reset)
{
    if (!ftp_claim_job()) return 0;
    if (reset) ftp_folder_next=1000;
    ftp_scan_folders();
    ftp_store(&ftp_job,FTP_JOB_NONE);
    return 1;
}

static u32 ftp_choose_folder(u32 row)
{
    if (row<FTP_FIRST_PHOTO) {
        if ((row && !ftp_folder_more) || !ftp_read_folders(row==0)) return 1;
        return ftp_show(FTP_MENU_FOLDERS,ftp_folder_count ? FTP_FIRST_PHOTO : 0);
    }
    if (row-FTP_FIRST_PHOTO>=ftp_folder_count) return 1;
    ftp_folder_send=ftp_folders[row-FTP_FIRST_PHOTO];
    if (!ftp_start_job(FTP_JOB_FOLDER_SEND)) return 1;
    ftp_status_parent=FTP_MENU_FOLDERS;
    return ftp_show(FTP_MENU_STATUS,FTP_STATUS_REFRESH);
}

static u32 ftp_edit_choose(u32 row)
{
    if (ftp_edit_waiting) return ftp_show(FTP_MENU_EDIT,0);
    if (row<4) return ftp_keyboard_open(row);
    if (row==4) {
        struct ftp_request address;
        if (!endpoint(ftp_draft.address,&address)) ftp_edit_status=FTP_EDIT_BAD_ADDRESS;
        else if (!ftp_draft.user[0]) ftp_edit_status=FTP_EDIT_BAD_USER;
        else if (!ftp_start_job(FTP_JOB_CONFIG_SAVE)) ftp_edit_status=FTP_EDIT_SAVE_FAILED;
    }
    return ftp_show(FTP_MENU_EDIT,4);
}

static u32 ftp_choose(u32 row)
{
    u32 job=ftp_load(&ftp_job);
    if (ftp_view==FTP_MENU_ROOT) {
        if (row==0) return ftp_open_files(job);
        if (row==1) return ftp_show(FTP_MENU_SERVERS,ftp_profile);
        if (row==4) return ftp_show(FTP_MENU_OPTIONS,0);
        if (row==2) {
            if (!ftp_pending()) return ftp_open_files(job);
            ftp_start_job(FTP_JOB_UPLOAD);
        }
        ftp_status_parent=FTP_MENU_ROOT;
        return ftp_show(FTP_MENU_STATUS,FTP_STATUS_REFRESH);
    }
    if (ftp_view==FTP_MENU_STATUS) {
        if (row==FTP_STATUS_ACTION) {
            if (ftp_transferring(job)) ftp_store(&ftp_cancel,1);
            else if (job==FTP_JOB_NONE) {
                if (ftp_pending()) ftp_start_job(FTP_JOB_UPLOAD);
                else return ftp_open_files(job);
            }
        }
        return ftp_show(FTP_MENU_STATUS,FTP_STATUS_REFRESH);
    }
    if (ftp_view==FTP_MENU_OPTIONS) {
        if (row==0) ftp_toggle_auto();
        else if (row==1) ftp_store(&ftp_jpeg_only,!ftp_load(&ftp_jpeg_only));
        else if (row==2 && ftp_read_folders(1)) {
            return ftp_show(FTP_MENU_FOLDERS,ftp_folder_count ? FTP_FIRST_PHOTO : 0);
        } else if (row==3) return ftp_show(FTP_MENU_CLEAR,0);
        else if (row==4) ftp_store(&ftp_overwrite,!ftp_load(&ftp_overwrite));
        return ftp_show(FTP_MENU_OPTIONS,row);
    }
    if (ftp_view==FTP_MENU_CLEAR) {
        if (row==0) return ftp_show(FTP_MENU_OPTIONS,3);
        if (job!=FTP_JOB_NONE || ftp_start_job(FTP_JOB_NONE)) {
            ftp_store(&ftp_discard,1);
            ftp_store(&ftp_cancel,1);
        }
        ftp_status_parent=FTP_MENU_OPTIONS;
        return ftp_show(FTP_MENU_STATUS,FTP_STATUS_REFRESH);
    }
    if (job!=FTP_JOB_NONE) return ftp_show(ftp_view,row);
    if (ftp_view==FTP_MENU_FOLDERS) return ftp_choose_folder(row);
    if (ftp_view==FTP_MENU_SERVERS) {
        if (row==FTP_CONFIGS) {
            if (!ftp_claim_job()) return 1;
            ftp_edit_read();
            ftp_store(&ftp_job,FTP_JOB_NONE);
            return ftp_show(FTP_MENU_EDIT,0);
        }
        if (row<FTP_CONFIGS) ftp_profile=row;
        return ftp_show(FTP_MENU_ROOT,1);
    }
    if (ftp_view==FTP_MENU_EDIT) return ftp_edit_choose(row);
    if (row<FTP_FIRST_PHOTO) {
        if ((row && !ftp_more) || !ftp_read_photos(row==0)) return 1;
        return ftp_show(FTP_MENU_FILES,ftp_photo_count ? FTP_FIRST_PHOTO : 0);
    }
    if (row-FTP_FIRST_PHOTO>=ftp_photo_count) return 1;
    if (!ftp_toggle_mark(&ftp_photos[row-FTP_FIRST_PHOTO]) && ftp_load(&ftp_queue_full)) {
        ftp_status_parent=FTP_MENU_FILES;
        return ftp_show(FTP_MENU_STATUS,FTP_STATUS_REFRESH);
    }
    return ftp_show(FTP_MENU_FILES,row);
}

#define FTP_PICK(n) static u32 ftp_pick_##n(u32 a,u32 b,u32 c,u32 d) \
{ (void)a; (void)b; (void)c; (void)d; return ftp_choose(n); }
FTP_PICK(0) FTP_PICK(1) FTP_PICK(2) FTP_PICK(3) FTP_PICK(4)
FTP_PICK(5) FTP_PICK(6) FTP_PICK(7)
#undef FTP_PICK

static u32 ftp_back(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (ftp_view==FTP_MENU_EDIT) {
        if (ftp_load(&ftp_job)!=FTP_JOB_NONE) return ftp_show(FTP_MENU_EDIT,0);
        for (u32 i=0;i<sizeof(ftp_draft.password);++i) ((volatile char *)ftp_draft.password)[i]=0;
        return ftp_show(FTP_MENU_SERVERS,FTP_CONFIGS);
    }
    if (ftp_view==FTP_MENU_FOLDERS || ftp_view==FTP_MENU_CLEAR)
        return ftp_show(FTP_MENU_OPTIONS,ftp_view==FTP_MENU_FOLDERS ? 2 : 3);
    if (ftp_view==FTP_MENU_STATUS && ftp_status_parent!=FTP_MENU_ROOT)
        return ftp_show(ftp_status_parent,ftp_status_parent==FTP_MENU_OPTIONS ? 3 : 0);
    return ftp_show(FTP_MENU_ROOT,ftp_view==FTP_MENU_FILES ? 0 : ftp_view==FTP_MENU_SERVERS ? 1 :
                    ftp_view==FTP_MENU_OPTIONS ? 4 : 3);
}

static u32 open_ftp_menu(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return ftp_show(FTP_MENU_ROOT,ftp_transferring(ftp_load(&ftp_job)) ? 3 : 0);
}

static int ftp_menu_ready(void)
{
    for (u32 slot=FTP_ROW_SLOT;slot<FTP_ROW_SLOT+FTP_MENU_ROWS;++slot)
        if (!WORD(0xb20287b4u+slot*12+4)) return 0;
    return ftp_service_ready();
}

static int ftp_mark_playback(void)
{
    if (ftp_load(&ftp_job)!=FTP_JOB_NONE) return 0;
    struct ftp_photo photo;
    /* The stock WT playback action also resolves the current image via 3B10. */
    photo.id=CALL(0x1063c7d5u,u32,u32)(0x3b10);
    return photo.id>>29==1u && ftp_photo_path(photo.id,photo.path) && ftp_toggle_mark(&photo);
}

static void ftp_menu_prepare(void)
{
    static const z72_menu_callback actions[]={ftp_pick_0,ftp_pick_1,ftp_pick_2,
        ftp_pick_3,ftp_pick_4,ftp_pick_5,ftp_pick_6,ftp_pick_7};
    const u32 *setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *stock=(const u32 *)setup[3];
    const u32 titles[]={FTP_WT7_UPLOAD_TEXT,FTP_SELECT_PHOTOS_TEXT,0x478,Z72_TEXT_FTP_STATUS,
                        Z72_TEXT_FTP_EDIT,0x492,0x30c,0x30d};
    ftp_keyboard_prepare();
    for (u32 v=0;v<FTP_MENU_VIEW_COUNT;++v) {
        for (u32 i=0;i<6;++i) ftp_pages[v].header[i]=setup[i];
        for (u32 i=0;i<24;++i) ftp_pages[v].title[i]=rotation_menu.title[i];
        ftp_pages[v].title[0]=titles[v];
        ftp_pages[v].title[1]=1;
        ftp_pages[v].header[2]=(u32)ftp_pages[v].title;
        ftp_pages[v].header[3]=(u32)ftp_pages[v].rows;
        ftp_pages[v].header[4]=(u32)ftp_pages[v].maps;
        for (u32 row=0;row<FTP_MENU_ROWS;++row) {
            u32 *item=ftp_pages[v].rows[row];
            for (u32 i=0;i<7;++i) item[i]=stock[i];
            item[0]=FTP_ROW_SLOT+row;
            item[1]=item[2]=0xffffu; item[3]=0;
            item[4]=0x1201u;
            item[5]=0xd7u; item[6]=FTP_MENU_ID+v*16+row;
            ftp_pages[v].maps[row]=(u32)ftp_pages[v].events[row];
            for (u32 i=0;i<ROTATION_EVENTS;++i) {
                u32 *entry=ftp_pages[v].events[row][i];
                for (u32 col=0;col<4;++col) entry[col]=rotation_menu.events[0][i][col];
                u32 ev=entry[0];
                if (ev==0x257 || ev==0x25c || ev==0x27d || ev==0x252) {
                    entry[1]=(u32)actions[row]; entry[2]=0; entry[3]=0x430;
                }
                if (ev==0x256 || ev==0x271 || ev==0x2cf || ev==0x27f) {
                    entry[1]=v ? (u32)ftp_back : 0; entry[2]=0; entry[3]=v ? 0x430u : 0x431u;
                }
            }
        }
    }
    for (u32 i=0;i<5;++i) ftp_callbacks[i]=rotation_menu.callbacks[i];
    ftp_callbacks[0]=(u32)ftp_page_init;
}
#endif
