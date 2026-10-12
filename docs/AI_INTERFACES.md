# AI 接口初步清单

本轮只做只读定位。没有新增 AI hook、自动命令或行为开关。已有的 AI 随机词条功能保留原实现，仍属于测试功能。

## 证据来源与等级

当前本地 `SAN14PK_SC.exe` 为 PE64，首选基址 `0x140000000`，SHA-256 为 `604f5fe9bdd6dc2b813b93333aabeadefbad50afd2b4f71a11718b7363bca8ef`。本文地址均为 RVA，运行时需要加实际模块基址。哈希只标识分析样本，不新增运行时整文件白名单。

- **仅发现类型名**：读取 MSVC RTTI，核对 TypeDescriptor、Complete Object Locator 和虚表落点。不能据此认定函数语义、参数或线程。
- **静态调用已核对**：反汇编核对具体指令和调用；仅记录已经看到的数据读写。
- **运行时已观察**：需要在真实游戏中记录对象、调用线程、参数、返回值及状态变化。本轮 AI 条目均未达到此等级。

[机器清单](AI_INTERFACE_INVENTORY.json) 收录 48 个相关类型，逐项提供名称 RVA、描述符、COL、虚表起点和连续代码指针候选。每项都有 signature、call_relationships、read_range、write_range 和 runtime_observed；未核实值明确为 null/false。RTTI 只能确定表的起点，不能确定长度；相邻回调数组也可能连续存放代码指针。因此只将首个指针确认为虚函数入口，其余保持候选，method_count 为 null、boundary_verified 为 false。例如 ChangePersonLoyalty 后方的连续指针不能认定为该类的 113 个虚函数。

可复现命令：

```powershell
python tools/inventory_game_interfaces.py --exe '你的游戏目录\SAN14PK_SC.exe' --out .mod-analysis/ai-inventory.json
```

工具只读 EXE，不附加进程。输出本身不证明接口已经可调用。

## 五层结构

| 层级 | 类型 | 名称 RVA | 虚表 RVA | 管理器成员 | 当前认识 |
| --- | --- | --- | --- | --- | --- |
| 总管理器 | CAIManager | 0x192f4d8 | 0x124f5d8 | 全局指针 0x1a1e6b0 | 初始化路径已静态核对 |
| 势力 | CForceAI | 0x192f450 | 0x124f598 | +0x10 | 初始化及相邻层连接已核对；更新逻辑未知 |
| 军团 | CDistrictAI | 0x192f470 | 0x124f5a8 | +0x18 | 同上 |
| 据点 | CBaseAI | 0x192f430 | 0x124f588 | 未核实 | 类型存在；不能把名字当成已发现的调用接口 |
| 出征编成 | CTroopsAI | 0x192f498 | 0x124f5b8 | +0x20 | 初始化已核对；编成算法和输入未知 |
| 部队行动 | CArmyAI | 0x192f4b8 | 0x124f5c8 | +0x28 | 初始化及少量调用链已核对 |

`CTroopsAI` 是本 EXE 的实际类型名。没有发现 `CFormationAI`，不据名称相似创造类型或签名。

`0xb34d0..0xb3964` 读取全局指针 `0x1a1e6b0`，已有对象时返回 0；分配 `0xf4300` 字节后，在 `0xb352d` 调用 `0x8bd10` 构造管理器。随后分别分配 `0x18` 字节的势力、军团、编成、部队 AI 对象，写入上述虚表，并放入管理器 +0x10、+0x18、+0x20、+0x28。子对象 +0x08/+0x10 连接相邻层；最外端置零。函数最后返回 1。没有证明该路径负责每回合更新，也没有证明 CBaseAI 在这条链中的位置。

`0x8bd10..0x8be3d`：RCX 为新管理器对象，返回 RAX 为该对象；写入虚表及初始化管理器 +0x08..+0xb0 的部分成员，调用 `0xc6790`、`0xc67f0`、`0xc6730`、`0xc6850`、`0xc66d0` 创建容器。容器的业务含义未知。

