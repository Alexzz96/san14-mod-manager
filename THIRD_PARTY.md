# 第三方代码

MinHook v1.3.4，原项目：https://github.com/TsudaKageyu/minhook

固定提交：`c3fcafdc10146beb5919319d0683e44e3c30d537`。仅收录构建需要的 `include/`、`src/` 和原始许可、作者信息；源码未修改。

MinHook 及其 HDE 部分的完整许可和版权声明保存在 [LICENSE.txt](native/vendor/minhook/LICENSE.txt)，安装包也附带原始许可证。构建工具 Zig 和 Python 不随玩家安装包分发。

本项目原创代码的许可证尚待作者确定，当前不额外授予开源再分发许可。

头像资源的 zlib 解压使用 [miniz](https://github.com/richgel999/miniz) 的 tinfl 模块，固定提交 `2948eebafc5b9f1c69bebed80333e403a09bd62d`，MIT 许可。来源与原始哈希记录于 [UPSTREAM.json](native/vendor/miniz/UPSTREAM.json)，原始许可见 [LICENSE](native/vendor/miniz/LICENSE)；安装包包含 `Miniz-MIT-LICENSE.txt`。仅静态编译 tinfl 解压代码，不需要额外运行库。

游戏头像由玩家本地的 `0002/ScreenLayout.rdb` 和对应资源包只读加载，在内存中解压并缓存；源码和安装包均不附带游戏头像。RDB、G1T 的字段含义参照 [Cethleann](https://github.com/neptuwunium/Cethleann) 的格式定义，并以本地资源的大小、标识和边界交叉核对。

拼音字典来自 [pypinyin 0.55.0](https://github.com/mozillazg/python-pinyin)，MIT 许可；使用其基本多音字词表生成 `native/vendor/pinyin/readings.h`。原始许可与来源、版本和数据哈希见 [拼音词表目录](native/vendor/pinyin/UPSTREAM.json) 及 [LICENSE.txt](native/vendor/pinyin/LICENSE.txt)。安装包附带 `Pinyin-MIT-LICENSE.txt`；玩家无需安装 pypinyin。可选的 `native/generate_pinyin.py` 用于维护时重新生成词表。
