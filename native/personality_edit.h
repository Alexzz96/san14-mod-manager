#ifndef S14_PERSONALITY_EDIT_H
#define S14_PERSONALITY_EDIT_H
#include <windows.h>
#include <stdint.h>
#define S14_PERSONALITY_SLOTS 9
typedef struct {uintptr_t world;int officer,slot,add;unsigned short before[9];unsigned epoch;} S14PersonalityEdit;
enum {S14_PE_IDLE=0,S14_PE_PENDING,S14_PE_APPLIED,S14_PE_STALE,S14_PE_FULL,S14_PE_DUPLICATE,S14_PE_UNAVAILABLE,S14_PE_FAILED};
int s14_personality_plan(const unsigned short slots[9],int add,int slot,int *target);
int s14_personality_init(uintptr_t base,uintptr_t end);
void s14_personality_enabled(int on);
int s14_personality_capture(uintptr_t world,int officer,unsigned short slots[9]);
int s14_personality_submit(uintptr_t world,int officer,const unsigned short before[9],int add,int slot);
int s14_personality_result(int *slot);
void s14_personality_service(void *state);
int s14_personality_next_log(char *text,size_t size);
#endif
