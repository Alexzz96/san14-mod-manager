#ifndef S14_TURN_REPORT_H
#define S14_TURN_REPORT_H
#include <windows.h>
#include "report_model.h"
// Called only on the manager worker. Components are matched by turn boundaries.
void s14_turn_report_reset(void);
unsigned int s14_turn_report_epoch(void);
void s14_turn_report_search(int start,int end,const wchar_t *text);
void s14_turn_report_battle(int start,int end,const wchar_t *text);
int s14_turn_report_take(ULONGLONG now,const wchar_t **search,const wchar_t **battle);
void s14_turn_report_search_data(const S14ReportSearch*);
void s14_turn_report_battle_data(const S14ReportBattle*);
/* Borrowed snapshots on the manager worker; copy before retaining them. */
void s14_turn_report_data(const S14ReportSearch**,const S14ReportBattle**);
#endif
