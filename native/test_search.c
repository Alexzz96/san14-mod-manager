#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#ifdef NDEBUG
#error "Search model verification requires assertions enabled"
#endif
#include <stdio.h>
#include <string.h>
#include "search_model.h"
int main(void) {
    S14SearchGuard g={0};
    assert(!s14_search_claim(&g,1,100,1,1,0)); // canceled confirmation
    assert(!s14_search_claim(&g,1,100,1,0,1)); // disabled
    assert(s14_search_claim(&g,1,100,1,1,1));
    for (int i=0;i<10000;i++) assert(!s14_search_claim(&g,1,100,1,1,1));
    assert(s14_search_claim(&g,1,100,2,1,1)); // another human faction
    assert(s14_search_claim(&g,1,110,1,1,1));
    assert(s14_search_claim(&g,1,90,1,1,1)); // load earlier save
    assert(s14_search_claim(&g,2,90,1,1,1)); // another game world
    assert(!s14_search_claim(&g,2,90,52,1,1));
    assert(s14_search_budget(14,1,100)==14);
    assert(s14_search_budget(14,3,100)==4);
    assert(s14_search_budget(14,1,2)==2);
    assert(!s14_search_budget(0,1,10)); assert(!s14_search_budget(14,0,10)); assert(!s14_search_budget(201,1,10));
    S14SearchReport r={0}; S14SearchEvent begin={.kind=S14_SEARCH_BEGIN,.day=100,.force=1,.dispatched=3}; s14_search_reduce(&r,&begin);
    S14SearchEvent result={.kind=S14_SEARCH_RESULT,.day=101,.force=2,.type=S14_SEARCH_PERSON};
    s14_search_reduce(&r,&result); assert(!r.completed);
    result.force=1; wcscpy(result.actor,L"曹操"); wcscpy(result.location,L"襄阳郡"); wcscpy(result.detail,L"在^05襄阳郡^00发现武将，并未成功登用。"); result.outcome=2;
    s14_search_reduce(&r,&result);
    result.type=S14_SEARCH_ITEM; wcscpy(result.detail,L"发现名品：青釭剑"); s14_search_reduce(&r,&result);
    result.type=S14_SEARCH_BOOK; wcscpy(result.detail,L"发现战法书"); s14_search_reduce(&r,&result);
    result.type=S14_SEARCH_MONEY; result.amount=300; wcscpy(result.detail,L"发现 300 金钱"); s14_search_reduce(&r,&result);
    result.type=S14_SEARCH_NOTHING; s14_search_reduce(&r,&result);
    assert(r.completed==5 && r.people==1 && r.items==1 && r.books==1 && r.money==300 && r.empty==1);
    // Returning travelers and turn-income events never enter the result reducer.
    s14_search_reduce(&r,&(S14SearchEvent){.kind=S14_SEARCH_END,.pending=2}); assert(r.completed==5);
    wchar_t summary[256],details[4096]; s14_search_format(&r,summary,details); assert(wcsstr(details,L"青釭剑"));
    assert(r.lines==5 && wcsstr(details,L"曹操 → 襄阳郡") && wcsstr(details,L"探索失败") && !wcsstr(details,L"^05"));
    wchar_t temp[MAX_PATH],root[MAX_PATH],folder[MAX_PATH],path[MAX_PATH]; GetTempPathW(MAX_PATH,temp);
    swprintf(root,MAX_PATH,L"%lsS14-search-test-%lu",temp,(unsigned long)GetCurrentProcessId()); assert(CreateDirectoryW(root,NULL));
    swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager",root); assert(CreateDirectoryW(folder,NULL));
    assert(s14_search_report_save(root,&r)); wchar_t saved[256],saved_details[4096]; s14_search_report_read(root,saved,saved_details);
    assert(!wcscmp(saved,summary) && !wcscmp(saved_details,details));
    s14_search_reduce(&r,&begin); assert(s14_search_report_save(root,&r));
    swprintf(path,MAX_PATH,L"%ls\\search-results.ini",folder);
    wchar_t stale[16]; GetPrivateProfileStringW(L"SearchReport",L"Item04",L"",stale,16,path); assert(!stale[0]);
    assert(DeleteFileW(path)); assert(RemoveDirectoryW(folder)); assert(RemoveDirectoryW(root));
    s14_search_reduce(&r,&(S14SearchEvent){.kind=S14_SEARCH_BEGIN,.day=110,.force=1,.dispatched=0});
    result.type=S14_SEARCH_MONEY; result.amount=400; result.day=111; s14_search_reduce(&r,&result);
    assert(r.money==400 && !r.people); // long task produces its result in this turn
    for (int i=0;i<100;i++) s14_search_reduce(&r,&result);
    assert(r.lines==101 && !r.truncated && r.money==40400); // full results beyond the old 64-line limit
    wchar_t *popup=s14_search_popup_text(&r); assert(popup && wcsstr(popup,L"101. 曹操 → 襄阳郡")); HeapFree(GetProcessHeap(),0,popup);
    s14_search_reduce(&r,&(S14SearchEvent){.kind=S14_SEARCH_RESET}); assert(!r.active && !r.completed);
    puts("{\"confirmation_guard\":true,\"duplicate_frames\":10000,\"save_rewind\":true,\"order_budget\":true,\"four_result_types\":true,\"long_task_results\":true,\"unicode_report_roundtrip\":true,\"complete_actor_location_results\":true,\"failure_lines\":true,\"color_commands_removed\":true,\"more_than_64_details\":true}");
    return 0;
}
