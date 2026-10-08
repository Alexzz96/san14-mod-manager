#ifndef S14_TURN_REPORT_H
#define S14_TURN_REPORT_H
#include <windows.h>
// Called only on the manager worker. Components are matched by turn boundaries.
void s14_turn_report_reset(void);
unsigned int s14_turn_report_epoch(void);
void s14_turn_report_search(int start,int end,const wchar_t *text);
void s14_turn_report_battle(int start,int end,const wchar_t *text);
int s14_turn_report_take(ULONGLONG now,const wchar_t **search,const wchar_t **battle);
#endif
