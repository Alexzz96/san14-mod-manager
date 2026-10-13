#ifndef S14_TECH_TREE_UI_H
#define S14_TECH_TREE_UI_H
#include <stdint.h>
int s14_tech_ui_install(uintptr_t base,uintptr_t end);
int s14_tech_ui_create(void *page);
void s14_tech_ui_person(void *page,int officer,unsigned permission);
void s14_tech_ui_display(void *page,int show);
void s14_tech_ui_update(void *page);
void s14_tech_ui_release(void *page,void *resources);
/* Called by the existing native dispatch hook, never install a second hook. */
int s14_tech_ui_dispatch(void *container,int event,void *sender);
#endif
