#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rule.h"
#include "MinHook.h"

typedef uint64_t (*Target)(uint64_t,uint64_t,uint64_t,uint64_t);
static Target original;
static Target target;
static volatile LONG running=1, failures=0;
static volatile LONG64 invocations=0;
static uint64_t detour(uint64_t a,uint64_t b,uint64_t c,uint64_t d) { return original(a,b,c,d)+1; }
static DWORD WINAPI stress(LPVOID unused) {
    (void)unused;
    while (InterlockedCompareExchange(&running,1,1)) {
        uint64_t result=target(11,22,33,44);
        if (result!=110 && result!=111) InterlockedIncrement(&failures);
        InterlockedIncrement64(&invocations);
    }
    return 0;
}
#define REQUIRE(expr) do { if (!(expr)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#expr); exit(1); } } while (0)

int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--hold")) { puts("ready"); fflush(stdout); Sleep(30000); return 0; }
    // Same complete 15-byte entry sequence as both inspected game functions.
    const unsigned char code[]={0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
        0x48,0x89,0x7c,0x24,0x20,0x48,0x8d,0x04,0x11,0x4c,0x01,0xc0,0x4c,0x01,0xc8,0xc3};
    unsigned char *memory=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    REQUIRE(memory!=NULL); memcpy(memory,code,sizeof(code));
    DWORD protection; REQUIRE(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&protection));
    FlushInstructionCache(GetCurrentProcess(),memory,sizeof(code));
    target=(Target)memory; REQUIRE(target(11,22,33,44)==110);
    REQUIRE(MH_Initialize()==MH_OK);
    REQUIRE(MH_CreateHook(memory,detour,(LPVOID*)&original)==MH_OK);
    HANDLE threads[8];
    for(int i=0;i<8;i++) { threads[i]=CreateThread(NULL,0,stress,NULL,0,NULL); REQUIRE(threads[i]!=NULL); }
    REQUIRE(MH_EnableHook(memory)==MH_OK);
    REQUIRE(target(11,22,33,44)==111);
    Sleep(30);
    REQUIRE(MH_DisableHook(memory)==MH_OK);
    REQUIRE(target(11,22,33,44)==110);
    InterlockedExchange(&running,0);
    REQUIRE(WaitForMultipleObjects(8,threads,TRUE,10000)==WAIT_OBJECT_0);
    for(int i=0;i<8;i++) CloseHandle(threads[i]);
    REQUIRE(failures==0 && invocations>0);
    REQUIRE(memcmp(memory,code,sizeof(code))==0);
    REQUIRE(MH_RemoveHook(memory)==MH_OK); REQUIRE(MH_Uninitialize()==MH_OK);
    VirtualFree(memory,0,MEM_RELEASE);

    S14Cell *cells=calloc(48400,sizeof(S14Cell)); REQUIRE(cells!=NULL);
    int tile=100*220+100;
    for(int y=0;y<100;y++) cells[y*220+100]=(S14Cell){9,0};
    S14Decision decision;
    S14EvaluateBoard(cells,220,220,tile,10,0,1,0,&decision);
    REQUIRE(!decision.allowed && decision.count==6 && decision.reads<=30);
    LARGE_INTEGER frequency,started,finished;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&started);
    volatile int checksum=0;
    for(int i=0;i<1000000;i++) { S14EvaluateBoard(cells,220,220,tile,10,0,1,0,&decision); checksum+=decision.count; }
    QueryPerformanceCounter(&finished);
    REQUIRE(checksum==6000000);
    double ns=(double)(finished.QuadPart-started.QuadPart)*1e9/frequency.QuadPart/1000000;
    printf("{\"hook_stress_calls\":%lld,\"hook_stress_failures\":%ld,\"rule_iterations\":1000000,\"rule_ns_per_call\":%.1f,\"rule_neighbor_read_bound\":30}\n",
           (long long)invocations,(long)failures,ns);
    free(cells); return 0;
}
