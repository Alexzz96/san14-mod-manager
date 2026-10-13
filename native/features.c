#define WIN32_LEAN_AND_MEAN
#include "features.h"
const S14Feature s14_features[]={
    {L"wall_cluster_limit",L"WallClusterLimit",L"墙体连片上限",L"同一势力领地内，相连的土垒和石墙合计最多 5 个。\n按六边格连接计算，拐弯和分叉也计入整片。",L"游戏规则",S14_WALL_LIMIT,0,1},
    {L"limit_hint",L"LimitHint",L"超限悬浮提示",L"点击超限位置时说明规则，5 秒后自动消失。\n重复点击会重新计时；需要开启墙体连片上限。",L"界面提示",S14_LIMIT_HINT,S14_WALL_LIMIT,1},
    {L"diagnostics",L"Diagnostics",L"诊断日志",L"记录规则检查和建造结果，用于定位异常。\n日常游玩可关闭，管理器的连接状态仍会显示。",L"辅助工具",S14_DIAGNOSTICS,0,1},
    {L"auto_search",L"AutoSearch",L"回合自动搜索",L"确认进行后，按设置使用剩余政令派遣探索。\n展开 Mod 可调整派遣条件并查看回合结果。",L"人才探索",S14_AUTO_SEARCH,0,1},
    {L"cao_ren_attack_defense",L"CaoRenAttackDefense",L"百战精锐 · 曹仁",L"曹仁已记录斩敌达到 5000（含伤兵），获得百战精锐。\n显示神 曹仁；实际攻军、防御 +10%，回合结束后解锁。",L"战斗增幅",S14_CAO_REN_BUFF,0,1},
    {L"plugin_troops",L"PluginTroops",L"扩展兵种 · 陷阵营",L"高顺出征可选择陷阵营；实际属性增幅与独立扣费。\n支持保存、读档；混乱免疫尚未启用。",L"扩展兵种",S14_PLUGIN_TROOPS,0,1},
    {L"ai_random_affix",L"AiRandomAffix",L"AI 随机词条",L"各 AI 城市第 9、18、27…次成功出征必出；其余按 10% 抽取。\n仅本次部队有效；不为已有部队补抽，同类增幅取最高。",L"随机词条",S14_AI_RANDOM_AFFIX,0,1},
    {L"huang_zhong_blink",L"HuangZhongBlink",L"闪袭背刺 · 黄忠",L"仅黄忠主将部队：直击后闪到敌军背后最近合法空格。\n搜索后方 1、2、3 格；一次直击背刺 +30%，冷却 5 游戏日。",L"位移词条",S14_HUANG_ZHONG_BLINK,0,1},
    {L"auto_domestic_fill",L"AutoDomesticFill",L"回合自动补齐内政",L"确认进行后先补齐自势力直辖区域，再派遣探索。\n保留现职、锁定与排除；命令书不足按优先级部分补齐。",L"内政",S14_AUTO_DOMESTIC,0,1},
    {L"archer_technology",L"ArcherTechnology",L"弓兵科技树",L"原生第四页学习弓弩五类基础强化，最多 8 点；进阶未接入。\n关闭暂停强化和学习，保留进度；开启同时启用弓弩弹药。",L"兵种培养",S14_ARCHER_TECH,0,1}
};
const int s14_feature_count=sizeof(s14_features)/sizeof(s14_features[0]);
const S14Mod s14_mods[S14_MOD_COUNT]={
    {L"wall_cluster_limit",L"墙体连片上限",L"同势力领地内，土垒与石墙相连最多 5 格。",L"规则",10,1},
    {L"auto_search",L"回合自动搜索",L"确认进行后使用剩余政令探索，回合结束汇总结果。",L"探索",13,1},
    {L"battle_events",L"战斗数据记录与战绩",L"采集全势力战斗，为累计战绩和武将时间线提供数据。",L"统计",41,0},
    {L"officer_views",L"武将情报",L"F 武将清单、武将战绩条和曹仁部队数值解析，可分别开启。",L"界面",44,1},
    {L"cao_ren_attack_defense",L"百战精锐 · 曹仁",L"斩敌达到 5000：神 曹仁，攻军、防御 +10%；随存档恢复。",L"战斗",14,1},
    {L"diagnostics",L"诊断日志",L"记录规则检查与建造结果，便于定位异常。",L"工具",12,0},
    {L"plugin_troops",L"扩展兵种 · 陷阵营",L"高顺专属；出征选兵种，额外费用 2000 + 每千兵 500。",L"兵种",15,0},
    {L"ai_random_affix",L"AI 随机词条",L"AI 城市每 9 次出征保底；其余 10%，攻军、防御 +10%。",L"随机",16,1},
    {L"huang_zhong_blink",L"闪袭背刺 · 黄忠",L"仅黄忠主将：后方 1→2→3 格找落点，一次直击背刺 +30%。",L"位移",17,1},
    {L"domestic_officers",L"区域官员配置",L"Ctrl+G 规划；支持每回合先补齐直辖内政，再派遣探索。",L"内政",62,1},
    {L"archer_technology",L"弓兵科技树",L"五类基础强化，最多 8 点；关闭暂停加成，保留学习进度。",L"兵种培养",19,0}
};
int s14_mod_page(int mod){
    return mod==S14_MOD_SEARCH || mod==S14_MOD_BATTLE || mod==S14_MOD_VIEWS || mod==S14_MOD_DIAGNOSTICS || mod==S14_MOD_DOMESTIC?S14_PAGE_FOUNDATION:S14_PAGE_TESTING;
}
const wchar_t *s14_mod_maturity(int mod){
    if(mod==S14_MOD_DOMESTIC)return L"自动已核对 / 快速设置待验";
    if(mod==S14_MOD_BLINK)return L"待实机验证";
    return mod==S14_MOD_SEARCH || mod==S14_MOD_BATTLE?L"稳定":mod==S14_MOD_VIEWS?L"查询稳定 / 编辑待验":mod==S14_MOD_DIAGNOSTICS?L"基础工具":L"测试";
}
static int boolean(const wchar_t *path,const wchar_t *section,const wchar_t *key,int fallback) {
    int value=(int)GetPrivateProfileIntW(section,key,fallback,path);
    return value==0?0:(value==1?1:fallback);
}
unsigned int s14_config_read(const wchar_t *path) {
    // A missing setting is a fresh installation. Preserve explicit legacy Mode=2.
    int legacy=(int)GetPrivateProfileIntW(L"Rule",L"Mode",-1,path);
    unsigned int flags=0;
    if (boolean(path,L"Manager",L"Enabled",legacy!=0)) flags|=S14_MASTER;
    if (boolean(path,L"Features",L"WallClusterLimit",legacy==2)) flags|=S14_WALL_LIMIT;
    if (boolean(path,L"Features",L"LimitHint",1)) flags|=S14_LIMIT_HINT;
    if (boolean(path,L"Features",L"Diagnostics",legacy==1)) flags|=S14_DIAGNOSTICS;
    if (boolean(path,L"Features",L"AutoSearch",1)) flags|=S14_AUTO_SEARCH;
    if (boolean(path,L"Features",L"CaoRenAttackDefense",0)) flags|=S14_CAO_REN_BUFF;
    if (boolean(path,L"Features",L"PluginTroops",0)) flags|=S14_PLUGIN_TROOPS;
    if (boolean(path,L"Features",L"AiRandomAffix",0)) flags|=S14_AI_RANDOM_AFFIX;
    if (boolean(path,L"Features",L"HuangZhongBlink",0)) flags|=S14_HUANG_ZHONG_BLINK;
    if (boolean(path,L"Features",L"AutoDomesticFill",0)) flags|=S14_AUTO_DOMESTIC;
    /* Keep the existing INI key so an upgrade preserves explicit preferences. */
    if (boolean(path,L"Troops",L"ArcherTechnology",0)) flags|=S14_ARCHER_TECH;
    return flags;
}
int s14_config_set(const wchar_t *path,unsigned int flag,int enabled) {
    const wchar_t *section=L"Features",*key=NULL;
    if (flag==S14_MASTER) { section=L"Manager"; key=L"Enabled"; }
    for (int i=0;i<s14_feature_count;i++) if (s14_features[i].flag==flag) key=s14_features[i].key;
    if (flag==S14_ARCHER_TECH) section=L"Troops";
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
static const wchar_t *search_keys[]={L"Executors",L"ReturnDays",L"Priority"};
static const unsigned int search_defaults[]={1,0,0};
unsigned int s14_search_settings_read(const wchar_t *path) {
    unsigned int packed=0;
    for (int i=0;i<3;i++) {
        unsigned int value=GetPrivateProfileIntW(L"AutoSearch",search_keys[i],search_defaults[i],path);
        if (value>(unsigned int)(i==0?3:i==1?2:1)) value=search_defaults[i];
        packed|=value<<(i*4);
    }
    return packed;
}
int s14_battle_setting_read(const wchar_t *path) {
    return boolean(path,L"Observation",L"BattleEvents",1);
}
int s14_views_setting_read(const wchar_t *path) {return boolean(path,L"Views",L"Enabled",1);}
unsigned int s14_visual_settings_read(const wchar_t *path){return (unsigned)boolean(path,L"Visuals",L"CaoRenHalo",1)|((unsigned)boolean(path,L"Visuals",L"CaoRenTooltip",1)<<1)|((unsigned)boolean(path,L"Visuals",L"AiAffixHalo",1)<<2)|((unsigned)boolean(path,L"Visuals",L"AiAffixTooltip",1)<<3);}
int s14_search_setting_set(const wchar_t *path,int group,int value) {
    if (group<0 || group>2 || value<0 || value>(group==0?3:group==1?2:1)) return 0;
    wchar_t text[8]; wsprintfW(text,L"%d",value);
    return WritePrivateProfileStringW(L"AutoSearch",search_keys[group],text,path)!=0;
}
