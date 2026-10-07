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
static void save_preview(S14Toast *toast) {
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=toast->width; info.bmiHeader.biHeight=-toast->height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(NULL); REQUIRE(dc!=NULL);
    void *pixels=NULL;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); REQUIRE(bitmap && pixels);
    HGDIOBJ previous=SelectObject(dc,bitmap); s14_toast_paint(toast,dc); GdiFlush();
    FILE *file=fopen("toast-preview.bmp","wb"); REQUIRE(file!=NULL);
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
    save_preview(&toast);
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
    // Use the real clock and message pump as the plugin does, all off-screen.
    started=GetTickCount64(); REQUIRE(s14_toast_show(&toast,instance,owner,click,started));
    while (!s14_toast_tick(&toast,GetTickCount64(),1)) { pump(); MsgWaitForMultipleObjects(0,NULL,FALSE,16,QS_ALLINPUT); }
    ULONGLONG elapsed=GetTickCount64()-started;
    REQUIRE(elapsed>=5000 && elapsed<5500 && !IsWindowVisible(toast.window));
    REQUIRE(GetForegroundWindow()==foreground);
    s14_toast_destroy(&toast); DestroyWindow(owner);
    printf("{\"phase_policy\":\"passed\",\"toast_deadline_ms\":5000,\"actual_hide_ms\":%llu,\"click_through\":true,\"focus_preserved\":true,\"repeat_reuses_window\":true,\"game_process_touched\":false}\n",(unsigned long long)elapsed);
    return 0;
}
