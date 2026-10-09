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
static int menu_pumps;
static void test_menu_pump(void){menu_pumps++;}
static int no_read(void *context,uintptr_t address,void *out,size_t size) { (void)context;(void)address;(void)out;(void)size;return 0; }
static int career(void *context,const wchar_t *campaign,const wchar_t *branch,int id,S14OfficerCareer *out) {
    (void)context;if (wcscmp(campaign,L"test-campaign") || wcscmp(branch,L"branch-a") || id!=613) return 0;
    out->valid_mask=S14_CAREER_KILLS;out->officer_kills=0;return 1;
}
static int render_window(HWND window,const char *path) {
    RECT r;GetClientRect(window,&r);int width=r.right,height=r.bottom;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,NULL,0);HGDIOBJ old=SelectObject(dc,bitmap);
    int ok=PrintWindow(window,dc,PW_CLIENTONLY);GdiFlush();
    BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+width*height*4;
    FILE *f=fopen(path,"wb");if (!f) ok=0;
    if (f) { ok=ok && fwrite(&header,sizeof(header),1,f)==1 && fwrite(&bi.bmiHeader,sizeof(BITMAPINFOHEADER),1,f)==1 && fwrite(bits,width*height*4,1,f)==1;fclose(f); }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);return ok;
}
static int render(S14OfficerUI *ui,const char *path){return render_window(ui->window,path);}
static void value(S14OfficerUI *ui,int key,int row,wchar_t text[128]){
    HWND tables[]={ui->pinned,ui->list};for(int fixed=1;fixed>=0;fixed--){
        HWND table=tables[fixed?0:1];int n=Header_GetItemCount(ListView_GetHeader(table));
        for(int col=0;col<n;col++)if(s14_officer_ui_column(ui,fixed,col)==key){ListView_GetItemText(table,row,col,text,128);return;}
    }CHECK(0);
}
static int sort_index(S14OfficerUI *ui,int key){for(int i=0;i<ui->sort_count;i++)if(ui->sort_keys[i]==key)return i;return -1;}
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
    CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,1)>0);CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,0)>0);
    b.career.valid_mask=S14_CAREER_KILLS;b.career.officer_kills=4;CHECK(s14_officer_compare(&a,&b,S14_SORT_KILLS,1)>0);
    a.career.valid_mask=b.career.valid_mask=0;CHECK(s14_officer_compare(&a,&b,S14_SORT_ROUTS,1)>0);CHECK(s14_officer_compare(&a,&a,S14_SORT_ROUTS,1)==0);
    a.special=(S14SpecialTotals){.valid_mask=3,.duels=3,.wins=2,.losses=1,.captures=3,.captured=0,.unique_captives=2};
    for(int key=S14_SORT_DUELS;key<=S14_SORT_UNIQUE_CAPTIVES;key++){
        CHECK(s14_officer_compare(&a,&b,key,0)>0);CHECK(s14_officer_compare(&a,&b,key,1)==(key==S14_SORT_CAPTURED?1:-1));
        S14Officer more=b;more.special=(S14SpecialTotals){.valid_mask=3,.duels=4,.wins=3,.losses=2,.captures=4,.captured=1,.unique_captives=3};
        CHECK(s14_officer_compare(&a,&more,key,0)<0);CHECK(s14_officer_compare(&a,&more,key,1)>0);
    }
    memset(&a.special,0,sizeof(a.special));
    S14Officer ratio_a={0},ratio_b={0};ratio_a.id=1;ratio_b.id=2;
    ratio_a.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=9613,.own_loss=349};
    ratio_b.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=8185,.own_loss=5431};
    wchar_t ratio_text[32];s14_officer_kda_text(&ratio_a,ratio_text,32);CHECK(!wcscmp(ratio_text,L"27.54"));
    CHECK(s14_officer_compare(&ratio_a,&ratio_b,S14_SORT_KDA,1)<0);
    ratio_b.battle.own_loss=0;s14_officer_kda_text(&ratio_b,ratio_text,32);CHECK(!wcscmp(ratio_text,L"∞"));CHECK(s14_officer_compare(&ratio_a,&ratio_b,S14_SORT_KDA,1)>0);
    ratio_b.battle.enemy_loss=0;s14_officer_kda_text(&ratio_b,ratio_text,32);CHECK(!wcscmp(ratio_text,L"0.00"));
    ratio_a.battle.valid_mask=0;s14_officer_kda_text(&ratio_a,ratio_text,32);CHECK(!wcscmp(ratio_text,L"0.00"));
    ratio_a.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=UINT64_MAX,.own_loss=UINT64_MAX-1};ratio_b.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=UINT64_MAX-1,.own_loss=UINT64_MAX-2};
    CHECK(s14_officer_compare(&ratio_a,&ratio_b,S14_SORT_KDA,0)<0); // Exact rational ordering, beyond double precision.

    S14OfficerSnapshot *s=calloc(1,sizeof(*s));CHECK(s!=NULL);CHECK(!s14_officers_capture(no_read,NULL,1,s));CHECK(s->read_errors==1);
    s->count=2;s->rows[0]=a;s->rows[1]=b;
    S14OfficerCareerProvider provider={1,L"test-campaign",L"branch-a",NULL,career};
    s14_officers_attach_career(s,&provider);CHECK(s->rows[0].career.valid_mask==S14_CAREER_KILLS);CHECK(s->rows[0].career.officer_kills==0);CHECK(!s->rows[1].career.valid_mask);
    provider.branch_id=NULL;s14_officers_attach_career(s,&provider);CHECK(!s->rows[0].career.valid_mask);
    provider.branch_id=L"branch-a";provider.version=99;s14_officers_attach_career(s,&provider);CHECK(!s->rows[0].career.valid_mask);
    S14BattleStatsSnapshot *stats=calloc(1,sizeof(*stats));CHECK(stats!=NULL);stats->version=1;s->world=stats->world=7;stats->bound_to_save=1;stats->start_day=100;
    stats->rows[a.id]=(S14BattleTotals){.valid_mask=31,.enemy_loss=5000,.units_routed=2,.units_defeated=1,.own_loss=88,.officers_injured=1};
    s14_officers_attach_battle(s,stats);CHECK(s->battle_connected && s->battle_bound && s->rows[0].battle.enemy_loss==5000 && !s->rows[1].battle.valid_mask);
    CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_ENEMY_LOSS,1)<0);CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_DEFEATS,0)>0);
    S14Officer high=s->rows[1];high.battle=(S14BattleTotals){.valid_mask=31,.enemy_loss=5001,.units_routed=3,.units_defeated=0};
    CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,1)>0);CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,0)<0);
    CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ROUTS,1)>0);CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_DEFEATS,1)<0);
    high.battle.enemy_loss=UINT64_MAX;CHECK(s14_officer_compare(&s->rows[0],&high,S14_SORT_ENEMY_LOSS,1)>0);
    CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_OWN_LOSS,0)>0);CHECK(s14_officer_compare(&s->rows[0],&s->rows[1],S14_SORT_INJURIES,1)<0);
    stats->world=8;s14_officers_attach_battle(s,stats);CHECK(!s->battle_connected && !s->rows[0].battle.valid_mask);stats->world=7;
    S14OfficerUI ui={0};CHECK(s14_officer_ui_create(&ui,GetModuleHandleW(NULL),NULL,1));
    CHECK(!GetDlgItem(ui.window,508));CHECK(Header_GetItemCount(ListView_GetHeader(ui.pinned))==3);
    CHECK(Header_GetItemCount(ListView_GetHeader(ui.list))==11);CHECK(SendMessageW(ui.sort,CB_GETCOUNT,0,0)==14);CHECK(ui.active_tab==0);
    for(int col=0;col<3;col++){int key=s14_officer_ui_column(&ui,1,col);CHECK(key==(col==0?S14_SORT_NAME:col==1?S14_SORT_FORCE:S14_SORT_LOCATION));}
    CHECK(sort_index(&ui,S14_SORT_AMBITION)<0 && sort_index(&ui,S14_SORT_CAPTURES)>=0);
    const int ordered[]={S14_SORT_ENEMY_LOSS,S14_SORT_OWN_LOSS,S14_SORT_KDA,S14_SORT_ROUTS,S14_SORT_DEFEATS,S14_SORT_KILLS,S14_SORT_INJURIES,S14_SORT_DUEL_WINS,S14_SORT_DUEL_LOSSES,S14_SORT_CAPTURES,S14_SORT_CAPTURED};
    for(int i=0;i<11;i++)CHECK(s14_officer_ui_column(&ui,0,i)==ordered[i]);
    CHECK(sort_index(&ui,S14_SORT_DUELS)<0 && sort_index(&ui,S14_SORT_UNIQUE_CAPTIVES)<0);
    for(int i=0;i<11;i++){wchar_t title[128];LVCOLUMNW col={0};col.mask=LVCF_TEXT;col.pszText=title;col.cchTextMax=128;CHECK(ListView_GetColumn(ui.list,i,&col));CHECK(wcslen(title)==4);}

    a.ability[2]=76;a.ability[3]=67;a.ability[4]=84;b.ability[2]=58;b.ability[3]=46;b.ability[4]=78;
    wcscpy(a.force_name,L"刘备");wcscpy(b.force_name,L"曹操");wcscpy(a.location,L"出征 · 张嶷队");wcscpy(b.location,L"出征 · 曹仁队");
    ui.snapshot->count=2;ui.snapshot->player_force=2;ui.snapshot->rows[0]=a;ui.snapshot->rows[1]=b;
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);CHECK(ui.visible_count==2);
    SetWindowTextW(ui.query,L"山战");CHECK(ui.visible_count==1);CHECK(ui.snapshot->rows[ui.indices[0]].id==613);
    wchar_t text[128];value(&ui,S14_SORT_NAME,0,text);CHECK(!wcscmp(text,L"张嶷"));value(&ui,S14_SORT_LOCATION,0,text);CHECK(!wcscmp(text,L"出征 · 张嶷队"));
    value(&ui,S14_SORT_CAPTURES,0,text);CHECK(!wcscmp(text,L"0"));value(&ui,S14_SORT_CAPTURED,0,text);CHECK(!wcscmp(text,L"0"));value(&ui,S14_SORT_KDA,0,text);CHECK(!wcscmp(text,L"0.00"));
    ui.snapshot->rows[0].special.captures=99;value(&ui,S14_SORT_CAPTURES,0,text);CHECK(!wcscmp(text,L"0"));CHECK(!ui.snapshot->rows[0].special.valid_mask);
    ListView_SetItemState(ui.pinned,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    CHECK(ui.selected_id==613 && ListView_GetNextItem(ui.list,-1,LVNI_SELECTED)==0);
    SendMessageW(ui.tabs[1],BM_CLICK,0,0);CHECK(ui.active_tab==1);CHECK(sort_index(&ui,S14_SORT_CAPTURES)<0 && sort_index(&ui,S14_SORT_AMBITION)>=0);
    value(&ui,S14_SORT_AMBITION,0,text);CHECK(!wcscmp(text,L"3"));value(&ui,S14_SORT_BOND,0,text);CHECK(!wcscmp(text,L"4"));value(&ui,S14_SORT_LOYALTY,0,text);CHECK(!wcscmp(text,L"120"));
    CHECK(ui.selected_id==613 && ListView_GetNextItem(ui.pinned,-1,LVNI_SELECTED)==0 && ListView_GetNextItem(ui.list,-1,LVNI_SELECTED)==0);
    SendMessageW(ui.sort,CB_SETCURSEL,sort_index(&ui,S14_SORT_AMBITION),0);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(505,CBN_SELCHANGE),(LPARAM)ui.sort);CHECK(ui.filter.sort==S14_SORT_AMBITION);
    SendMessageW(ui.query,WM_KEYDOWN,VK_TAB,0);CHECK(ui.active_tab==0 && ui.filter.sort==S14_SORT_ENEMY_LOSS);
    SendMessageW(ui.query,WM_KEYDOWN,VK_TAB,0x40000000);CHECK(ui.active_tab==0); // Holding Tab does not oscillate.
    SendMessageW(ui.scope,WM_KEYDOWN,VK_TAB,0);CHECK(ui.active_tab==1 && ui.filter.sort==S14_SORT_AMBITION);
    SendMessageW(ui.query,WM_CHAR,L'\t',0);GetWindowTextW(ui.query,text,128);CHECK(!wcscmp(text,L"山战"));
    SetWindowTextW(ui.query,L"zy sz");CHECK(ui.visible_count==1);CHECK(ui.snapshot->rows[ui.indices[0]].id==613);
    S14OfficerSnapshot *tab_snapshot=ui.snapshot;int tab_errors=ui.candidate->read_errors;
    s14_officer_ui_set_tab(&ui,0);CHECK(ui.snapshot==tab_snapshot && ui.candidate->read_errors==tab_errors);
    ui.snapshot->world=7;s14_officers_attach_battle(ui.snapshot,stats);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    value(&ui,S14_SORT_ENEMY_LOSS,0,text);CHECK(!wcscmp(text,L"5000"));value(&ui,S14_SORT_ROUTS,0,text);CHECK(!wcscmp(text,L"2"));value(&ui,S14_SORT_DEFEATS,0,text);CHECK(!wcscmp(text,L"1"));value(&ui,S14_SORT_OWN_LOSS,0,text);CHECK(!wcscmp(text,L"88"));
    ui.snapshot->rows[0].battle.valid_mask=0;value(&ui,S14_SORT_ENEMY_LOSS,0,text);CHECK(!wcscmp(text,L"0"));CHECK(ui.snapshot->rows[0].battle.enemy_loss==5000);
    s14_officers_attach_battle(ui.snapshot,stats);ui.snapshot->rows[0].special=(S14SpecialTotals){.valid_mask=1,.duels=3,.wins=2,.losses=1};
    value(&ui,S14_SORT_DUEL_WINS,0,text);CHECK(!wcscmp(text,L"2"));value(&ui,S14_SORT_CAPTURES,0,text);CHECK(!wcscmp(text,L"0"));
    // Neither pane needs horizontal scrolling; identities stay in the fixed pane.
    RECT fixed_before,fixed_after;GetWindowRect(ui.pinned,&fixed_before);ListView_Scroll(ui.list,500,0);GetWindowRect(ui.pinned,&fixed_after);
    CHECK(GetScrollPos(ui.list,SB_HORZ)==0 && GetScrollPos(ui.pinned,SB_HORZ)==0 && EqualRect(&fixed_before,&fixed_after));
    value(&ui,S14_SORT_NAME,0,text);CHECK(!wcscmp(text,L"张嶷"));ListView_Scroll(ui.list,-500,0);
    // Both panes retain the same row after scrolling, resizing and changing tabs.
    SetWindowTextW(ui.query,L"");ui.selected_id=0;ui.snapshot->count=120;
    for(int i=0;i<120;i++){ui.snapshot->rows[i]=a;ui.snapshot->rows[i].id=1000+i;swprintf(ui.snapshot->rows[i].name,24,L"测试武将 %03d",i);}
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    ListView_EnsureVisible(ui.list,90,FALSE);CHECK(ListView_GetTopIndex(ui.list)>0 && ListView_GetTopIndex(ui.list)==ListView_GetTopIndex(ui.pinned));
    SendMessageW(ui.pinned,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);CHECK(ListView_GetTopIndex(ui.list)==ListView_GetTopIndex(ui.pinned));
    SetWindowPos(ui.window,NULL,-20000,-20000,1020,640,SWP_NOACTIVATE|SWP_SHOWWINDOW);s14_officer_ui_set_tab(&ui,1);
    ListView_EnsureVisible(ui.pinned,119,FALSE);
    if(ListView_GetTopIndex(ui.list)!=ListView_GetTopIndex(ui.pinned)){RECT lp,rp,la,ra,lh,rh;GetClientRect(ui.pinned,&lp);GetClientRect(ui.list,&rp);ListView_GetItemRect(ui.pinned,ListView_GetTopIndex(ui.pinned),&la,LVIR_BOUNDS);ListView_GetItemRect(ui.list,ListView_GetTopIndex(ui.list),&ra,LVIR_BOUNDS);GetWindowRect(ListView_GetHeader(ui.pinned),&lh);GetWindowRect(ListView_GetHeader(ui.list),&rh);fprintf(stderr,"scroll mismatch left=%d right=%d heights=%ld,%ld pages=%d,%d rows=%ld:%ld,%ld:%ld headers=%ld,%ld styles=%llx,%llx\n",ListView_GetTopIndex(ui.pinned),ListView_GetTopIndex(ui.list),lp.bottom,rp.bottom,ListView_GetCountPerPage(ui.pinned),ListView_GetCountPerPage(ui.list),la.top,la.bottom,ra.top,ra.bottom,lh.bottom-lh.top,rh.bottom-rh.top,(unsigned long long)GetWindowLongPtrW(ui.pinned,GWL_STYLE),(unsigned long long)GetWindowLongPtrW(ui.list,GWL_STYLE));}
    CHECK(ListView_GetTopIndex(ui.list)==ListView_GetTopIndex(ui.pinned));
    RECT left_row,right_row;int top=ListView_GetTopIndex(ui.list);CHECK(ListView_GetItemRect(ui.list,top,&right_row,LVIR_BOUNDS) && ListView_GetItemRect(ui.pinned,top,&left_row,LVIR_BOUNDS));CHECK(left_row.top==right_row.top);

    s14_officer_ui_set_tab(&ui,0);CHECK(!(GetWindowLongPtrW(ui.list,GWL_STYLE)&WS_HSCROLL));
    HDC measure=GetDC(ui.list);HGDIOBJ old_font=SelectObject(measure,ui.dense_font);
    for(int i=0;i<11;i++){wchar_t title[128];LVCOLUMNW col={0};col.mask=LVCF_TEXT;col.pszText=title;col.cchTextMax=128;CHECK(ListView_GetColumn(ui.list,i,&col));SIZE extent;CHECK(GetTextExtentPoint32W(measure,title,4,&extent));CHECK(ListView_GetColumnWidth(ui.list,i)>=extent.cx+6);}
    SelectObject(measure,old_font);ReleaseDC(ui.list,measure);
    if(argc>1){char narrow[1024];snprintf(narrow,sizeof(narrow),"%s-narrow.bmp",argv[1]);CHECK(render(&ui,narrow));}
    // IsDialogMessage is the real game message-pump path; it must not consume Tab as focus navigation.
    int previous_tab=ui.active_tab;MSG tab={0};tab.hwnd=ui.query;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;
    if(!IsDialogMessageW(ui.window,&tab)){TranslateMessage(&tab);DispatchMessageW(&tab);}CHECK(ui.active_tab!=previous_tab);
    CHECK(ui.snapshot==tab_snapshot && ui.candidate->read_errors==tab_errors);
    SetWindowPos(ui.window,NULL,-20000,-20000,1500,880,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    ui.snapshot->count=2;ui.snapshot->rows[0]=a;ui.snapshot->rows[1]=b;ui.snapshot->rows[0].battle=stats->rows[a.id];ui.snapshot->rows[0].special=(S14SpecialTotals){.valid_mask=1,.duels=3,.wins=2,.losses=1};
    s14_officer_ui_set_tab(&ui,0);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    ListView_SetItemState(ui.list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);CHECK(ui.selected_id==613);
    CHECK(ListView_GetNextItem(ui.pinned,-1,LVNI_SELECTED)==0);
    RECT client,grid;GetClientRect(ui.window,&client);GetWindowRect(ui.list,&grid);MapWindowPoints(NULL,ui.window,(POINT*)&grid,2);CHECK(grid.bottom>=client.bottom-28);

    CHECK(IsWindowEnabled(ui.timeline_button));ui.timeline.snapshot->version=1;ui.timeline.snapshot->world=ui.snapshot->world;ui.timeline.snapshot->count=3;
    for(int i=0;i<3;i++){S14TimelineEvent *v=&ui.timeline.snapshot->events[i];*v=(S14TimelineEvent){.id=(uint64_t)i+1,.kind=i+1,.day=100,.actor=613,.target=518,.verified=1,.actor_role=S14_SPECIAL_ACTOR_PERSON,.place={.tile=31100,.city_id=24,.area_id=239,.basis=1}};v->clock[0]=7;v->clock[1]=1;v->clock[2]=7;v->clock[3]=11;wcscpy(v->actor_name,L"张嶷");wcscpy(v->target_name,L"曹仁");wcscpy(v->place.city,L"襄阳郡");wcscpy(v->place.area,L"长坂");}
    SendMessageW(ui.timeline_button,BM_CLICK,0,0);CHECK(IsWindowVisible(ui.timeline.window) && ui.timeline.count==3 && ui.timeline.officer.id==613);
    ListView_GetItemText(ui.timeline.list,0,1,text,128);CHECK(!wcscmp(text,L"襄阳郡 · 长坂"));
    ListView_GetItemText(ui.timeline.list,0,4,text,128);CHECK(!wcscmp(text,L"擒获 曹仁"));
    ListView_GetItemText(ui.timeline.list,1,4,text,128);CHECK(!wcscmp(text,L"与 曹仁 单挑 · 制胜"));
    CHECK(!(GetWindowLongPtrW(ui.timeline.list,GWL_STYLE)&WS_HSCROLL));
    if(argc>1){char history_path[1024];snprintf(history_path,sizeof(history_path),"%s-timeline.bmp",argv[1]);CHECK(render_window(ui.timeline.window,history_path));}
    SendMessageW(ui.timeline.close,BM_CLICK,0,0);CHECK(!IsWindowVisible(ui.timeline.window) && IsWindowVisible(ui.window));
    ui.last_capture_ok=1;wcscpy(ui.notice,L"独立测试预览 · Tab 切换 · 固定身份列");
    if(argc>1){CHECK(render(&ui,argv[1]));s14_officer_ui_set_tab(&ui,1);char raw_path[1024];snprintf(raw_path,sizeof(raw_path),"%s-raw.bmp",argv[1]);CHECK(render(&ui,raw_path));s14_officer_ui_set_tab(&ui,0);}
    free(stats);
    SetWindowTextW(ui.query,L"");SendMessageW(ui.scope,CB_SETCURSEL,1,0);SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(502,CBN_SELCHANGE),(LPARAM)ui.scope);CHECK(ui.visible_count==1);
    ui.snapshot->rows[0].career.valid_mask=S14_CAREER_KILLS;ui.snapshot->rows[0].career.officer_kills=7;value(&ui,S14_SORT_KILLS,0,text);CHECK(!wcscmp(text,L"7"));
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
    ui.pump_events=test_menu_pump;SendMessageW(ui.window,WM_TIMER,538,0);CHECK(menu_pumps==0);
    ui.menu_active=1;SendMessageW(ui.window,WM_TIMER,538,0);CHECK(menu_pumps==1);ui.menu_active=0;
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
    printf("{\"checks\":%d,\"status\":\"passed\",\"read_only\":true,\"career_unknown_distinct_from_zero\":true,\"nested_owner_focus\":true,\"focus_changes_do_not_hide\":true,\"search_and_dropdown_keep_open\":true,\"manual_close_button\":true,\"two_tabs\":true,\"tab_message_pump\":true,\"pinned_identity_columns\":true,\"synchronized_rows_and_scroll\":true,\"unknown_counts_display_zero\":true,\"archive_panel_removed\":true,\"no_horizontal_scroll\":true,\"four_character_battle_headers\":true,\"kda_exact_sort_and_zero_denominator\":true,\"officer_timeline_menu_and_window\":true,\"pinyin_and_initials\":true,\"polyphonic_names\":true,\"inner_attributes\":true,\"search_14000_ms\":%llu,\"inactive_duration_ms\":1200}\n",checks,(unsigned long long)search_ms);return 0;
}
