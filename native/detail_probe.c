/* Production native-detail reader through query/read-only process rights. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "detail_model.h"
static int read(void *p,uintptr_t at,void *out,size_t n) {SIZE_T got;return ReadProcessMemory((HANDLE)p,(void*)at,out,n,&got) && got==n;}
int wmain(int argc,wchar_t **argv) {
    if(argc!=3) return 2;DWORD pid=wcstoul(argv[1],NULL,10);HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);if(!p) return 3;
    wchar_t name[32768];DWORD n=32768;if(!QueryFullProcessImageNameW(p,0,name,&n)) {CloseHandle(p);return 4;}
    const wchar_t *leaf=wcsrchr(name,L'\\');if(!leaf || _wcsicmp(leaf+1,L"SAN14PK_SC.exe")) {CloseHandle(p);return 4;}
    HMODULE mods[1024];DWORD needed; if(!EnumProcessModules(p,mods,sizeof(mods),&needed)) {CloseHandle(p);return 5;}
    S14DetailFrame frame;int ready=s14_detail_validate(read,p,(uintptr_t)mods[0]),captured=ready && s14_detail_capture(read,p,(uintptr_t)mods[0],&frame);
    if(!ready) memset(&frame,0,sizeof(frame));char utf8[128]={0};if(captured) WideCharToMultiByte(CP_UTF8,0,frame.name,-1,utf8,sizeof(utf8),NULL,NULL);
    char json[1024];snprintf(json,sizeof(json),"{\"read_only\":true,\"ready\":%d,\"visible\":%d,\"officer_id\":%d,\"name\":\"%s\",\"world\":\"0x%llx\",\"dialog\":\"0x%llx\",\"panel\":[%ld,%ld,%ld,%ld],\"calls\":%u,\"bytes\":%u}\n",ready,captured,frame.officer_id,utf8,(unsigned long long)frame.world,(unsigned long long)frame.dialog,frame.panel.left,frame.panel.top,frame.panel.right,frame.panel.bottom,frame.calls,frame.bytes);
    FILE *f=_wfopen(argv[2],L"wb");int ok=f && fputs(json,f)>=0;if(f) fclose(f);CloseHandle(p);fputs(json,stdout);return ok && ready?0:6;
}
