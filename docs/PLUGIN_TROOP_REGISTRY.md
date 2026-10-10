# 插件兵种注册表 · 陷阵营

0.10.0 本地版已增加高顺陷阵营的游戏适配：出征选择、独立图标、原生计算后的属性倍率、额外扣费、保存与读档。实机验收待完成；混乱免疫不启用。本文保留纯注册表/报价工具说明，游戏运行流程与限制见 [TROOP_RUNTIME.md](TROOP_RUNTIME.md)。

## 首个兵种设计

| 项目 | 首版草案 |
| --- | --- |
| 独立 ID / 修订 | `san14.xianzhen` / `1` |
| 名称 / 定位 | 陷阵营 / 高顺专属攻坚步军 |
| 允许主将 | 高顺，规范武将编号 254 |
| 原生承载 | 大戟，原生编号 1；沿用地形、兵模和原生规则 |
| 独立图标 | `assets/troops/xianzhen.svg`，本项目绘制的盾牌与双戟 |
| 解锁资格标识 | `troop.xianzhen`；当前仅接受调用者明确传入的资格，尚未连接技能学习 |
| 攻军 / 防御 | +300% / +200% |
| 攻城 / 破城 / 机动 | +10% / +20% / −20% |
| 特殊效果描述 | 直接交战减伤 80%、混乱免疫、包围无效；实机验收待完成 |
| 额外出征军费 | 固定 2000 金 + 每千人 500 金，不足千人按千人计 |

军费是**原生出征费用之外的额外费用**。陷阵营的 `max_soldiers` 为 1000；1～1000 兵额外 2500 金，超过上限返回 `soldiers_invalid`。省略该字段的旧目录兼容默认上限 100000。报价中的可用资金应来自未来原生费用结算后的余额，不能只验证出征前余额就认为费用足够。

属性预览以每次原生最终结果为基数，计算一次 `原生结果 × (10000 + bonus_bp) / 10000`。所有倍率使用整数基点，10000 表示 100%。不覆盖武将白字基础属性，不把上一次增加后的结果再次作为原生基数。后续蓝字显示、真实战斗和机动计算需要各自适配，目前预览不等于这些入口已实现。

上述数值是便于核对的可编辑设计草案，不代表已完成游戏平衡测试。解锁和出征选择是两件事：解锁高顺的资格，不会让他的普通大戟或重骑自动变成陷阵营；只有明确选择 `san14.xianzhen` 的那一次出征才应获得独立身份。

## 文件与构建

- `data/troops.json`：唯一兵种定义来源，UTF-8，支持多个兵种。当前最多 256 个，数量与原生 0～20 编号独立。
- `tools/troop_catalog.py`：严格校验整份定义，再生成 C 目录和独立 HTML 预览。拒绝重复 ID、重复 JSON 字段、未知字段/效果、无效原生承载、费用或倍率越界、非法武将范围及缺失图标。
- `native/troop_registry.h` / `.c`：纯 C 注册表和资格、报价、属性计划接口，无游戏函数调用。
- `native/troop_catalog_cli.c`：开发用目录查询和军费模拟工具。
- `tools/troop_preview.html`：设计预览模板，示例数值不是读取到的游戏数据。
- `native/test_troop_registry.c`、`tests/test_troop_catalog.py`：独立验证；整体构建与 verifier 已接入。

仅生成定义和预览，无需游戏或编译器：

```powershell
python tools/troop_catalog.py
```

输出 `native/build/troop_catalog.h` 和 `native/build/troops-preview.html`。预览内含图标，可直接打开 HTML，也可以通过本地 HTTP 服务查看。兵力、资金、武将编号、原生权限和模拟解锁资格可以调整；改变示例原生数值，可以核对每项倍率。

完整构建和校验沿用项目命令：

```powershell
python native/build.py --zig C:/tools/zig/zig.exe
python -m unittest discover -s tests -v
python native/verify_native.py
native/build/troop_catalog.exe --list
native/build/troop_catalog.exe --quote san14.xianzhen 254 1000 10600 1
```

最后一条返回 `ok`、额外军费 2500 和 `funds_deducted:false`。末尾的 `1` 是模拟已解锁；`0` 返回 `locked`。成功表示满足**注册表模拟规则**，不代表可以调用原生出征提交入口。

当前定义是**构建时配置**：修改 JSON 后需要重新构建插件。当前不支持运行中读取玩家修改的 JSON 或热更新注册表。0.10.7 为陷阵营增加专用 D3D11 盾戟图标，与目录 SVG 使用相同设计；不是通用 SVG 运行时加载器，新兵种的图标仍需添加对应绘制适配。

## C 接口契约

内置目录 `s14_troop_builtin_registry()` 是只读稳定快照，不需要运行时解析或分配。扩展方也可创建 builder，逐项 `register`，再 `seal` 后发布给其他线程。

```c
const S14TroopRegistry *r = s14_troop_builtin_registry();
const S14TroopDefinition *d = s14_troop_registry_find(r, "san14.xianzhen");
S14TroopRequest request = {
    .commander_id = 254, .native_carrier = 1,
    .native_allowed = 1, .unlocked = 1,
    .soldiers = 1000, .treasury = 10600
};
S14TroopQuote quote;
S14TroopResult result = s14_troop_quote(r, d->id, &request, &quote);
/* result == S14_TROOP_OK; quote.extra_gold == 2500; no funds are changed. */
```

`register` 深复制结构，拒绝覆盖已有 ID，失败不增加条目数。builder 是单线程使用；封存后不能追加。封存前的 `at/find` 指针可能因扩容失效，封存后的指针稳定，直到 registry 被销毁。销毁前必须停止所有读取；内置目录禁止销毁或写入。

`quote` 使用独立 ID、主将、显式解锁、原生承载权限、所选承载类型、兵力与资金校验。字符串 ID 不写入原生 formation 字段，不依赖武将 `+0x164` 权限位增加新的原生编号。调用者仍须提供原生实际许可，插件注册不越过原生许可。

`plan_attributes` 只计算计划值；失败不改变输出，不更改输入数组。属性数组顺序与现有 `S14_ARMY_*` 相同：攻军、攻城、破城、机动、防御。界面可独立安排显示顺序。

特殊效果目前只有类型化描述：`damage_reduction`、`status_immunity/confusion`。新效果必须新增类型、校验与适配，未知名称不会静默执行。`required_capabilities` / `missing_capabilities` 返回此兵种需要的出征身份、选择界面、属性、费用、减伤和免疫适配能力；纯目录 CLI 的适配能力仍为 0；游戏运行适配能力为 31，缺少混乱免疫。登记一个效果不意味着执行它。

## 本次验证与后续边界

独立用例覆盖 256 项登记（超过原生 21 项）、重复 ID、封存、深复制、非法 UTF-8、资格与承载隔离、费用向上取整及资金边界、非有限属性拒绝、1000 次同基数预览不累乘。Python 检查定义、生成器、图标与脚本边界；verifier 比较编译目录和 JSON，防止两者分叉。

后续真正进入游戏，需要接入额外兵种按钮与图标、预览和提交事务、只属于这一支部队的身份绑定、取消/失败/重复提交、实际属性及特殊效果结算、普通和自动存档的恢复。不能用可复用的部队内存地址、原生兵种编号或主将编号单独代替特殊兵种身份。技能树、经验消费、重骑机动技能以及原生兵种池扩容均不属于本次实现。
