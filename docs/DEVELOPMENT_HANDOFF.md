# 开发接续索引

更新：2026-10-12。当前源码版本：`0.11.0-dev.14`。区域官员配置、直辖自动补齐和结果刷新说明见 [内政功能](DOMESTIC_OFFICERS.md) 及 [本版修复](RELEASE_NOTES_0.11.0-dev.14.md)。

本地原始源码来自 GitHub 下载包；当前修改通过独立分支和 PR 同步，主分支是否包含它以合并状态为准。游戏本体与插件源码分别保留，不把游戏 EXE、存档、私有分析文件纳入源码交付。

## 基础编辑框架的真实状态

已经落地：管理器“基础功能 / 测试功能 / 设置”分区；自动探索、F 查询和战绩统计的稳定性标记；355 项个性目录搜索；九槽暂存添加、替换、删除；应用请求和结果编号；整个人物修改前快照、会话校验、游戏规划线程执行、读档/执行阶段/关闭取消，以及旧效果清理和复读核对。

尚未完成：基础属性和原生有效五维、已学战法编辑、出征装备只读、官职任免调换、亲爱厌恶和结义、战法书库存、名品转移，以及完整六页编辑器与 F“武将编辑”入口。这些操作不能标为“只剩实机验收”。

公共编辑请求目前只接入个性；其他操作返回具体能力缺失原因。候选包门槛关闭，只允许明确标记未完成的本地开发包。

重要文档：

- [基础框架原始整理](FOUNDATION_FRAMEWORK.md)
- [实施状态与验收记录](FOUNDATION_IMPLEMENTATION.md)
- [当前 EXE 原生编辑接口证据](NATIVE_EDIT_RESEARCH.md)
- [个性编辑操作说明](PERSONALITY_EDITOR.md)
- [AI 五层接口清单](AI_INTERFACES.md)与 [RTTI 机器清单](AI_INTERFACE_INVENTORY.json)

## 特殊能力讨论的接续位置

用户提出：词条可以承载特殊能力，例如兵种攻击带有击退效果。

讨论草案保存于 [特殊能力与击退设计](SPECIAL_EFFECT_DESIGN.md)。它是设计和接口研究任务，没有新增击退代码、JSON 效果类型、游戏 hook 或 AI 行为修改，也没有把击退设置成已启用功能。

当前游戏 EXE 已有长枪击退扩展，静态证据保存于 [长枪击退逻辑](LONG_SPEAR_KNOCKBACK.md)：`.sp5push` 检查有效兵种 5，规划相邻目标后退一格，受阻补混乱，并调用现有移动包装及路径处理。插件尚未接入；后续必须处理与 EXE 已有触发的重叠、来源去重范围和混乱免疫兼容。

用户已指定黄忠部队测试。`0.11.0-dev.3` 接入 [黄忠专属闪袭背刺](HUANG_ZHONG_BLINK_TEST.md)：双校验原始编号 271 与唯一原始姓名，默认关闭，仅主将；相邻真实普通直击后，延迟到当前 EXE 长枪批处理返回再移动来源，后方 1→2→3 格寻找最近合法落点；下一笔对原目标的合法普通直击 +30%，冷却/机会各 5 游戏日。新增批处理和限定作用范围的方向适配两个 hook，复用原有扣兵入口；不 hook 传递原生 RBP 上下文的包装。

`native/blink_backstab.c` 实现身份、占位、执行代次、故障停用、一次机会及 `.s14blink` 保存恢复。`native/test_blink_backstab.c` 只使用合成对象，覆盖六向、多格、故障、跨进程保存和旧档回退。`tools/verify_blink_anchors.py` 只读取 EXE，并可在自身进程内测试复制代码的 MinHook 跳板；不安装或连接游戏。

当前运行游戏只读核对了黄忠编号 271、唯一原始姓名和 18 处代码锚点。实际能力位移、显示、伤害、保存重启均保持待验收。通用设计见 [闪袭 / 位移背刺](BLINK_BACKSTAB_DESIGN.md)，敌军真实朝向、通用配置挂载和沿途跨山/墙限制尚未实现；后方按本次接敌轴定义。

