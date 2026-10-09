#ifndef S14_PORTRAIT_H
#define S14_PORTRAIT_H
#include <windows.h>
#include <stdint.h>
typedef struct S14PortraitCache S14PortraitCache;
S14PortraitCache *s14_portrait_create(const wchar_t *root);
void s14_portrait_destroy(S14PortraitCache*);
/* Draw/queue only. All archive reads and decoding run on the asset thread. */
int s14_portrait_draw(S14PortraitCache*,int,HDC,const RECT*,int faded);
int s14_portrait_identity(uintptr_t base,uintptr_t world,int officer);
int s14_portrait_decode(const unsigned char*,size_t,uint32_t out[128*128]);
/* A synchronous read-only helper for isolated resource validation. */
int s14_portrait_extract(const wchar_t*,int,uint32_t out[128*128]);
#endif
