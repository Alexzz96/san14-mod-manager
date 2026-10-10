#ifndef S14_AI_AFFIX_H
#define S14_AI_AFFIX_H
#include <windows.h>
#include <stdint.h>
#define S14_AI_UNITS 501
#define S14_AI_CITIES 52
typedef struct {uint16_t leader,serial;uint32_t present,hit;uint64_t generation;} S14AiBinding;
typedef struct {uint64_t seed,rng,rolls,generation;S14AiBinding units[S14_AI_UNITS];uint64_t city_departures[S14_AI_CITIES];} S14AiState;
typedef struct {uint64_t rng,next,sequence,city_count,city_sequence;int eligible,hit,random_hit,guaranteed,city,leader,force,player;uintptr_t caller,world;} S14AiDraw;
void s14_ai_state_seed(S14AiState*,uint64_t);
int s14_ai_state_commit(S14AiState*,int,int,int,int,int,int,int,int,S14AiDraw*);
uint64_t s14_ai_random(uint64_t*);
int s14_ai_roll_hit(uint64_t);
int s14_ai_percent_max(int,int);
void s14_ai_draw(const S14AiState*,int,S14AiDraw*);
void s14_ai_attach(uintptr_t,const wchar_t*,int);
void s14_ai_configure(int);
void s14_ai_prepare(int,uintptr_t,S14AiDraw*);
void s14_ai_created(void*,const S14AiDraw*);
void s14_ai_creation_begin(const S14AiDraw*);
void s14_ai_creation_end(void);
int s14_ai_active(void*);
int s14_ai_person_active(uintptr_t,int);
int s14_ai_name(void*,const wchar_t*,wchar_t*,size_t);
int s14_ai_person_name(uintptr_t,int,const wchar_t*,wchar_t*,size_t);
void s14_ai_forget(void*);
void s14_ai_sweep(void);
int s14_ai_save(uintptr_t,const unsigned char*);
void s14_ai_load_begin(void);
void s14_ai_load_end(uintptr_t,int,const unsigned char*,int);
void s14_ai_new_game(uintptr_t);
int s14_ai_next_log(char*,size_t);
void s14_ai_status(wchar_t*,size_t);
int s14_ai_faulted(void);
int s14_ai_checkpoint_owned(const wchar_t*);
#endif
