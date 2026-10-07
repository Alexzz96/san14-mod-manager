#ifndef S14_INTERACTION_H
#define S14_INTERACTION_H
#include <stdint.h>

enum { S14_PHASE_ENFORCE, S14_PHASE_PREVIEW, S14_PHASE_COMMIT };
int s14_check_phase(uintptr_t common_caller, uintptr_t wrapper_caller, int construction_thread);
int s14_check_result(int original, int allowed, int mode, int phase);

#endif
