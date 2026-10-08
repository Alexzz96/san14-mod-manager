#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "interaction.h"
#include "toast.h"

#define REQUIRE(expr) do { if (!(expr)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#expr); exit(1); } } while (0)
static void pump(void) {
    MSG message;
    while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
}
static void save_preview(S14Toast *toast,const char *path) {
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=toast->width; info.bmiHeader.biHeight=-toast->height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(NULL); REQUIRE(dc!=NULL);
    void *pixels=NULL;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); REQUIRE(bitmap && pixels);
    HGDIOBJ previous=SelectObject(dc,bitmap); s14_toast_paint(toast,dc); GdiFlush();
    FILE *file=fopen(path,"wb"); REQUIRE(file!=NULL);
    DWORD size=(DWORD)(toast->width*toast->height*4);
    BITMAPFILEHEADER header={0}; header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader); header.bfSize=header.bfOffBits+size;
    REQUIRE(fwrite(&header,sizeof(header),1,file)==1);
    REQUIRE(fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1);
    REQUIRE(fwrite(pixels,size,1,file)==1); fclose(file);
    SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc);
}

int main(void) {
    const uintptr_t previews[]={0x6eebe0,0x7089fd,0x70d9bc,0x722571,0x724406};
    for (unsigned int i=0;i<sizeof(previews)/sizeof(previews[0]);i++) {
        int phase=s14_check_phase(0x2514d5,previews[i],0);
        REQUIRE(phase==S14_PHASE_PREVIEW);
        REQUIRE(s14_check_result(1,0,2,phase)==1);
        REQUIRE(s14_check_result(0,0,2,phase)==0);
    }
    REQUIRE(s14_check_phase(0x70d9fa,0,0)==S14_PHASE_PREVIEW);
    const uintptr_t shared[]={0x733cd,0x7370a,0x74245,0x74605};
    for (unsigned int i=0;i<sizeof(shared)/sizeof(shared[0]);i++) {
        REQUIRE(s14_check_phase(0x2514d5,shared[i],1)==S14_PHASE_PREVIEW);
        REQUIRE(s14_check_phase(0x2514d5,shared[i],0)==S14_PHASE_ENFORCE);
    }
    int commit=s14_check_phase(0x2514d5,0x70744c,1);
    REQUIRE(commit==S14_PHASE_COMMIT && s14_check_result(1,0,2,commit)==0);
    REQUIRE(s14_check_result(1,1,2,commit)==1);
    REQUIRE(s14_check_result(1,0,1,commit)==1);
    REQUIRE(s14_check_phase(0x2514d5,0x26015d,1)==S14_PHASE_ENFORCE);
    REQUIRE(s14_check_result(1,0,2,s14_check_phase(0x2514d5,0x26015d,1))==0);
    REQUIRE(s14_check_result(1,0,2,s14_check_phase(0x37894d,0,1))==0);
    REQUIRE(s14_check_phase(0x2514d5,0x123456,1)==S14_PHASE_ENFORCE);

    HINSTANCE instance=GetModuleHandleW(NULL);
    HWND owner=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"S14 private UI test",WS_POPUP,
        -20000,-20000,1920,1080,NULL,NULL,instance,NULL); REQUIRE(owner!=NULL);
    ShowWindow(owner,SW_SHOWNOACTIVATE);
    HWND foreground=GetForegroundWindow(); S14Toast toast={0};
    ULONGLONG started=GetTickCount64(); POINT click={-19500,-19500};
    REQUIRE(s14_toast_show(&toast,instance,owner,click,started)); pump();
    REQUIRE(IsWindowVisible(toast.window) && GetForegroundWindow()==foreground);
    REQUIRE(GetWindow(toast.window,GW_OWNER)==owner);
    REQUIRE(GetWindowLongPtrW(toast.window,GWL_EXSTYLE)&WS_EX_NOACTIVATE);
    REQUIRE(GetWindowLongPtrW(toast.window,GWL_EXSTYLE)&WS_EX_TRANSPARENT);
    REQUIRE(SendMessageW(toast.window,WM_NCHITTEST,0,0)==HTTRANSPARENT);
    REQUIRE(toast.deadline==started+5000);
    save_preview(&toast,"toast-preview.bmp");
    REQUIRE(!s14_toast_tick(&toast,started+4999,1) && IsWindowVisible(toast.window));
    REQUIRE(s14_toast_tick(&toast,started+5000,1)==1 && !IsWindowVisible(toast.window));
    REQUIRE(s14_toast_show(&toast,instance,owner,click,started));
    HWND first_window=toast.window;
    REQUIRE(s14_toast_show(&toast,instance,owner,click,started+3000));
    REQUIRE(toast.window==first_window && toast.deadline==started+8000);
    REQUIRE(!s14_toast_tick(&toast,started+5000,1));
    REQUIRE(s14_toast_tick(&toast,started+8000,1)==1);
    REQUIRE(s14_toast_show(&toast,instance,owner,click,started));
    REQUIRE(s14_toast_tick(&toast,started+1,0)==2 && !IsWindowVisible(toast.window));
    REQUIRE(s14_toast_text(&toast,instance,owner,click,started,L"开始自动搜索",L"已派遣 14 名武将。",2000));
    REQUIRE(toast.deadline==started+2000 && !wcscmp(toast.title,L"开始自动搜索"));
    REQUIRE(!s14_toast_tick(&toast,started+1999,1));
    REQUIRE(s14_toast_tick(&toast,started+2000,1)==1 && !IsWindowVisible(toast.window));
    wchar_t results[16384]={0}; size_t at=0;
    wcscpy(results,L"完成 120 次 · 金钱 321 · 探索失败 119 次\n\n"); at=wcslen(results);
    for (int i=1;i<=120;i++) { int written=swprintf(results+at,16384-at,L"%d. 张辽 → 襄阳郡：%ls\n",i,i==2?L"找到了金钱 321":L"探索失败（未发现任何收获）"); REQUIRE(written>0); at+=(size_t)written; }
    REQUIRE(s14_toast_report(&toast,instance,owner,started,results)); pump();
    REQUIRE(toast.window==first_window && toast.persistent && wcsstr(toast.report_text,L"120. 张辽 → 襄阳郡"));
    REQUIRE(!(GetWindowLongPtrW(toast.window,GWL_EXSTYLE)&WS_EX_TRANSPARENT));
    REQUIRE(SendMessageW(toast.window,WM_NCHITTEST,0,0)==HTCLIENT);
    REQUIRE(SendMessageW(toast.window,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);
    REQUIRE(!s14_toast_tick(&toast,started+600000,1) && IsWindowVisible(toast.window));
    REQUIRE(toast.content_height>toast.height && toast.scroll==0); save_preview(&toast,"search-report-preview.bmp");
    SendMessageW(toast.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0); REQUIRE(toast.scroll>0);
    for (int i=0;i<200;i++) SendMessageW(toast.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);
    REQUIRE(toast.scroll==toast.content_height-(toast.height-MulDiv(92,toast.scale,96)));
    save_preview(&toast,"search-report-bottom.bmp");
    REQUIRE(s14_toast_tick(&toast,started+600001,0)==2 && !IsWindowVisible(toast.window));
    REQUIRE(!s14_toast_tick(&toast,started+600002,1) && IsWindowVisible(toast.window));
    SendMessageW(toast.window,WM_LBUTTONDOWN,MK_LBUTTON,0); SendMessageW(toast.window,WM_LBUTTONUP,0,0);
    REQUIRE(!toast.deadline && !toast.persistent && !IsWindowVisible(toast.window));
    REQUIRE(!s14_toast_tick(&toast,started+600003,1) && !IsWindowVisible(toast.window));
    toast.scale=192; DeleteObject(toast.body_font); DeleteObject(toast.title_font);
    toast.body_font=CreateFontW(-32,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    toast.title_font=CreateFontW(-38,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    REQUIRE(s14_toast_report(&toast,instance,owner,started,results)); REQUIRE(toast.width==1440 && toast.height<=1032);
    save_preview(&toast,"search-report-dpi192.bmp");
    SendMessageW(toast.window,WM_LBUTTONUP,0,0); toast.scale=96;
    s14_toast_destroy(&toast);
    wchar_t battle_results[16384]={0};
    wcscpy(battle_results,L"自势力 · 本回合战斗报告\n参与武将 3 · 造成敌方兵力减少 5000 · 己方兵力损失 242\n击溃事件 1 · 击伤事件 1 · 吸收伤兵 263\n\n263 年 5 月下旬 → 263 年 6 月上旬\n\n士兵阵亡、新增伤兵与武将击杀：尚未核实，暂不显示计数。\n兵力减少包含受伤等变化；吸收伤兵单独统计。\n\n武将汇总\n曹仁 · 造成兵力减少 5000 · 自身损失 93 · 击溃 1 · 击伤 1 · 吸收伤兵 263\n王威 · 造成兵力减少 0 · 自身损失 81 · 战法效果 1\n\n战斗明细\n");
    at=wcslen(battle_results);
    for(int i=1;i<=80;i++) {int written=swprintf(battle_results+at,16384-at,L"%d. %ls\n",i,i%3==0?L"王威 → 罗宪：止步，状态已施加／延长":i%3==1?L"曹仁 → 张嶷：兵力 5000 → 2988，减少 2012":L"曹仁击溃张嶷部队；吸收伤兵 263");REQUIRE(written>0);at+=(size_t)written;}
    // Combined report: switching tabs is not dismissal, each scroll is kept,
    // and the close button really stops background restoration.
    REQUIRE(s14_toast_report_tabs(&toast,instance,owner,started,results,battle_results));pump();
    REQUIRE(toast.second_report && toast.selected_tab==1 && toast.width==820 && toast.persistent);
    REQUIRE(!wcscmp(toast.title,L"本回合报告 · 自势力"));
    REQUIRE(wcsstr(toast.second_report,L"自势力") && !wcsstr(toast.second_report,L"襄阳郡"));
    save_preview(&toast,"turn-report-battle-top.bmp");
    SendMessageW(toast.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);int battle_scroll=toast.scroll;REQUIRE(battle_scroll>0);
    SendMessageW(toast.window,WM_LBUTTONUP,0,MAKELPARAM(50,65));REQUIRE(toast.selected_tab==0 && toast.scroll==0 && toast.deadline);
    SendMessageW(toast.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);int search_scroll=toast.scroll;REQUIRE(search_scroll>0);
    SendMessageW(toast.window,WM_LBUTTONUP,0,MAKELPARAM(200,65));REQUIRE(toast.selected_tab==1 && toast.scroll==battle_scroll && IsWindowVisible(toast.window));
    SendMessageW(toast.window,WM_LBUTTONUP,0,MAKELPARAM(50,65));REQUIRE(toast.scroll==search_scroll);
    save_preview(&toast,"turn-report-search-preview.bmp");
    SendMessageW(toast.window,WM_LBUTTONUP,0,MAKELPARAM(200,65));save_preview(&toast,"turn-report-battle-preview.bmp");
    for(int i=0;i<200;i++) SendMessageW(toast.window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);
    REQUIRE(toast.scroll==toast.content_height-(toast.height-MulDiv(144,toast.scale,96)));save_preview(&toast,"turn-report-battle-bottom.bmp");
    SendMessageW(toast.window,WM_LBUTTONUP,0,MAKELPARAM(790,25));REQUIRE(!toast.deadline && !toast.persistent && !IsWindowVisible(toast.window));
    REQUIRE(!s14_toast_tick(&toast,started+600003,1) && !IsWindowVisible(toast.window));
    // Reuse for a new short toast cannot retain either old report buffer.
    REQUIRE(s14_toast_text(&toast,instance,owner,click,started,L"开始自动搜索",L"正在派遣",2000));REQUIRE(!toast.report_text && !toast.second_report);
    // The user may switch applications while this off-screen test runs. Verify
    // that neither fixture took focus, without requiring the desktop to freeze.
    REQUIRE(GetForegroundWindow()!=toast.window && GetForegroundWindow()!=owner);
    // Use the real clock and message pump as the plugin does, all off-screen.
    started=GetTickCount64(); REQUIRE(s14_toast_show(&toast,instance,owner,click,started));
    while (!s14_toast_tick(&toast,GetTickCount64(),1)) { pump(); MsgWaitForMultipleObjects(0,NULL,FALSE,16,QS_ALLINPUT); }
    ULONGLONG elapsed=GetTickCount64()-started;
    REQUIRE(elapsed>=5000 && elapsed<5500 && !IsWindowVisible(toast.window));
    REQUIRE(GetForegroundWindow()!=toast.window && GetForegroundWindow()!=owner);
    s14_toast_destroy(&toast); DestroyWindow(owner);
    printf("{\"phase_policy\":\"passed\",\"toast_deadline_ms\":5000,\"actual_hide_ms\":%llu,\"click_through\":true,\"focus_preserved\":true,\"repeat_reuses_window\":true,\"report_all_120_lines\":true,\"report_scroll_to_end\":true,\"report_persists_until_click\":true,\"report_click_dismiss\":true,\"report_background_restore\":true,\"report_dpi192\":true,\"game_process_touched\":false}\n",(unsigned long long)elapsed);
    return 0;
}
