#ifndef S14_PINYIN_H
#define S14_PINYIN_H
#include <stddef.h>
#include <wchar.h>
/* Literal, full pinyin, and initials; bounded query, offline heteronyms. */
int s14_search_contains(const wchar_t *text,const wchar_t *query,size_t length);
#endif
