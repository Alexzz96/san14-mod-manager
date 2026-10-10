#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include "affix_names.c"
#include "battle_stats.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char *image,*world,*person,*pool;
static volatile LONG person_calls,army_calls;
static uintptr_t person_fixture(void *p,wchar_t *out){
    assert(GetLastError()==1234);InterlockedIncrement(&person_calls);size_t n=0;
    for(int part=0;part<2;part++)for(int i=0;i<9;i++){wchar_t c;memcpy(&c,(char*)p+0x12+part*18+i*2,2);if(!c)break;out[n++]=c;}out[n]=0;
    SetLastError(777);return 0x314;
}
static const wchar_t *army_fixture(void *p){assert(p==pool+29*512 && GetLastError()==1234);InterlockedIncrement(&army_calls);SetLastError(777);return L"曹仁队";}
static void putptr(void *p,size_t o,uintptr_t v){memcpy((char*)p+o,&v,8);}
static void putshort(void *p,size_t o,unsigned short v){memcpy((char*)p+o,&v,2);}
static void entry_test(const char *name_bytes,const char *army_bytes){
    unsigned char *p=VirtualAlloc(NULL,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(p);
    for(int k=0;k<2;k++){const char *s=k?army_bytes:name_bytes;assert(strlen(s)==128);for(int i=0;i<64;i++){unsigned int v;assert(sscanf(s+i*2,"%2x",&v)==1);p[(k?0x20bec0:0x20c270)+i]=(unsigned char)v;}}
    assert(MH_Initialize()==MH_OK);p[0x20c270]^=1;assert(!s14_affix_names_install((uintptr_t)p,(uintptr_t)p+0x300000));p[0x20c270]^=1;
    assert(MH_CreateHook(p+0x20bec0,army_name_hook,(void**)&original_army_name)==MH_OK);
    assert(!s14_affix_names_install((uintptr_t)p,(uintptr_t)p+0x300000));assert(!s14_affix_names_ready());
    assert(MH_RemoveHook(p+0x20bec0)==MH_OK);assert(s14_affix_names_install((uintptr_t)p,(uintptr_t)p+0x300000));
    assert(MH_EnableHook(MH_ALL_HOOKS)==MH_OK && MH_DisableHook(MH_ALL_HOOKS)==MH_OK && MH_Uninitialize()==MH_OK);
    InterlockedExchange(&names_ready,0);VirtualFree(p,0,MEM_RELEASE);
}
static int refresh(unsigned epoch,int resume){S14BattleTotals row;int gap,bound;assert(s14_stats_row((uintptr_t)world,518,&row,&gap,&bound));return s14_affix_publish((uintptr_t)world,row.enemy_loss,1,bound,gap,100,epoch,resume);}
static S14RoundObject unit(int id,int leader,int force,int troops){return (S14RoundObject){.kind=2,.id=id,.leader=leader,.force=force,.troops=troops,.active=1};}
static void names(int eligible){
    unsigned char original[512];memcpy(original,person,512);struct {wchar_t out[19];uint64_t canary;} b={.canary=0xabcdef0011223344ull};
    int before=person_calls;SetLastError(1234);assert(person_name_hook(person,b.out)==0x314 && GetLastError()==777 && person_calls==before+1);
    assert(!wcscmp(b.out,eligible?L"神 曹仁":L"曹仁") && b.canary==0xabcdef0011223344ull && !memcmp(original,person,512));
    before=army_calls;SetLastError(1234);const wchar_t *army=army_name_hook(pool+29*512);
    assert(!wcscmp(army,eligible?L"神 曹仁":L"曹仁队") && GetLastError()==777 && army_calls==before+1);
}
typedef struct {uintptr_t pointer;HANDLE ready,release;} ThreadName;
static DWORD WINAPI name_thread(void *arg){ThreadName *t=arg;SetLastError(1234);const wchar_t *s=army_name_hook(pool+29*512);assert(!wcscmp(s,L"神 曹仁"));t->pointer=(uintptr_t)s;SetEvent(t->ready);assert(WaitForSingleObject(t->release,5000)==WAIT_OBJECT_0);assert(!wcscmp(s,L"神 曹仁"));return 0;}
int main(int argc,char **argv){
    assert(argc==2 || argc==3 || argc==4);
    if(argc==4)entry_test(argv[2],argv[3]);
    image=VirtualAlloc(NULL,0x2020000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);world=calloc(1,0x85200);person=calloc(1,512);pool=calloc(30,512);assert(image && world && person && pool);
    names_base=(uintptr_t)image;putptr(image,0x1fc91d0,(uintptr_t)world);putptr(world,0x148+518*8,(uintptr_t)person);putptr(person,0,names_base+0x12a00d0);putshort(person,0x10,518);memcpy(person+0x12,L"曹",4);memcpy(person+0x24,L"仁",4);
    putptr(world,0x7df60,(uintptr_t)pool);putptr(world,0x7df60+29*8,(uintptr_t)(pool+29*512));putptr(pool+29*512,0,names_base+0x123e288);pool[29*512+0x10]=1;putshort(pool+29*512,0x12,518);
    original_person_name=person_fixture;original_army_name=army_fixture;s14_affix_configure(1);
    wchar_t root[MAX_PATH],dir[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);s14_stats_root(root);
    unsigned char old[32]={0x31},later[32]={0x32},fresh[32]={0x33};
    unsigned epoch=s14_affix_suspend();s14_stats_load_begin();s14_stats_load_end((uintptr_t)world,100,1,argc==3?later:old,1,0);assert(refresh(epoch,1));
    if(argc==3){assert(s14_affix_active((uintptr_t)world,518));names(1);puts("{\"cross_process_restore\":true,\"name\":\"神 曹仁\"}");return 0;}
    assert(!s14_affix_active((uintptr_t)world,518));names(0);
    S14RoundEvent e={.id=1,.world=(uintptr_t)world,.day=100,.kind=S14_ROUND_DAMAGE,.stable=1,.source=unit(29,518,1,5500),.target=unit(30,613,2,10000),.target_after=unit(30,613,2,5001)};
    s14_stats_consume(&e);assert(refresh(epoch,0));assert(!s14_affix_active((uintptr_t)world,518));assert(s14_stats_save((uintptr_t)world,old,1));names(0);
    e.id=2;e.target.troops=5001;e.target_after.troops=5000;s14_stats_consume(&e);s14_stats_consume(&e);assert(!s14_affix_active((uintptr_t)world,518));
    assert(refresh(epoch,0) && s14_affix_active((uintptr_t)world,518));assert(!s14_affix_active((uintptr_t)world,511) && !s14_affix_active((uintptr_t)world+1,518));names(1);
    wchar_t display[24];assert(s14_affix_display((uintptr_t)world,518,L"神 曹仁",display,24) && !wcscmp(display,L"神 曹仁"));assert(!s14_affix_display((uintptr_t)world,518,L"曹仁",display,4) && !display[0]);
    assert(s14_stats_save((uintptr_t)world,later,1));
    s14_affix_configure(0);names(0);s14_affix_configure(1);names(1);
    unsigned char copy[512];memcpy(copy,person,512);wchar_t out[24];SetLastError(1234);assert(person_name_hook(copy,out)==0x314 && !wcscmp(out,L"曹仁"));
    ThreadName threads[2]={0};HANDLE workers[2];for(int i=0;i<2;i++){threads[i].ready=CreateEventW(NULL,TRUE,FALSE,NULL);threads[i].release=CreateEventW(NULL,TRUE,FALSE,NULL);workers[i]=CreateThread(NULL,0,name_thread,&threads[i],0,NULL);assert(workers[i] && WaitForSingleObject(threads[i].ready,5000)==WAIT_OBJECT_0);}assert(threads[0].pointer!=threads[1].pointer);
    for(int i=0;i<2;i++){SetEvent(threads[i].release);assert(WaitForSingleObject(workers[i],5000)==WAIT_OBJECT_0);CloseHandle(workers[i]);CloseHandle(threads[i].ready);CloseHandle(threads[i].release);}
    // Real x64 hook ABI against private functions, never native game execution.
    assert(MH_Initialize()==MH_OK && MH_CreateHook((void*)person_fixture,person_name_hook,(void**)&original_person_name)==MH_OK && MH_CreateHook((void*)army_fixture,army_name_hook,(void**)&original_army_name)==MH_OK && MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    PersonNameCall volatile call_person=person_fixture;ArmyNameCall volatile call_army=army_fixture;SetLastError(1234);assert(call_person(person,out)==0x314 && !wcscmp(out,L"神 曹仁") && GetLastError()==777);SetLastError(1234);assert(!wcscmp(call_army(pool+29*512),L"神 曹仁") && GetLastError()==777);
    assert(MH_DisableHook(MH_ALL_HOOKS)==MH_OK && MH_Uninitialize()==MH_OK);
    original_person_name=person_fixture;original_army_name=army_fixture;
    epoch=s14_affix_suspend();names(0);assert(!s14_affix_publish((uintptr_t)world,5000,1,1,0,100,epoch-1,1));assert(!s14_affix_publish((uintptr_t)world,5000,1,1,0,100,epoch,0));
    s14_stats_load_begin();s14_stats_load_end((uintptr_t)world,100,1,old,1,0);assert(refresh(epoch,1) && !s14_affix_active((uintptr_t)world,518));names(0);
    epoch=s14_affix_suspend();s14_stats_load_begin();s14_stats_load_end((uintptr_t)world,100,1,later,1,0);assert(refresh(epoch,1));names(1);
    epoch=s14_affix_suspend();s14_stats_load_begin();s14_stats_load_end((uintptr_t)world,100,0,old,1,0);assert(refresh(epoch,1));names(1);
    epoch=s14_affix_suspend();s14_stats_load_begin();s14_stats_load_end((uintptr_t)world,100,1,fresh,1,1);assert(refresh(epoch,1));names(0);
    free(person);free(pool);free(world);VirtualFree(image,0,MEM_RELEASE);
    puts("{\"status\":\"passed\",\"threshold_4999_5000\":true,\"commander_518_only\":true,\"deferred_round_unlock\":true,\"old_save_rollback\":true,\"failed_load_restore\":true,\"new_campaign_isolation\":true,\"stale_epoch_rejected\":true,\"canonical_identity\":true,\"native_fields_unchanged\":true,\"name_buffer_canary\":true,\"no_duplicate_prefix\":true,\"disabled_restores\":true,\"native_hook_abi\":true,\"original_calls_once\":true,\"last_error_preserved\":true,\"thread_local_army_name\":true}");return 0;
}
