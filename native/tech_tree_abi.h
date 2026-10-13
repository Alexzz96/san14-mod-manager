#ifndef S14_TECH_TREE_ABI_H
#define S14_TECH_TREE_ABI_H
#include <stdint.h>
/* Read-only footer after the 51 native virtual slots. The detail observer
   recognizes our lifecycle wrappers without treating arbitrary pages as ours. */
#define S14_TECH_PAGE_MAGIC UINT64_C(0x5331345445434831)
typedef struct {uint64_t magic,version,original_vtable;} S14TechPageIdentity;
#endif
