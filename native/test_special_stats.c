#include "special_stats.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef NDEBUG
#error Assertions required
#endif
static unsigned char first[32]={1},second[32]={2},bad[32]={3};
static wchar_t root[MAX_PATH];
static S14SpecialSnapshot *snapshot;
static void capture(unsigned long long id,int target,int role,int verified){
    S14SpecialEvent e={.id=id,.kind=S14_SPECIAL_CAPTURE,.day=100,.actor=10,.target=target,
        .actor_force=1,.target_force=2,.outcome=1,.actor_role=role,.verified=verified};
    wcscpy(e.actor_name,L"曹仁");wcscpy(e.target_name,L"张嶷");s14_special_consume(7,&e);
}
static void check(unsigned int n,uint64_t times,uint64_t people){
    assert(s14_special_snapshot(7,snapshot));assert(snapshot->count==n);
    assert(snapshot->rows[10].captures==times && snapshot->rows[10].unique_captives==people);
}
int main(int argc,char **argv){
    assert(argc>=2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));
    snapshot=malloc(sizeof(*snapshot));assert(snapshot);s14_special_root(root);
    if(argc==3){
        s14_special_load(7,100,1,first,1,0);check(5,3,2);assert(snapshot->bound);
        for(unsigned int i=0;i<snapshot->count;i++)assert(!snapshot->events[i].id);
        capture(1,20,S14_SPECIAL_ACTOR_PERSON,1);check(6,4,2);
        printf("{\"status\":\"passed\",\"cross_process_restore\":true}\n");free(snapshot);return 0;
    }
    s14_special_begin(7,100);check(0,0,0);
    capture(1,20,S14_SPECIAL_ACTOR_COMMANDER,0);check(1,0,0);assert(!snapshot->rows[20].valid_mask);
    capture(2,20,S14_SPECIAL_ACTOR_COMMANDER,1);check(2,0,0); // Commander cannot be credited as actual captor.
    capture(3,20,S14_SPECIAL_ACTOR_PERSON,1);capture(3,20,S14_SPECIAL_ACTOR_PERSON,1);check(3,1,1);
    capture(4,20,S14_SPECIAL_ACTOR_PERSON,1);capture(5,30,S14_SPECIAL_ACTOR_PERSON,1);check(5,3,2);
    assert(snapshot->rows[20].captured==2 && snapshot->rows[30].captured==1);
    assert(s14_special_save(7,first,1));
    S14SpecialEvent duel={.id=6,.kind=S14_SPECIAL_DUEL,.day=100,.actor=10,.target=20,
        .actor_force=1,.target_force=2,.outcome=0,.actor_role=S14_SPECIAL_ACTOR_PERSON,.verified=0};
    s14_special_consume(7,&duel);check(6,3,2);assert(!snapshot->rows[10].duels);
    duel.id=7;duel.verified=1;s14_special_consume(7,&duel);assert(s14_special_snapshot(7,snapshot));assert(snapshot->rows[10].wins==1 && snapshot->rows[20].losses==1);
    duel.id=8;duel.outcome=1;s14_special_consume(7,&duel);
    duel.id=9;duel.outcome=2;s14_special_consume(7,&duel);assert(s14_special_snapshot(7,snapshot));assert(snapshot->rows[10].duels==2);
    duel.id=10;duel.target_force=1;s14_special_consume(7,&duel);assert(s14_special_snapshot(7,snapshot));assert(snapshot->rows[10].duels==2);
    s14_special_gap();assert(s14_special_save(7,second,1));
    s14_special_load(7,100,0,first,1,0);check(10,3,2);assert(snapshot->incomplete); // Failed load keeps current state.
    s14_special_load(7,100,1,first,1,0);check(5,3,2);assert(!snapshot->incomplete && !snapshot->rows[10].duels);
    s14_special_load(7,100,1,second,1,0);check(10,3,2);assert(snapshot->incomplete && snapshot->rows[10].wins==1);
    wchar_t path[MAX_PATH],hex[65];for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",bad[i]);
    swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\career\\checkpoints\\%ls.s14special",root,hex);
    HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD wrote;assert(WriteFile(f,"unknown",7,&wrote,NULL));CloseHandle(f);
    assert(!s14_special_checkpoint_owned(path) && !s14_special_save(7,bad,1));
    f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(f!=INVALID_HANDLE_VALUE);char b[8]={0};assert(ReadFile(f,b,7,&wrote,NULL) && !strcmp(b,"unknown"));CloseHandle(f);
    s14_special_begin(7,99);check(0,0,0); // Date rollback without load also separates the history.
    for(unsigned int i=1;i<=S14_SPECIAL_HISTORY+1;i++)capture(i,20,S14_SPECIAL_ACTOR_PERSON,0);
    assert(s14_special_snapshot(7,snapshot));assert(snapshot->count==S14_SPECIAL_HISTORY && snapshot->truncated && snapshot->incomplete && !snapshot->rows[10].captures);
    // Leave the first checkpoint intact for the independent fresh-process restore.
    printf("{\"status\":\"passed\",\"candidate_never_credited\":true,\"commander_not_captor\":true,\"repeat_capture_vs_unique\":true,\"duel_outcomes\":true,\"duplicates\":true,\"save_rollback\":true,\"unknown_file_preserved\":true,\"bounded_history\":true}\n");free(snapshot);return 0;
}
