#ifndef S14_PERSON_ENTRY_H
#define S14_PERSON_ENTRY_H
#include <stdint.h>
#include <stddef.h>
int s14_person_entry_init(uintptr_t base,uintptr_t end);
int s14_person_entry_open(void);
/* UI-thread polling: 1 once our information stack has closed and the original
   map state is active again; -1 on scene replacement, 0 while waiting. */
int s14_person_entry_return_ready(void);
void s14_person_entry_cancel_return(void);
int s14_person_entry_next_log(char *out,size_t capacity);
#endif
