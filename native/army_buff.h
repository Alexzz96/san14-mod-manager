#ifndef S14_ARMY_BUFF_H
#define S14_ARMY_BUFF_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
int s14_army_buff_install(uintptr_t,uintptr_t);
int s14_army_buff_install_code(void);
uintptr_t s14_army_buff_failed_entry(void);
void s14_army_buff_configure(int);
int s14_army_buff_ready(void);
int s14_army_buff_enabled(void);
int s14_army_buff_next_log(char*,size_t);
#endif
