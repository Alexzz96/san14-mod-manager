#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "officer_ui.h"
#include "pinyin.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } checks++; } while(0)
static int checks;
static int no_read(void *context,uintptr_t address,void *out,size_t size) { (void)context;(void)address;(void)out;(void)size;return 0; }
static int career(void *context,const wchar_t *campaign,const wchar_t *branch,int id,S14OfficerCareer *out) {
    (void)context;if (wcscmp(campaign,L"test-campaign") || wcscmp(branch,L"branch-a") || id!=613) return 0;
    out->valid_mask=S14_CAREER_KILLS;out->officer_kills=0;return 1;
}
static int render(S14OfficerUI *ui,const char *path) {
    RECT r;GetClientRect(ui->window,&r);int width=r.right,height=r.bottom;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,NULL,0);HGDIOBJ old=SelectObject(dc,bitmap);
    int ok=PrintWindow(ui->window,dc,PW_CLIENTONLY);GdiFlush();
    BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+width*height*4;
    FILE *f=fopen(path,"wb");if (!f) ok=0;
    if (f) { ok=ok && fwrite(&header,sizeof(header),1,f)==1 && fwrite(&bi.bmiHeader,sizeof(BITMAPINFOHEADER),1,f)==1 && fwrite(bits,width*height*4,1,f)==1;fclose(f); }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);return ok;
}
int main(int argc,char **argv) {
    S14Officer a={0},b={0};a.id=613;a.force=2;a.status=4;a.place_kind=2;a.ability[0]=83;a.ability[1]=72;a.total=382;
    wcscpy(a.name,L"张嶷");wcscpy(a.courtesy,L"伯岐");wcscpy(a.personality_text,L"山战、伏击、募兵");a.troops=5000;
    b.id=518;b.force=5;b.status=2;b.place_kind=2;b.ability[0]=93;b.ability[1]=87;b.total=362;wcscpy(b.name,L"曹仁");b.troops=5411;
    S14OfficerFilter f={0};CHECK(s14_officer_matches(&a,2,&f));
    wcscpy(f.query,L"山战");CHECK(s14_officer_matches(&a,2,&f));CHECK(!s14_officer_matches(&b,2,&f));
    wcscpy(f.query,L"张嶷 山战");CHECK(s14_officer_matches(&a,2,&f));
    wcscpy(f.query,L"张嶷 铁壁");CHECK(!s14_officer_matches(&a,2,&f));
    wcscpy(f.query,L"伯岐");CHECK(s14_officer_matches(&a,2,&f));
    const wchar_t *queries[]={L"zhangni",L"zhangyi",L"zn",L"zy",L"ZHANGNI",L"boqi",L"bq",L"shanzhan",L"sz",L"zy sz",L"张嶷 sz",L"zhang嶷",L"伏ji"};
    for (unsigned int j=0;j<sizeof(queries)/sizeof(*queries);j++) { wcscpy(f.query,queries[j]);CHECK(s14_officer_matches(&a,2,&f)); }
    CHECK(s14_search_contains(L"曹操",L"caocao",6));CHECK(s14_search_contains(L"曹操",L"cc",2));
    CHECK(s14_search_contains(L"诸葛亮",L"zhugeliang",10));CHECK(s14_search_contains(L"诸葛亮",L"zgl",3));
    CHECK(s14_search_contains(L"劉備",L"liubei",6));CHECK(s14_search_contains(L"吕布",L"lvbu",4));
    CHECK(s14_search_contains(L"吕布",L"lubu",4));CHECK(s14_search_contains(L"吕布",L"LÜBU",4));
    CHECK(s14_search_contains(L"吕布",L"lb",2));CHECK(!s14_search_contains(L"山战、伏击",L"zhanfu",6));
    CHECK(!s14_search_contains(L"曹仁",L"caocao",6));CHECK(!s14_search_contains(L"张嶷",L"zhangnix",8));
    wchar_t too_long[129];wmemset(too_long,L'z',128);too_long[128]=0;CHECK(!s14_search_contains(L"张嶷",too_long,128));
    unsigned char raw[512]={0};a.status=4;a.force=2;
    for (int value=-20;value<=20;value+=10) {
        int16_t encoded=(int16_t)value;memcpy(raw+0x1a2,&encoded,2);memcpy(raw+0x1a4,&encoded,2);
        raw[0x120]=120;s14_officer_decode_character(raw,&a);
        CHECK(a.ambition==value/10+3);CHECK(a.bond==value/10+3);CHECK(a.loyalty==120);
    }
    CHECK(s14_officer_inner_grade(-30)==-1);CHECK(s14_officer_inner_grade(15)==-1);CHECK(s14_officer_inner_grade(30)==-1);
    a.status=1;s14_officer_decode_character(raw,&a);CHECK(a.loyalty==-1);
    a.status=5;s14_officer_decode_character(raw,&a);CHECK(a.loyalty==-1);
    a.status=6;a.force=0;s14_officer_decode_character(raw,&a);CHECK(a.loyalty==-1);
    a.status=4;a.force=2;raw[0x120]=0;s14_officer_decode_character(raw,&a);CHECK(a.loyalty==0);
    raw[0x120]=255;s14_officer_decode_character(raw,&a);CHECK(a.loyalty==255);
    a.ambition=3;a.bond=4;a.loyalty=120;b.ambition=5;b.bond=2;b.loyalty=90;
    CHECK(s14_officer_compare(&a,&b,S14_SORT_AMBITION,1)>0);CHECK(s14_officer_compare(&a,&b,S14_SORT_BOND,1)<0);
    CHECK(s14_officer_compare(&a,&b,S14_SORT_LOYALTY,1)<0);CHECK(s14_officer_compare(&a,&b,S14_SORT_LOYALTY,0)>0);
    b.loyalty=-1;CHECK(s14_officer_compare(&a,&b,S14_SORT_LOYALTY,0)<0);CHECK(s14_officer_compare(&a,&b,S14_SORT_LOYALTY,1)<0);
    wcscpy(f.query,L"zy sz");ULONGLONG search_start=GetTickCount64();
    for (int j=0;j<14000;j++) if (!s14_officer_matches(&a,2,&f)) return 1;
    ULONGLONG search_ms=GetTickCount64()-search_start;CHECK(search_ms<2000);
    wcscpy(f.query,L"  \t ");f.own_force=1;CHECK(s14_officer_matches(&a,2,&f));CHECK(!s14_officer_matches(&b,2,&f));CHECK(!s14_officer_matches(&a,0,&f));
    f.own_force=0;f.place=1;CHECK(!s14_officer_matches(&a,2,&f));f.place=2;CHECK(s14_officer_matches(&a,2,&f));f.place=0;
    for (int state=0;state<10;state++) { a.status=state;CHECK(s14_officer_matches(&a,2,&f)==(state!=0 && state!=7 && state!=9)); }
    f.include_history=1;a.status=9;CHECK(s14_officer_matches(&a,2,&f));a.status=4;
    CHECK(s14_officer_compare(&a,&b,S14_SORT_LEADERSHIP,1)>0);CHECK(s14_officer_compare(&a,&b,S14_SORT_LEADERSHIP,0)<0);
    CHECK(s14_officer_compare(&a,&b,S14_SORT_TOTAL,1)<0);CHECK(s14_officer_compare(&a,&b,S14_SORT_TROOPS,1)>0);
    a.troops=-1;CHECK(s14_officer_compare(&a,&b,S14_SORT_TROOPS,0)>0);CHECK(s14_officer_compare(&a,&b,S14_SORT_TROOPS,1)>0);
    a.career.valid_mask=S14_CAREER_KILLS;a.career.officer_kills=0;
    CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,1)<0);CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,0)<0);
    b.career.valid_mask=S14_CAREER_KILLS;b.career.officer_kills=4;CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,1)>0);
    a.career.valid_mask=b.career.valid_mask=0;CHECK(s14_officer_compare(&a,&b,S14_SORT_ROUTS,1)>0);CHECK(s14_officer_compare(&a,&a,S14_SORT_ROUTS,1)==0);
    S14OfficerSnapshot *s=calloc(1,sizeof(*s));CHECK(s!=NULL);CHECK(!s14_officers_capture(no_read,NULL,1,s));CHECK(s->read_errors==1);
    s->count=2;s->rows[0]=a;s->rows[1]=b;
    S14OfficerCareerProvider provider={1,L"test-campaign",L"branch-a",NULL,career};
    s14_officers_attach_career(s,&provider);CHECK(s->rows[0].career.valid_mask==S14_CAREER_KILLS);CHECK(s->rows[0].career.officer_kills==0);CHECK(!s->rows[1].career.valid_mask);
    provider.branch_id=NULL;s14_officers_attach_career(s,&provider);CHECK(!s->rows[0].career.valid_mask);
    provider.branch_id=L"branch-a";provider.version=99;s14_officers_attach_career(s,&provider);CHECK(!s->rows[0].career.valid_mask);
    S14BattleStatsSnapshot *stats=calloc(1,sizeof(*stats));CHECK(stats!=NULL);stats->version=1;s->world=stats->world=7;stats->bound_to_save=1;stats->start_day=100;
    stats->rows[a.id]=(S14BattleTotals){.valid_mask=31,.enemy_loss=5000,.units_routed=2,.units_defeated=1,.own_loss=88,.officers_injured=1};
    s14_officers_attach_battle(s,stats);CHECK(s->battle_connected && s->battle_bound && s->rows[0].battle.enemy_loss==5000 && !s->rows[1].battle.valid_mask);
    CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_ENEMY_LOSS,1)<0);CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_DEFEATS,0)<0);
    S14Officer high=s->rows[1];high.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=5001,.units_routed=3,.units_defeated=0};
    CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,1)>0);CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,0)<0);
    CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ROUTS,1)>0);CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_DEFEATS,1)<0);
    high.battle.enemy_loss=UINT64_MAX;CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,1)>0);
    CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_OWN_LOSS,0)<0);CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_INJURIES,1)<0);
    stats->world=8;s14_officers_attach_battle(s,stats);CHECK(!s->battle_connected && !s->rows[0].battle.valid_mask);stats->world=7;
    S14OfficerUI ui={0};CHECK(s14_officer_ui_create(&ui,GetModuleHandleW(NULL),NULL,1));
    CHECK(Header_GetItemCount(ListView_GetHeader(ui.list))==21);CHECK(SendMessageW(ui.sort,CB_GETCOUNT,0,0)==21);
    ui.snapshot->count=2;ui.snapshot->player_force=2;ui.snapshot->rows[0]=a;ui.snapshot->rows[1]=b;
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);CHECK(ui.visible_count==2);
    SetWindowTextW(ui.query,L"山战");CHECK(ui.visible_count==1);CHECK(ui.snapshot->rows[ui.indices[0]].id==613);
    ListView_SetItemState(ui.list,0,LVIS_SELECTED,LVIS_SELECTED);wchar_t info[4096];GetWindowTextW(ui.details,info,4096);CHECK(wcsstr(info,L"张嶷")!=NULL);CHECK(wcsstr(info,L"未采集")!=NULL);
    CHECK(wcsstr(info,L"野心 3 / 情义 4 / 忠诚 120")!=NULL);
    SetWindowTextW(ui.query,L"zy sz");CHECK(ui.visible_count==1);CHECK(ui.snapshot->rows[ui.indices[0]].id==613);
    wchar_t value[32];ListView_GetItemText(ui.list,0,14,value,32);CHECK(!wcscmp(value,L"3"));
    ListView_GetItemText(ui.list,0,15,value,32);CHECK(!wcscmp(value,L"4"));ListView_GetItemText(ui.list,0,16,value,32);CHECK(!wcscmp(value,L"120"));
    ui.snapshot->world=7;s14_officers_attach_battle(ui.snapshot,stats);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    ListView_GetItemText(ui.list,0,17,value,32);CHECK(!wcscmp(value,L"5000"));ListView_GetItemText(ui.list,0,13,value,32);CHECK(!wcscmp(value,L"2"));
    ListView_GetItemText(ui.list,0,18,value,32);CHECK(!wcscmp(value,L"1"));ListView_GetItemText(ui.list,0,19,value,32);CHECK(!wcscmp(value,L"88"));
    ListView_SetItemState(ui.list,0,LVIS_SELECTED,LVIS_SELECTED);GetWindowTextW(ui.details,info,4096);CHECK(wcsstr(info,L"存档战绩") && wcsstr(info,L"杀敌数（含伤兵）5000"));
    ui.snapshot->rows[0].battle.valid_mask=0;SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);GetWindowTextW(ui.details,info,4096);CHECK(wcsstr(info,L"杀敌数（含伤兵）未采集"));
    ui.snapshot->rows[0].battle.valid_mask=31;s14_officers_attach_battle(ui.snapshot,stats);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);free(stats);
    if (argc>1) {
        SetWindowTextW(ui.query,L"");ListView_SetItemState(ui.list,-1,0,LVIS_SELECTED);ListView_SetItemState(ui.list,0,LVIS_SELECTED,LVIS_SELECTED);
        CHECK(ListView_GetNextItem(ui.list,-1,LVNI_SELECTED)==0);CHECK(ui.selected_id==613);
        ui.last_capture_ok=1;wcscpy(ui.notice,L"测试数据预览 · 实时字段只读 · 野心与情义 1～5 级");
        SetWindowPos(ui.window,NULL,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);CHECK(render(&ui,argv[1]));ShowWindow(ui.window,SW_HIDE);
    }
    SetWindowTextW(ui.query,L"");SendMessageW(ui.scope,CB_SETCURSEL,1,0);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(502,CBN_SELCHANGE),(LPARAM)ui.scope);CHECK(ui.visible_count==1);
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(505,CBN_SELCHANGE),(LPARAM)ui.sort);
    ui.snapshot->rows[0].career.valid_mask=S14_CAREER_KILLS;ui.snapshot->rows[0].career.officer_kills=7;
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    ListView_SetItemState(ui.list,0,LVIS_SELECTED,LVIS_SELECTED);GetWindowTextW(ui.details,info,4096);CHECK(wcsstr(info,L"击杀武将 7")!=NULL);
    SetWindowTextW(ui.query,L"ZZZ");CHECK(!ui.visible_count);
    SetWindowPos(ui.window,NULL,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);CHECK(IsWindowVisible(ui.window));
    HWND launcher=CreateWindowExW(0,L"STATIC",L"launcher fixture",WS_OVERLAPPEDWINDOW,0,0,1,1,NULL,NULL,GetModuleHandleW(NULL),NULL);
    HWND game=CreateWindowExW(0,L"STATIC",L"game fixture",WS_OVERLAPPEDWINDOW,0,0,1,1,launcher,NULL,GetModuleHandleW(NULL),NULL);
    HWND unrelated=CreateWindowExW(0,L"STATIC",L"other app fixture",WS_OVERLAPPEDWINDOW,0,0,1,1,NULL,NULL,GetModuleHandleW(NULL),NULL);
    CHECK(launcher && game && unrelated);ui.owner=game;SetWindowLongPtrW(ui.window,GWLP_HWNDPARENT,(LONG_PTR)game);
    CHECK(GetWindow(ui.window,GW_OWNER)==game);
    CHECK(GetAncestor(ui.window,GA_ROOTOWNER)!=game); /* Reproduce the previous misclassification. */
    CHECK(s14_owned_foreground(game,NULL,ui.window));CHECK(s14_owned_foreground(game,ui.window,ui.query));
    HWND combo_popup=CreateWindowExW(0,L"STATIC",L"dropdown fixture",WS_POPUP,0,0,1,1,ui.window,NULL,GetModuleHandleW(NULL),NULL);CHECK(combo_popup!=NULL);
    CHECK(s14_owned_foreground(game,ui.window,combo_popup));CHECK(!s14_owned_foreground(game,ui.window,unrelated));CHECK(!s14_owned_foreground(game,ui.window,NULL));
    SendMessageW(ui.window,WM_ACTIVATE,WA_INACTIVE,(LPARAM)unrelated);
    SendMessageW(ui.window,WM_ACTIVATEAPP,FALSE,0);
    ULONGLONG inactive_start=GetTickCount64();int inactive_polls=0;
    while (GetTickCount64()-inactive_start<1200) {
        if (s14_officer_ui_sync_enabled(&ui,1) || !IsWindowVisible(ui.window)) { fprintf(stderr,"FAIL: inactive view closed\n");return 1; }
        SendMessageW(ui.window,WM_NULL,0,0);Sleep(20);inactive_polls++;
    }
    CHECK(inactive_polls>0);CHECK(IsWindowVisible(ui.window));
    SetWindowTextW(ui.query,L"山战");CHECK(ui.visible_count==1);CHECK(IsWindowVisible(ui.window));
    SendMessageW(ui.scope,CB_SHOWDROPDOWN,TRUE,0);CHECK(IsWindowVisible(ui.window));SendMessageW(ui.scope,CB_SHOWDROPDOWN,FALSE,0);
    ui.last_capture_ok=1;wcscpy(ui.notice,L"opened snapshot fixture");
    S14OfficerSnapshot *opened=ui.snapshot;int errors=ui.candidate->read_errors;
    for (int j=0;j<5;j++) SendMessageW(ui.window,WM_TIMER,1,0);
    CHECK(IsWindowVisible(ui.window));CHECK(ui.snapshot==opened);CHECK(ui.candidate->read_errors==errors);
    CHECK(ui.last_capture_ok==1);CHECK(!wcscmp(ui.notice,L"opened snapshot fixture"));
    SetWindowTextW(ui.query,L"zy");CHECK(ui.visible_count==1);
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(506,BN_CLICKED),(LPARAM)ui.direction);
    CHECK(ui.snapshot==opened);CHECK(ui.candidate->read_errors==errors);CHECK(ui.last_capture_ok==1);
    SendMessageW(ui.close_button,BM_CLICK,0,0);CHECK(!IsWindowVisible(ui.window));
    SetWindowPos(ui.window,NULL,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    CHECK(!s14_officer_ui_sync_enabled(&ui,1));CHECK(IsWindowVisible(ui.window));
    SendMessageW(ui.query,WM_KEYDOWN,VK_ESCAPE,0);CHECK(!IsWindowVisible(ui.window));
    SetWindowPos(ui.window,NULL,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    CHECK(s14_officer_ui_sync_enabled(&ui,0)==1);CHECK(!IsWindowVisible(ui.window));
    SetWindowPos(ui.window,NULL,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    SendMessageW(ui.window,WM_COMMAND,IDCANCEL,0);CHECK(!IsWindowVisible(ui.window));
    SendMessageW(ui.window,WM_CLOSE,0,0);CHECK(!IsWindowVisible(ui.window));
    s14_officer_ui_destroy(&ui);DestroyWindow(combo_popup);DestroyWindow(game);DestroyWindow(launcher);DestroyWindow(unrelated);free(s);
    printf("{\"checks\":%d,\"status\":\"passed\",\"read_only\":true,\"career_unknown_distinct_from_zero\":true,\"nested_owner_focus\":true,\"focus_changes_do_not_hide\":true,\"search_and_dropdown_keep_open\":true,\"manual_close_button\":true,\"pinyin_and_initials\":true,\"polyphonic_names\":true,\"inner_attributes\":true,\"search_14000_ms\":%llu,\"inactive_duration_ms\":1200}\n",checks,(unsigned long long)search_ms);return 0;
}
