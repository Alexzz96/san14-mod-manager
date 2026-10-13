#ifndef S14_ARCHER_TECH_H
#define S14_ARCHER_TECH_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include "tech_tree_model.h"
void s14_archer_tech_attach(uintptr_t base,const wchar_t *root,int ready);
void s14_archer_tech_configure(int enabled);
int s14_archer_tech_ready(void);
int s14_archer_tech_enabled(void);
int s14_archer_tech_can_learn(int officer);
unsigned short s14_archer_tech_mask(int officer);
int s14_archer_tech_learn(int officer,int node);
float s14_archer_tech_attribute(void *army,int attribute,int actual,float value);
void s14_archer_tech_ammo(int officer,int base_capacity,int base_recovery,int *capacity,int *recovery_hundredths);
int s14_archer_tech_save(uintptr_t world,const unsigned char *hash);
void s14_archer_tech_load_begin(void);
void s14_archer_tech_load_end(uintptr_t world,int success,const unsigned char *hash,int valid);
void s14_archer_tech_new_game(uintptr_t world);
int s14_archer_tech_next_log(char *out,size_t cap);
#endif
