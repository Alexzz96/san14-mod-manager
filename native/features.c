#define WIN32_LEAN_AND_MEAN
#include "features.h"
const S14Feature s14_features[]={
    {L"wall_cluster_limit",L"WallClusterLimit",L"墙体连片上限",L"同一势力领地内，相连的土垒和石墙合计最多 5 个。\n按六边格连接计算，拐弯和分叉也计入整片。",L"游戏规则",S14_WALL_LIMIT,0,1},
    {L"limit_hint",L"LimitHint",L"超限悬浮提示",L"点击超限位置时说明规则，5 秒后自动消失。\n重复点击会重新计时；需要开启墙体连片上限。",L"界面提示",S14_LIMIT_HINT,S14_WALL_LIMIT,1},
    {L"diagnostics",L"Diagnostics",L"诊断日志",L"记录规则检查和建造结果，用于定位异常。\n日常游玩可关闭，管理器的连接状态仍会显示。",L"辅助工具",S14_DIAGNOSTICS,0,1}
};
const int s14_feature_count=sizeof(s14_features)/sizeof(s14_features[0]);
static int boolean(const wchar_t *path,const wchar_t *section,const wchar_t *key,int fallback) {
    int value=(int)GetPrivateProfileIntW(section,key,fallback,path);
    return value==0?0:(value==1?1:fallback);
}
unsigned int s14_config_read(const wchar_t *path) {
    int legacy=(int)GetPrivateProfileIntW(L"Rule",L"Mode",2,path);
    unsigned int flags=0;
    if (boolean(path,L"Manager",L"Enabled",legacy!=0)) flags|=S14_MASTER;
    if (boolean(path,L"Features",L"WallClusterLimit",legacy==2)) flags|=S14_WALL_LIMIT;
    if (boolean(path,L"Features",L"LimitHint",1)) flags|=S14_LIMIT_HINT;
    if (boolean(path,L"Features",L"Diagnostics",legacy==1)) flags|=S14_DIAGNOSTICS;
    return flags;
}
int s14_config_set(const wchar_t *path,unsigned int flag,int enabled) {
    const wchar_t *section=L"Features",*key=NULL;
    if (flag==S14_MASTER) { section=L"Manager"; key=L"Enabled"; }
    for (int i=0;i<s14_feature_count;i++) if (s14_features[i].flag==flag) key=s14_features[i].key;
    if (!key || (enabled!=0 && enabled!=1)) return 0;
    // Change only this setting. Preserve future keys and another manager's
    // unrelated changes instead of rewriting a cached configuration file.
    return WritePrivateProfileStringW(section,key,enabled?L"1":L"0",path)!=0;
}
unsigned int s14_effective_flags(unsigned int requested) {
    if (!(requested&S14_MASTER)) return 0;
    unsigned int flags=requested&S14_KNOWN_FLAGS;
    for (int i=0;i<s14_feature_count;i++) {
        unsigned int dependency=s14_features[i].dependency;
        if (dependency && (flags&dependency)!=dependency) flags&=~s14_features[i].flag;
    }
    return flags;
}
int s14_runtime_mode(unsigned int flags) {
    flags=s14_effective_flags(flags);
    return flags&S14_WALL_LIMIT?2:(flags&S14_DIAGNOSTICS?1:0);
}
