#ifndef S14_DOMESTIC_NATIVE_H
#define S14_DOMESTIC_NATIVE_H
#include <windows.h>
#include "domestic.h"
/* Manual native execution is available for local acceptance. This is separate
   from in-game acceptance status; entry, session and budget checks still gate it. */
#ifndef S14_DOMESTIC_EXECUTION_ENABLED
#define S14_DOMESTIC_EXECUTION_ENABLED 1
#endif
enum {S14_D_IDLE,S14_D_PENDING,S14_D_PREVIEW,S14_D_APPLIED,S14_D_STALE,S14_D_FAILED,S14_D_CANCELLED,S14_D_UNAVAILABLE};
typedef struct {uint64_t id,preview;int status,completed,remaining;wchar_t message[256];S14DSnapshot snapshot;S14DPlan plan;} S14DResult;
int s14_domestic_init(uintptr_t base,uintptr_t end);
void s14_domestic_enabled(int on);
int s14_domestic_can_apply(wchar_t *reason,size_t size);
uint64_t s14_domestic_request(const S14DOptions*);
uint64_t s14_domestic_submit(uint64_t preview,const S14DOptions*,const S14DPlan*);
int s14_domestic_result(uint64_t id,S14DResult*);
void s14_domestic_cancel(void);
void s14_domestic_service(void *state);
int s14_domestic_next_log(char *text,size_t capacity);
void s14_domestic_auto_configure(int on);
void s14_domestic_preferences(const S14DOptions*);
int s14_domestic_auto_turn(void *state,uintptr_t world,int day,int force);
int s14_domestic_search_allowed(int person);
unsigned s14_domestic_revision(void);
void s14_domestic_auto_status(wchar_t *text,size_t capacity);
#endif
