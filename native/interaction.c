#include "interaction.h"

int s14_check_phase(uintptr_t common, uintptr_t wrapper, int construction_thread) {
    // Return addresses, not call instruction addresses. Real creation guards
    // take precedence even while the player's construction state is open.
    if (common==0x37894d || (common==0x2514d5 && wrapper==0x26015d)) return S14_PHASE_ENFORCE;
    if (common==0x2514d5 && wrapper==0x70744c) return S14_PHASE_COMMIT;
    if (common==0x70d9fa) return S14_PHASE_PREVIEW;
    if (common!=0x2514d5) return S14_PHASE_ENFORCE;
    switch (wrapper) {
        case 0x6eebe0: case 0x7089fd: case 0x70d9bc:
        case 0x722571: case 0x724406:
            return S14_PHASE_PREVIEW;
        // The hex-node enumerator is shared with AI. Only bypass its rule
        // inside the player's construction state and on that state's thread.
        case 0x733cd: case 0x7370a: case 0x74245: case 0x74605:
            return construction_thread?S14_PHASE_PREVIEW:S14_PHASE_ENFORCE;
        default: return S14_PHASE_ENFORCE;
    }
}

int s14_check_result(int original,int allowed,int mode,int phase) {
    if (!original || allowed || mode!=2 || phase==S14_PHASE_PREVIEW) return original;
    return 0;
}
