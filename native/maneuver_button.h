#ifndef S14_MANEUVER_BUTTON_H
#define S14_MANEUVER_BUTTON_H
#include <stdint.h>
#include <stddef.h>
int s14_maneuver_button_install(uintptr_t base,uintptr_t end);
int s14_maneuver_button_next_log(char *out,size_t capacity);
#endif
