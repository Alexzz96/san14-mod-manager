# 三国志 14 功能管理器 / SAN14 Mod Manager

Windows x64 原生插件与管理器，为已验证版本的三国志 14 提供可独立开启的扩展功能。

当前版本：**0.2.2 预发布**。墙体限制、5 秒提示和 0.2.1 的 F10 打开已实机确认；0.2.2 的安装反馈、游戏内热切换和回合结算仍待实机验收。

## 玩家使用

下载发布附件 `SAN14ModManager-0.2.2-windows-x64.zip`，解压并运行 `SAN14ModManager.exe`。保存存档、完全退出游戏后，在“安装与状态”中选择游戏目录并点击“安装 / 更新”。重新启动游戏，使用 **F10** 打开或收起管理器。

发行包只包含本项目安装器、说明、默认配置示例与第三方许可证。玩家不需要 Python 或编译器。GitHub 自动生成的 Source code 压缩包是开发源码，不是安装包。

| 功能 | 行为 |
| --- | --- |
| 扩展功能总开关 | 保留各项选择，统一启停追加功能 |
| 墙体连片上限 | 当前棋格所属势力相同的土垒、石墙合并为一片，最多 5 个；转弯、分叉、闭环都计数 |
| 超限悬浮提示 | 超限格仍可点击，显示规则并在约 5 秒后隐藏 |
| 诊断日志 | 默认关闭，可选详细记录 |

**不检查游戏版本号或 EXE 文件指纹。** 安装时只确认所选目录含 `SAN14PK_SC.exe`，不同构建或被修改的 EXE 不会因此被拒绝。启动接入前仍核对将要修改的实际代码位置和可读内存范围；入口对不上时不修改游戏代码，扩展功能停用，F10 管理器显示原因。这不表示所有游戏构建都已经实机验证。

安装器拒绝覆盖未知 `dinput8.dll`，支持本项目更新前备份及文件占用失败回退。开关共用 `SAN14BuildLimit.ini`。更多细节见 [使用说明](docs/USER_GUIDE.md)。

## 开发

要求 Windows x64、Python 3.12 或更高版本、**Zig 0.15.2**。MinHook v1.3.4 的必要源码与许可证已固定在仓库中。无需游戏文件即可构建并完成独立测试：

```powershell
python native/build.py --zig C:/tools/zig/zig.exe
python -m unittest discover -s tests -v
python native/verify_native.py
python native/package_release.py
```

也可将 Zig 加入 PATH 或设置 `ZIG_EXE`。构建输出在 `native/build`，打包输出在 `native/dist`。本机构建的文件哈希可能与发布附件不同；安装器始终嵌入本次 DLL 并记录本次哈希。

开发者可追加本地游戏代码接入点的静态校验：

```powershell
python native/verify_native.py --game-dir 'D:/Games/SAN14'
```

此命令只读原游戏 EXE，安装器测试在临时目录进行；若该目录的游戏正在运行，会额外验证更新被拒绝。它不会向游戏进程写内存、挂调试器或推进回合。游戏文件和临时副本不进入仓库或发布附件。

## 代码与项目管理

- `native/`：游戏接入、规则、提示、功能登记、界面和安装器。
- `reference/`：Python 规则参考实现；`tests/`：独立几何与规则测试。
- `docs/`：使用、架构、扩展方式、验证状态与后续计划。
- `.github/workflows/ci.yml`：Windows 构建、独立测试与候选安装包制品。

新功能通过登记表统一展示，按模块实现；版本发布使用标签与 Releases 附件。CI 通过不能替代游戏内验收。[贡献说明](CONTRIBUTING.md)、[验证状态](docs/VERIFICATION.md)、[后续计划](docs/ROADMAP.md)。

第三方依赖许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。本项目原创代码尚未指定开源许可证；仓库公开不代表已授予任意再分发许可。