`0x8cc40..0x8cc74` 是 CAIManager 的删除析构包装，先调用 `0x8bf00`，按 EDX 低位决定释放对象；不是更新入口。`0x8cc80` 是多个小 AI 类共用的删除析构包装，不能当成某层决策函数。

## 评价、候选对象和排序

以下均为“仅发现类型名”，完整定位见机器清单。参数、返回值、评价权重、候选容量、排序稳定性和读写范围均未知。

| 层级或用途 | 线索 |
| --- | --- |
| 势力评价 | CSetForceEvaluationNode |
| 军团评价 | CSetDistrictEvaluationNode |
| 据点、区域、棋格 | CMiscSetFootholdEvaluationNode、CMiscSetAreaEvaluationNode、CMiscSetHexEvaluationNode |
| 武将候选 | CMiscSetPersonEvaluationNode |
| 出征编成、兵力、警戒 | CSetFormationEvaluationNode、CSetTroopsEvaluationNode、CSetCautionTroopsEvaluationNode |
| 战法书评价 | CSetTacticsBookEvaluationNode |
| 排序 | CListSortEvaluationNode |
| 攻击候选 | CGetAttackTargetArmyListNode、CGetAttackTargetFootholdNode |

候选清单类型和排序类同时存在，只能作为后续追踪入口。尚未证明哪一个评价输出流入哪一个排序节点。

## 出征、运输、目标、命令和撤退

| 用途 | 当前发现 |
| --- | --- |
| 出征入口/建队 | CDepartureNode、CDepartureTroopsCreateNode |
| 运输 | CDepartureTransportNode、CCommandTransportDepatureNode、CCommandFrontFootholdTransportDepatureNode |
| 区域出征/占领 | CCommandAreaTryGetDepartureNode、CCommandAreaOccupyDepatureNode |
| 防御涂色 | CCommandAutoPaintDefenseDepartureNode |
| 据点攻击 | CCommandFootholdAttackDepatureNode |
| 攻击目标 | CGetAttackTargetArmyListNode、CGetAttackTargetFootholdNode |
| 撤退 | 未核实独立类型或函数；不猜签名或命令编号 |

上述类型均未核实可调用签名。`Depature` 拼写来自 EXE，保留原名。

部队层有五条**静态已核对但语义仍未知**的包装链：

| 包装函数 | 调用点 → CArmyAI 成员方法 | 已确认的数据范围 | 返回值 |
| --- | --- | --- | --- |
| 0x1ddc0 | 0x1ddee → 0xb79f0 | 读取管理器 +0x28；获得并校验对象后调用；成功调用 0x245730 | 1 / 2，业务含义未知 |
| 0x1de20 | 0x1de4e → 0xbb470 | 同上；成功时对对象 +0x1f0 OR 0x40 | 同上 |
| 0x1de80 | 0x1deae → 0xbf250 | 同上；成功时 OR 0x02 | 同上 |
| 0x1dee0 | 0x1df0e → 0xc0580 | 同上；成功时 OR 0x04 | 同上 |
| 0x1df40 | 0x1df6e → 0xc0690 | 同上 | 同上 |

五个包装均先调用 `0xa9790`，再用 `0x2f29d0` 校验返回对象。此时 CArmyAI 方法的 RCX 来自管理器 +0x28，RDX 是被校验对象。包装外部参数的完整签名和 +0x1f0 各位含义未知，不把它们命名为“攻击/撤退/运输”。`0x245730` 的业务副作用也未核实。

`0xb4490..0xb4532` 通过管理器 +0x60 的树结构检索键值，相关调用见 `0xa8d0e`、`0xa8e58`、`0xa913a`；对象键是否为部队、编成或其他标识尚未知。

## 内政、探索、官职和物品分配

