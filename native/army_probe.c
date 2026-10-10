/* Uses query/read-only rights and the production selected-unit reader. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include "army_model.h"
static int memory(void *p,uintptr_t at,void *out,size_t n){SIZE_T got;return ReadProcessMemory(p,(void*)at,out,n,&got) && got==n;}
int wmain(int argc,wchar_t **argv){
    if(argc!=3)return 2;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,wcstoul(argv[1],NULL,10));if(!p)return 3;
    wchar_t path[32768];DWORD size=32768;HMODULE modules[1024];DWORD needed;
    if(!QueryFullProcessImageNameW(p,0,path,&size) || !wcsrchr(path,L'\\') || _wcsicmp(wcsrchr(path,L'\\')+1,L"SAN14PK_SC.exe") || !EnumProcessModules(p,modules,sizeof(modules),&needed)){CloseHandle(p);return 4;}
    S14ArmyFrame f;int visible=s14_army_capture(memory,p,(uintptr_t)modules[0],&f);char name[128]={0},formation[128]={0};
    WideCharToMultiByte(CP_UTF8,0,f.name,-1,name,128,NULL,NULL);WideCharToMultiByte(CP_UTF8,0,f.formation,-1,formation,128,NULL,NULL);
    char json[1024];snprintf(json,sizeof(json),"{\"read_only\":true,\"visible\":%d,\"officer_id\":%d,\"name\":\"%s\",\"army_id\":%d,\"formation\":\"%s\",\"troops\":%d,\"wounded\":%d,\"morale\":%d,\"page\":\"0x%llx\",\"army\":\"0x%llx\",\"panel\":[%ld,%ld,%ld,%ld],\"calls\":%u,\"bytes\":%u}\n",visible,f.officer_id,name,f.army_id,formation,f.troops,f.wounded,f.morale,(unsigned long long)f.page,(unsigned long long)f.army,f.panel.left,f.panel.top,f.panel.right,f.panel.bottom,f.calls,f.bytes);
    FILE *file=_wfopen(argv[2],L"wb");int ok=file && fputs(json,file)>=0;if(file)fclose(file);fputs(json,stdout);CloseHandle(p);return ok?0:5;
}
