#ifndef S14_FEATURES_H
#define S14_FEATURES_H
#include <windows.h>
#define S14_MANAGER_VERSION L"0.2.2"
#define S14_MASTER 1u
#define S14_WALL_LIMIT 2u
#define S14_LIMIT_HINT 4u
#define S14_DIAGNOSTICS 8u
#define S14_KNOWN_FLAGS 15u
typedef struct {
    const wchar_t *id,*key,*name,*description,*category;
    unsigned int flag, dependency;
    int live;
} S14Feature;
extern const S14Feature s14_features[];
extern const int s14_feature_count;
unsigned int s14_config_read(const wchar_t *path);
int s14_config_set(const wchar_t *path,unsigned int flag,int enabled);
unsigned int s14_effective_flags(unsigned int requested);
int s14_runtime_mode(unsigned int flags);
#endif
