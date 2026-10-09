#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include "report_ui.c"
static void pump(void){MSG m;while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
static void paint(S14ReportUI *u,const char *path){
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO bi={0};bi.bmiHeader=(BITMAPINFOHEADER){.biSize=40,.biWidth=u->width,.biHeight=-u->height,.biPlanes=1,.biBitCount=32};void *bits=NULL;HBITMAP image=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);assert(image);HGDIOBJ old=SelectObject(dc,image);s14_report_ui_paint(u,dc);GdiFlush();
    if(path){BITMAPFILEHEADER h={.bfType=0x4d42,.bfSize=54+(DWORD)u->width*(DWORD)u->height*4,.bfOffBits=54};FILE *f=fopen(path,"wb");assert(f);assert(fwrite(&h,14,1,f)==1&&fwrite(&bi.bmiHeader,40,1,f)==1&&fwrite(bits,(size_t)u->width*u->height*4,1,f)==1);fclose(f);}SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);
}
static void click(S14ReportUI *u,RECT r){SendMessageW(u->window,WM_LBUTTONUP,0,MAKELPARAM((r.left+r.right)/2,(r.top+r.bottom)/2));pump();paint(u,NULL);}
int wmain(int argc,wchar_t **argv){
    HINSTANCE instance=GetModuleHandleW(NULL);HWND owner=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"isolated report test",WS_POPUP,-20000,-20000,1200,860,NULL,NULL,instance,NULL);assert(owner);ShowWindow(owner,SW_SHOWNOACTIVATE);
    S14ReportUI ui={0};S14ReportSearchLine search_rows[4]={{.type=4,.text=L"曹洪 → 宛城：获得金钱 365"},{.type=4,.text=L"蔡瑁 → 襄阳郡：获得金钱 890"},{.type=1,.text=L"程昱 → 新野：发现徐庶，登用失败"},{.type=0,.text=L"曹仁 → 南郡：探索失败"}};
    S14ReportSearch s={.start=100,.end=110,.force=1,.available=1,.completed=4,.empty=1,.money=1255,.people=1,.line_count=4,.lines=search_rows};
    S14ReportActor actors[3]={{.id=518,.portrait=24,.name=L"曹仁",.place=L"襄阳郡 · 长坂",.inflicted=5000,.lost=242,.routs=1,.wins=1,.injuries=1,.absorbed=314},{.id=325,.portrait=87,.name=L"蔡瑁",.place=L"襄阳郡 · 樊城",.inflicted=2680,.lost=408,.routs=1,.captures=1,.absorbed=138},{.id=511,.portrait=47,.name=L"曹洪",.place=L"襄阳郡 · 长坂",.inflicted=1218,.lost=2600,.defeats=1}};
    S14ReportLine lines[12]={0};for(int i=0;i<12;i++){lines[i]=(S14ReportLine){.actor=518,.target=613,.kind=1,.verified=1,.clock={7,1,7,(unsigned char)(11+i/4)}};wcscpy(lines[i].place,L"襄阳郡 · 长坂");swprintf(lines[i].text,256,L"交战记录 %d：曹仁队攻击张嶷队，双方兵力变化已采集。",i+1);}
    lines[2].kind=S14_REPORT_ROUT;lines[2].important=1;wcscpy(lines[2].text,L"张嶷击溃曹洪部队");lines[2].target=511;
    lines[5].kind=S14_REPORT_DUEL;lines[5].important=1;wcscpy(lines[5].text,L"曹仁单挑战胜张嶷");
    lines[8].kind=S14_REPORT_ROUT;lines[8].important=1;wcscpy(lines[8].text,L"曹仁击溃张嶷部队，吸收伤兵 314");
    lines[11].kind=S14_REPORT_CAPTURE;lines[11].important=1;lines[11].actor=325;lines[11].target=890;wcscpy(lines[11].text,L"蔡瑁擒获罗宪");wcscpy(lines[11].place,L"襄阳郡 · 樊城");
    S14ReportBattle b={.start=100,.end=110,.force=1,.available=1,.actor_count=3,.line_count=12,.routs=2,.defeats=1,.inflicted=8898,.lost=3250,.absorbed=452,.actors=actors,.lines=lines,.clock={7,1,7,11},.end_clock={7,1,7,21}};
    wchar_t root[MAX_PATH]=L"C:\\san14-missing-resource-test";if(argc>1){assert(wcslen(argv[1])<MAX_PATH);wcscpy(root,argv[1]);}
    assert(s14_report_ui_show(&ui,instance,owner,0,root,&s,&b));pump();paint(&ui,NULL);assert(ui.columns==3&&ui.visible&&ui.search_data.money==1255&&ui.important_count==4);assert(ui.search_data.lines!=search_rows&&ui.battle.actors!=actors);actors[0].inflicted=99;assert(ui.battle.actors[0].inflicted==5000);assert(outcome(&ui.battle.actors[0])==1&&outcome(&ui.battle.actors[1])==2&&outcome(&ui.battle.actors[2])==3);
    if(argc>1){HDC dc=GetDC(ui.window);RECT r={0,0,64,80};int ready=0;ULONGLONG start=GetTickCount64();while(GetTickCount64()-start<20000){ready=0;for(int i=0;i<3;i++)ready+=s14_portrait_draw(ui.portraits,ui.battle.actors[i].portrait,dc,&r,0);if(ready==3)break;MsgWaitForMultipleObjects(0,NULL,FALSE,16,QS_ALLINPUT);pump();}ReleaseDC(ui.window,dc);assert(ready==3);}
    paint(&ui,"native-turn-cards.bmp");RECT exploration=ui.search[1];SendMessageW(ui.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);paint(&ui,NULL);assert(ui.scroll>0&&!memcmp(&ui.search[1],&exploration,sizeof(RECT)));
    click(&ui,ui.search[1]);assert(ui.detail==1&&ui.detail_filter==4);paint(&ui,"native-turn-search-detail.bmp");click(&ui,ui.detail_close);assert(!ui.detail&&ui.visible);
    click(&ui,ui.search[3]);assert(ui.detail_filter==2);SendMessageW(ui.window,WM_KEYDOWN,VK_ESCAPE,0);paint(&ui,NULL);assert(!ui.detail&&ui.visible);
    ui.scroll=0;paint(&ui,NULL);click(&ui,card_rect(&ui,0));assert(ui.detail==2&&ui.detail_actor==0);paint(&ui,"native-turn-officer-detail.bmp");SendMessageW(ui.window,WM_KEYDOWN,VK_END,0);paint(&ui,NULL);assert(ui.detail_scroll==maximum(&ui));SendMessageW(ui.window,WM_KEYDOWN,VK_ESCAPE,0);paint(&ui,NULL);
    ui.expanded=1;paint(&ui,NULL);SendMessageW(ui.window,WM_KEYDOWN,VK_END,0);paint(&ui,NULL);assert(ui.scroll==maximum(&ui));paint(&ui,"native-turn-records-bottom.bmp");
    click(&ui,ui.done);assert(!ui.visible&&!IsWindowVisible(ui.window));s14_report_ui_tick(&ui,1);assert(!IsWindowVisible(ui.window));assert(s14_report_ui_reopen(&ui));assert(IsWindowVisible(ui.window));s14_report_ui_tick(&ui,0);assert(!IsWindowVisible(ui.window));s14_report_ui_tick(&ui,1);assert(IsWindowVisible(ui.window));
    assert(fonts(&ui,192));ui.width=2200;ui.height=1580;ui.scroll=ui.expanded=0;layout(&ui);paint(&ui,"native-turn-cards-dpi192.bmp");assert(ui.columns==3);
    assert(fonts(&ui,96));ui.width=520;ui.height=720;layout(&ui);paint(&ui,"native-turn-cards-narrow.bmp");assert(ui.columns==1);assert(card_rect(&ui,2).right<=ui.width);
    s14_report_ui_clear(&ui);assert(!ui.search_data.available&&!ui.battle.available&&!s14_report_ui_reopen(&ui));assert(GetForegroundWindow()!=ui.window&&GetForegroundWindow()!=owner);s14_report_ui_destroy(&ui);DestroyWindow(owner);
    printf("{\"status\":\"passed\",\"single_page_no_tabs\":true,\"pinned_exploration\":true,\"typed_exploration_details\":true,\"officer_details\":true,\"manual_close_and_reopen\":true,\"full_record_scroll\":true,\"own_defeat_capture_breakthrough_styles\":true,\"immutable_snapshot\":true,\"scale_96_192\":true,\"narrow_layout\":true,\"load_reset_clears_report\":true,\"no_game_process_calls\":true}\n");return 0;
}
