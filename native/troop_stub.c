/* Existing isolated component tests have no installed troop engine. */
#include "troop_runtime.h"
#include "personality_edit.h"
void s14_personality_service(void *state){(void)state;}
int s14_personality_capture(uintptr_t w,int id,unsigned short s[9]){(void)w;(void)id;(void)s;return 0;}
int s14_personality_submit(uintptr_t w,int id,const unsigned short s[9],int a,int t){(void)w;(void)id;(void)s;(void)a;(void)t;return S14_PE_UNAVAILABLE;}
int s14_personality_result(int *slot){(void)slot;return S14_PE_UNAVAILABLE;}
int s14_troop_icon_frame(S14TroopIconFrame *out){if(out)*out=(S14TroopIconFrame){0};return 0;}
float s14_troop_attribute(void *army,int attribute,int actual,float value){(void)army;(void)attribute;(void)actual;return value;}
float s14_troop_attribute_at(void *army,int attribute,int actual,float value,int mode,uintptr_t caller){(void)mode;(void)caller;return s14_troop_attribute(army,attribute,actual,value);}
int s14_troop_confusion_reject(void *army,int mode,int duration){(void)army;(void)mode;(void)duration;return 0;}
void s14_troop_status_service(void *army){(void)army;}
void s14_troop_clear_modifiers(void *army,int *out){(void)army;(void)out;}
int s14_troop_surround_mode(void *army,int mode,int actual,uintptr_t caller){(void)army;(void)actual;(void)caller;return mode;}
const S14TroopDefinition *s14_troop_bound(void *army){(void)army;return NULL;}
int s14_troop_damage(void *army,int amount){(void)army;return amount;}
void s14_troop_forget(void *army){(void)army;}
void s14_troop_load_begin(void){}
void s14_troop_load_end(uintptr_t world,int success,const unsigned char *hash,int valid){(void)world;(void)success;(void)hash;(void)valid;}
void s14_troop_new_game(uintptr_t world){(void)world;}
int s14_troop_save(uintptr_t world,const unsigned char *hash){(void)world;(void)hash;return 1;}
