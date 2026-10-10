/* Bounded production reader, query/read-only process rights. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include "map_effects_model.h"
static int memory(void *p,uintptr_t at,void *out,size_t n){SIZE_T got;return ReadProcessMemory(p,(void*)at,out,n,&got) && got==n;}
int wmain(int argc,wchar_t **argv){
    if(argc!=3)return 2;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,wcstoul(argv[1],NULL,10));if(!p)return 3;
    wchar_t path[32768];DWORD size=32768;HMODULE modules[1024];DWORD needed;
    if(!QueryFullProcessImageNameW(p,0,path,&size) || !wcsrchr(path,L'\\') || _wcsicmp(wcsrchr(path,L'\\')+1,L"SAN14PK_SC.exe") || !EnumProcessModules(p,modules,sizeof(modules),&needed)){CloseHandle(p);return 4;}
    uintptr_t base=(uintptr_t)modules[0];int ready=s14_map_validate(memory,p,base);S14MapCache cache={0};S14MapFrame f={0};char json[2048];
    FILE *file=_wfopen(argv[2],L"wb");if(!file){CloseHandle(p);return 5;}fputs("[\n",file);
    for(int i=0;i<3;i++){
        int captured=ready && s14_map_capture(memory,p,base,&cache,GetTickCount64(),&f);
        snprintf(json,sizeof(json),"{\"read_only\":true,\"ready\":%d,\"captured\":%d,\"discovered\":%d,\"markers\":%d,\"army_id\":%d,\"halo\":%d,\"tooltip\":%d,\"modal\":%d,\"main_map\":%d,\"state_depth\":%llu,\"state_vtable_rva\":\"0x%llx\",\"portrait\":[%ld,%ld,%ld,%ld],\"card\":[%ld,%ld,%ld,%ld],\"calls\":%u,\"bytes\":%u}%s\n",ready,captured,f.discovered,cache.markers,f.army_id,f.halo,f.tooltip,f.modal,f.scene.main_map,(unsigned long long)f.scene.depth,(unsigned long long)(f.scene.vtable?f.scene.vtable-base:0),f.portrait.left,f.portrait.top,f.portrait.right,f.portrait.bottom,f.card.left,f.card.top,f.card.right,f.card.bottom,f.calls,f.bytes,i<2?",":"");fputs(json,file);fputs(json,stdout);
    }
    fputs("]\n",file);fclose(file);CloseHandle(p);return 0;
}
