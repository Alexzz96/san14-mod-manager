#ifndef S14_TROOP_RUNTIME_H
#define S14_TROOP_RUNTIME_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "troop_registry.h"
typedef struct {
    uintptr_t state,tab,world;
    int visible,selected,commander,soldiers,carrier,gold,native_cost,extra_cost,max_soldiers;
    int x,y,w,h;
    wchar_t message[160];
} S14TroopFrame;
typedef struct {float x,y,size;int selected,detail;} S14TroopIconFrame;
int s14_troop_icon_frame(S14TroopIconFrame *frame);
int s14_troop_install(uintptr_t base,uintptr_t end);
int s14_troop_ready(void);
void s14_troop_configure(int enabled);
void s14_troop_root(const wchar_t *root);
void s14_troop_frame(S14TroopFrame *frame);
void s14_troop_request(const S14TroopFrame *frame);
float s14_troop_attribute(void *army,int attribute,int actual,float value);
float s14_troop_attribute_at(void *army,int attribute,int actual,float value,int mode,uintptr_t caller);
const S14TroopDefinition *s14_troop_bound(void *army);
int s14_troop_damage(void *army,int amount);
void s14_troop_forget(void *army);
void s14_troop_load_begin(void);
void s14_troop_load_end(uintptr_t world,int success,const unsigned char *hash,int valid);
void s14_troop_new_game(uintptr_t world);
int s14_troop_save(uintptr_t world,const unsigned char *hash);
int s14_troop_next_log(char *out,size_t cap);
/* Adapters run inside existing native callbacks, independently of logging. */
int s14_troop_confusion_reject(void *army,int mode,int duration);
void s14_troop_status_service(void *army);
void s14_troop_clear_modifiers(void *army,int *out);
int s14_troop_surround_mode(void *army,int mode,int actual,uintptr_t caller);
unsigned s14_troop_capabilities(void);
#endif