| 用途 | 已发现的类型线索 | 级别 |
| --- | --- | --- |
| 内政任命/移除 | CDomesticOfficerSetNode、CDomesticOfficerRemoveNode | 下层批量提交／结算调用已静态核对，见区域内政研究 |
| 内政开发优先 | CDomesticPriorityDevelopmentSelectNode | 选择函数调用已静态核对；完整参数与运行时待验 |
| 施政配置候选 | CCommandExecutionGovernmentNode、CCommandExecutionGovernmentAutoNode | 五组七槽操作更符合政策配置；不能当作区域官员自动分配入口 |
| 探索 | CCommandExecutionSearchNode | 类型名；现有自动探索模块的实际执行接口另见 auto_search.c，不将其当作 AI 策略接口 |
| 官职分配 | CCommandExecutionPostAutoNode | 类型名 |
| 名品分配 | CCommandExecutionItemNode | 类型名 |
| 战法学习 | CCommandExecutionLearnNode | 类型名 |
| 战法书评价 | CSetTacticsBookEvaluationNode | 类型名 |

这些节点虚表中的部分函数仅有 LEA/RET 结构，例如 `0x50870`、`0x50880`、`0x508a0`。必须核对返回地址的内容，不能把它们直接命名为授予物品、学习战法或任命官职的执行函数，也不能只凭指令形式认定返回的是另一张虚表。

2026-10-11 区域内政的进一步只读核对见 [自动配置可行性与执行路径](AUTO_DOMESTIC_FEASIBILITY.md)：任命／撤职节点与玩家内政界面共用 `0x1d5ba0` 的追加和 `0x1d1e80` 的批量结算。另发现 `CGetOfficerCandidateNode → 0x62cf40` 的候选生成路径，尚未加入本页原始机器清单。区域相关的 `0x3a320`、`0x3a310`、`0x877d0` 和 Government 的 `0x3a230`、`0x50850` 返回名称字符串，不是执行入口。本次仍没有新增 hook、调用游戏函数或开放自动配置。

## 编辑框架可复用证据与未解决接口

| 对象/字段 | 证据 | 当前可用范围 |
| --- | --- | --- |
| 个性九槽 | 0x21d5f0 原生 setter：EDX 0..8，R8D 0..355；写 +0x150，变更时清除 +0x1bc 对应位和 +0x1be 对应计时 | 已接入通用个性编辑；效果实机待验 |
| 个性旧效果 | 0x2748b0，已有原实现和模拟清理测试 | 替换/删除前调用；全目录效果待实机验 |
| 已学战法 | 0x204630..0x204757 扫描 +0x168 十个 uint8 编号，查询 0x76c00 目录 | 读取及容量静态核实；写入与后续缓存副作用未知 |
| 有效能力 | 0x20ce20，RCX 人物，EDX 能力索引，R8D 来源标志；原生调用使用 0xfff | 尚未接入查询面板；需规划线程调用和逐项归因验证 |
| 官职 | 有效能力函数读取人物 +0x11f、世界 +0x77c20，并调用 0x1d2990 | 只核实读取链；任命/撤销/调换及俸禄更新未知 |
| 名品 | CItemData；0x21a980 描述定义字段，0x215300 序列化 +0x4a；有效能力函数按 +0x4a 持有人筛选 | 持有人读取线索，不足以开放转移/收回写入 |
| 关系事件 | AddLike、SetPartner、Set_TreatyBrother 的事件类型存在 | 参数、关系槽容量、删除/重组清理未知 |
| 忠诚/能力/伤病事件 | ChangePersonLoyalty、ChangePersonParam、ChangeIllness 类型存在 | 类型名不等于安全调用入口；未开放写入 |

下一步需要找到真正的执行方法及关联清理，核对同一对象在保存和读档后的状态。尚未发现的容量、参数和副作用不能由类型名称或相邻字段推测。当前清单满足初步定位用途，不宣称完整逆向完成。

后续对五维、忠诚、学习扣书及官职调换尾部片段的核对详见 [当前 EXE 的人物编辑接口核查](NATIVE_EDIT_RESEARCH.md)。这些证据没有新增 AI 调用。
