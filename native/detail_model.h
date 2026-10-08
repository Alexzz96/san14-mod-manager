#ifndef S14_DETAIL_MODEL_H
#define S14_DETAIL_MODEL_H
#include "officer_model.h"
typedef struct {
    uintptr_t world,dialog,person;
    int officer_id;RECT panel;
    wchar_t name[24];
    unsigned int calls,bytes;
} S14DetailFrame;
// Native active UI registry, bounded to 2,048 objects / 256 KiB. No game calls.
int s14_detail_validate(S14OfficerRead,void*,uintptr_t);
int s14_detail_capture(S14OfficerRead,void*,uintptr_t,S14DetailFrame*);
int s14_detail_rect(const RECT *panel,int client_width,int client_height,RECT *bar,int *scale);
#endif
