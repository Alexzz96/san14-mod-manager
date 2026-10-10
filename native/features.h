#ifndef S14_FEATURES_H
#define S14_FEATURES_H
#include <windows.h>
#define S14_MANAGER_VERSION L"0.10.11"
enum { S14_SEARCH_WAITING=0,S14_SEARCH_MATCHED,S14_SEARCH_CONTEXT_PAUSED,S14_SEARCH_STOPPED,S14_SEARCH_UNAVAILABLE };
#define S14_MASTER 1u
#define S14_WALL_LIMIT 2u
#define S14_LIMIT_HINT 4u
#define S14_DIAGNOSTICS 8u
#define S14_AUTO_SEARCH 16u
#define S14_CAO_REN_BUFF 32u
#define S14_PLUGIN_TROOPS 64u
#define S14_AI_RANDOM_AFFIX 128u
#define S14_KNOWN_FLAGS 255u
typedef struct {
    const wchar_t *id,*key,*name,*description,*category;
    unsigned int flag, dependency;
    int live;
} S14Feature;
extern const S14Feature s14_features[];
extern const int s14_feature_count;
typedef struct { const wchar_t *id,*name,*description,*category; int action,children; } S14Mod;
enum { S14_MOD_WALL,S14_MOD_SEARCH,S14_MOD_BATTLE,S14_MOD_VIEWS,S14_MOD_CAO_REN_BUFF,S14_MOD_DIAGNOSTICS,S14_MOD_TROOPS,S14_MOD_AI_AFFIX,S14_MOD_COUNT };
extern const S14Mod s14_mods[S14_MOD_COUNT];
unsigned int s14_config_read(const wchar_t *path);
int s14_config_set(const wchar_t *path,unsigned int flag,int enabled);
unsigned int s14_effective_flags(unsigned int requested);
int s14_runtime_mode(unsigned int flags);
unsigned int s14_search_settings_read(const wchar_t *path);
int s14_battle_setting_read(const wchar_t *path);
int s14_views_setting_read(const wchar_t *path);
unsigned int s14_visual_settings_read(const wchar_t *path);
int s14_search_setting_set(const wchar_t *path,int group,int value);
#endif
