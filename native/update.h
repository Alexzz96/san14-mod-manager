#ifndef S14_UPDATE_H
#define S14_UPDATE_H
#include <stddef.h>
#include <windows.h>
typedef struct {
    char version[32],url[512],sha256[65];
    unsigned int size;
    int newer;
} S14Release;
int s14_version_compare(const char *left,const char *right,int *comparison);
int s14_release_parse(const char *json,size_t size,const char *current,S14Release *release);
int s14_update_check(S14Release *release,wchar_t error[192]);
int s14_update_download(const S14Release *release,wchar_t path[MAX_PATH],wchar_t error[192]);
#endif