现有基础代码：

- `native/troop_registry.h`：兵种定义、效果描述及能力位。
- `native/troop_runtime.c`：活动部队绑定、属性、减伤、混乱和包围相关适配。
- `native/ai_affix.c`：AI 词条抽取、出征身份、独立随机和存档恢复。
- `native/battle_probe.c`：战斗来源/目标快照、战法调用关联和实际扣兵前后状态。
- `native/battle_place.c`：地图棋格与地区的只读查询，不能用作部队位移设置接口。

注意：部分战斗来源需要更多核实；当前 EXE 的直接交战调用发生过改写，已有减伤调用点不能未经核对就充当新特殊能力的触发点。战绩采集口径保持原有实现。

## 本机验证和交付

固定 Zig：`.tools/zig/zig.exe`，版本 `0.15.2`。
使用可用的 Python 3.12+；若系统 PATH 中的 python 不可用，请指定自己的解释器路径。

构建：`native/build.py --zig .tools/zig/zig.exe`。
Python 测试：`-m unittest discover -s tests -v`。
原生验证：`native/verify_native.py`；不加真实游戏目录安装参数。
开发打包：`native/package_release.py --local --development`。

2026-10-10 按用户要求重新构建安装包，35 项 Python 测试与完整原生回归再次通过；结果为 `native/build/verification.json`。上一轮桌面焦点测试有一次被外部前台变化打断，未放宽断言，顺序重跑通过；本次原生回归直接通过。游戏中的新增效果、保存重启和旧档回退仍待验收。

上一轮本地安装器位于 `native/dist/SAN14ModManager-0.11.0-dev.1-windows-x64.exe`；含说明的完整包位于同目录同名 ZIP。该版本只含设计，没有实现闪袭。当前版本改用 `0.11.0-dev.3` 的同名文件，仍使用 `--local --development`，清单分别标明黄忠背刺已接入/实机待验收、插件击退未实现和基础编辑未完成。不要将 dev.1 的验证或哈希套用到 dev.2。

上一轮 dev.1 安装器 SHA-256：`99efb175f0a2d19edf8eb5c12db97f64397e8f98e8831e88a9b6e8fa77399365`。
上一轮 dev.1 ZIP SHA-256：`a7d245c9f3b2e3e2f4502fd44cab193b22d2f0ff9d9cb259905436cf480e2be5`。

游戏 EXE 分析样本 SHA-256：`604f5fe9bdd6dc2b813b93333aabeadefbad50afd2b4f71a11718b7363bca8ef`。本次打包后再次核对未改变，未自动安装插件或发布 GitHub Release。

黄忠测试版最终使用独立目录 `native/build-blink-test` 和 `native/dist-blink-test`，避免同目录其他构建覆盖或锁住 DLL。构建加 `--build-dir native/build-blink-test`，原生验证也加同一参数；打包加 `--build-dir native/build-blink-test --dist-dir native/dist-blink-test --local --development`。默认 `native/build` 仍兼容已有工作流。构建逐个编译 C 单元并在同一次构建内复用相同选项的对象，缓解游戏运行时的编译内存峰值。

最终完整原生回归与 35 项 Python 测试通过；独立静态报告核对当前 EXE 18 处背刺锚点、41 处复用入口和两个复制代码的 MinHook 跳板，位于 `native/build-blink-test/blink-static-verification.json`。修复了分区列表滚动高度按全局序号计算的问题，新增黄忠面板与有效/损坏 `.s14blink` 清理回归。真实游戏能力验收仍未执行。

最终安装器：`native/dist-blink-test/SAN14ModManager-0.11.0-dev.3-windows-x64.exe`，SHA-256 `6041f12bcdd7e1d3139f36019f9fba314024c0643226cb6248e7af801685fdb6`；同名 ZIP SHA-256 `20a85684732c2578ce1a15003b84a560bbbcc93cafc82f7f55befa3320ad2190`。ZIP CRC、包内 SHA-256 和安装器与验证产物一致性检查通过；EXE 样本哈希保持不变。
