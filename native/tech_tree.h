#ifndef S14_TECH_TREE_H
#define S14_TECH_TREE_H
#include <stdint.h>
/* Native officer-detail technology preview; all construction runs on game thread. */
int s14_tech_tree_install(uintptr_t base,uintptr_t end);
/* Select our fourth page using the native radio-group event path. The caller
   scopes this to the new map entry; ordinary officer openings stay unchanged. */
int s14_tech_tree_select_fourth(void *dialog);
#endif
