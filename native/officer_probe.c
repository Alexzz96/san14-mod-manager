/* Diagnostic executable: same production reader against an external game
   using PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, never game injection. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "officer_ui.h"
static int memory(void *context,uintptr_t address,void *out,size_t size) { SIZE_T got=0;int ok=ReadProcessMemory((HANDLE)context,(const void*)address,out,size,&got) && got==size;if(!ok)fprintf(stderr,"read_failed address=%llx size=%llu error=%lu\n",(unsigned long long)address,(unsigned long long)size,(unsigned long)GetLastError());return ok; }
static void json_text(FILE *f,const wchar_t *s) {
    char utf8[2048];int n=WideCharToMultiByte(CP_UTF8,0,s,-1,utf8,sizeof(utf8),NULL,NULL);fputc('"',f);
    for (int i=0;i<n-1;i++) { unsigned char c=(unsigned char)utf8[i];if (c=='"' || c=='\\') fputc('\\',f);if (c>=32) fputc(c,f); }
    fputc('"',f);
}
static int export(FILE *f,S14OfficerSnapshot *s) {
    fprintf(f,"{\"schema_version\":2,\"read_only\":true,\"count\":%d,\"player_force\":%d,\"read_errors\":%d,\"unstable\":%d,\"bytes_read\":%llu,\"rows\":[",s->count,s->player_force,s->read_errors,s->unstable,(unsigned long long)s->bytes_read);
    for (int i=0;i<s->count;i++) {
        S14Officer *p=&s->rows[i];if (i) fputc(',',f);fprintf(f,"{\"id\":%d,\"force\":%d,\"status\":%d,\"name\":",p->id,p->force,p->status);json_text(f,p->name);
        fputs(",\"force_name\":",f);json_text(f,p->force_name);fputs(",\"status_name\":",f);json_text(f,p->status_name);fputs(",\"location\":",f);json_text(f,p->location);
        fputs(",\"home\":",f);json_text(f,p->home_name);fputs(",\"personalities\":",f);json_text(f,p->personality_text);fputs(",\"tactics\":",f);json_text(f,p->tactics_text);
        fprintf(f,",\"ability\":[%d,%d,%d,%d,%d],\"ambition\":%d,\"bond\":%d,\"loyalty\":%d,\"ambition_raw\":%d,\"bond_raw\":%d,\"loyalty_raw\":%d,\"troops\":%d,\"army\":%d,\"tile\":%d,\"career_valid_mask\":%u}",p->ability[0],p->ability[1],p->ability[2],p->ability[3],p->ability[4],p->ambition,p->bond,p->loyalty,p->ambition_raw,p->bond_raw,p->loyalty_raw,p->troops,p->army,p->tile,p->career.valid_mask);
    }
    fputs("]}\n",f);return !ferror(f);
}
static int preview(S14OfficerSnapshot *s,const wchar_t *path) {
    S14OfficerUI ui={0};if (!s14_officer_ui_create(&ui,GetModuleHandleW(NULL),NULL,1)) return 0;
    memcpy(ui.snapshot,s,sizeof(*s));ui.last_capture_ok=1;wcscpy(ui.notice,L"实时武将数据 · 五维为基础能力 · 战绩尚未采集");
    SendMessageW(ui.window,WM_COMMAND,MAKEWPARAM(501,EN_CHANGE),(LPARAM)ui.query);
    for (int i=0;i<ui.visible_count;i++) if (ui.snapshot->rows[ui.indices[i]].id==613) { ListView_SetItemState(ui.list,i,LVIS_SELECTED,LVIS_SELECTED);break; }
    RECT r;GetClientRect(ui.window,&r);int width=r.right,height=r.bottom;
    SetWindowPos(ui.window,HWND_BOTTOM,-20000,-20000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=NULL;HBITMAP image=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,NULL,0);HGDIOBJ old=SelectObject(dc,image);
    PrintWindow(ui.window,dc,PW_CLIENTONLY);GdiFlush();
    BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+width*height*4;
    FILE *f=_wfopen(path,L"wb");int ok=f!=NULL;
    if (f) { ok=fwrite(&header,sizeof(header),1,f)==1 && fwrite(&bi.bmiHeader,sizeof(BITMAPINFOHEADER),1,f)==1 && fwrite(bits,width*height*4,1,f)==1;fclose(f); }
    SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);ReleaseDC(NULL,screen);s14_officer_ui_destroy(&ui);return ok;
}
int wmain(int argc,wchar_t **argv) {
    if (argc<3) return 2;DWORD pid=(DWORD)wcstoul(argv[1],NULL,10);
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);if (!process) return 3;
    wchar_t path[32768];DWORD length=32768;int ok=QueryFullProcessImageNameW(process,0,path,&length);wchar_t *name=wcsrchr(path,L'\\');
    if (!ok || !name || _wcsicmp(name+1,L"SAN14PK_SC.exe")) { CloseHandle(process);return 4; }
    HMODULE modules[1024];DWORD needed=0;if (!EnumProcessModules(process,modules,sizeof(modules),&needed)) { CloseHandle(process);return 5; }
    S14OfficerSnapshot *s=calloc(1,sizeof(*s));LARGE_INTEGER start,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);ok=s14_officers_capture(memory,process,(uintptr_t)modules[0],s);QueryPerformanceCounter(&end);
    printf("capture=%d count=%d errors=%d unstable=%d elapsed_ms=%.3f bytes=%llu\n",ok,s->count,s->read_errors,s->unstable,1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart,(unsigned long long)s->bytes_read);
    if (ok) { FILE *f=_wfopen(argv[2],L"wb");ok=f && export(f,s);if (f) fclose(f); }
    if (ok && argc>3) ok=preview(s,argv[3]);
    free(s);CloseHandle(process);return ok?0:6;
}
