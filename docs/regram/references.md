# Regram Desktop 源码参考

核对日期：2026-10-10。范围包含本目录的 iOS 功能清单和 129 项矩阵，以及用户追加的 Nagram-Qt、AyuGramDesktop 全部功能。两个客户端的功能增量已经导入 `Telegram/SourceFiles/regram/nagram/` 和 `regram/ayu/`，正在整合上游接入点、重叠设置和版本接口。源码导入不代表编译、运行或逐项验收通过。

| 来源 | 固定版本 | 用途 |
| --- | --- | --- |
| [Telegram Desktop](https://github.com/telegramdesktop/tdesktop/tree/42f8a36d43b8c805bc821905bea4cfeb3af1d41d) | v7.3.0，`42f8a36d43b8c805bc821905bea4cfeb3af1d41d` | 工程基线、原生设置与账户加密存储 |
| [Nagram-Qt](https://github.com/NextAlone/Nagram-Qt/tree/5d0222ca152396aa213f3455528ce09c544cce30) | main，`5d0222ca152396aa213f3455528ce09c544cce30` | 设置注册、过滤、菜单与输入、翻译、外观、网络及测试样本 |
| [AyuGramDesktop](https://github.com/AyuGram/AyuGramDesktop/tree/db3b9891cb0b04ebb7d8c0e71ada3bcc669b910a) | dev，`db3b9891cb0b04ebb7d8c0e71ada3bcc669b910a` | 幽灵模式、消息生命周期、撤回与编辑记录、翻译接入 |
| Regram iOS | `61c8a38bd6012faa2f00ddb1bedfc70007f390c7` | 行为、默认值、旧 JSON、会话 ID 编码和富文本样本 |

来源的 `main` / `dev` 会继续变化；本轮核对仅覆盖表中提交。源码参考下载保存在 Git 忽略的 `.cache/regram-references/`。iOS 原计划、矩阵和 `features.md` 当时尚未提交，已经复制到本目录并修复相对链接；源码链接使用固定 iOS 提交。

| 阶段 | 参考源码 | 采用前必须检查 |
| --- | --- | --- |
| M0 设置与身份 | Nagram [`core/options.cpp`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/Telegram/SourceFiles/nagram/core/options.cpp)、[`settings/home.cpp`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/Telegram/SourceFiles/nagram/settings/home.cpp)、[`BRANDING.md`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/BRANDING.md) | 账户作用域、KV 偏好持久化、跨窗口通知、独立运行身份及本地 API 配置 |
| M1 过滤 | Nagram [`filters/model.cpp`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/Telegram/SourceFiles/nagram/filters/model.cpp)、[`core/regex.cpp`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/Telegram/SourceFiles/nagram/core/regex.cpp) | 原参考使用 PCRE2；iOS 使用 ICU，兼容样本必须单独验证。不能把原参考的 JSON 格式当成 iOS 格式，或直接等同两端打包会话 ID |
| M2 菜单与输入 | Nagram `menu/`、`compose/`；[`上游接入点`](https://github.com/NextAlone/Nagram-Qt/blob/5d0222ca152396aa213f3455528ce09c544cce30/docs/nagram/upstream-hooks.md) | 普通聊天与话题两条渲染路径、发送与编辑、输入法、撤销、实体偏移 |
| M3 界面 | Nagram `interface/`、`chats/`、`privacy/profile.cpp` | 可发现入口、DPI、主题、键盘、原生平台差异；每项回映射至 RD 矩阵 |
| M4 翻译与转写 | Nagram `services/translation.cpp`、`draft_translation.cpp`、`transcription_queue.cpp`；AyuGram `features/translator/` | 请求归属、账户切换、取消、迟到结果、密钥保存、实际后端可用性 |
| M5 隐私与保留 | AyuGram [`data/messages_storage.cpp`](https://github.com/AyuGram/AyuGramDesktop/blob/db3b9891cb0b04ebb7d8c0e71ada3bcc669b910a/Telegram/SourceFiles/ayu/data/messages_storage.cpp)、[`data/ayu_database.cpp`](https://github.com/AyuGram/AyuGramDesktop/blob/db3b9891cb0b04ebb7d8c0e71ada3bcc669b910a/Telegram/SourceFiles/ayu/data/ayu_database.cpp) | 撤回、主动删除、TTL 的区别；原参考以 SQLite 保存内容，Regram 须接入账户本地加密能力并验证重启、清除和容量策略 |
| M6 媒体与网络 | Nagram `media/`、`network/` | 手动选择优先、下载与播放归属、设备拔插、三平台实际行为 |
| M7～M8 | 两个项目的测试、构建路径及发布资料 | 独立测量、逐项验收、安装隔离和发行包来源，不能沿用另一项目的通过结论 |

两个参考项目保留各自的源码许可与归属；复制或改写具体代码时记录来源并保留原有版权声明。Nagram 的图标是独立品牌资源，其 `BRANDING.md` 明确不以源码许可授予第三方品牌使用权。Regram 使用自己的资源；上游与第三方的既有归属信息继续保留。

提供的 API 凭据保存在 `.local/regram/api.cmake`，只供本机构建使用。公共模板为 `Telegram/build/regram/api.cmake.example`，文档、源码及提交不包含实际 API hash。签名凭据、登录资料和未来翻译服务密钥也分别使用本地配置。


Windows 可选图标使用带 PNG 图像的 ICO 容器，格式依据 [Microsoft 资源文件格式](https://learn.microsoft.com/en-us/windows/win32/menurc/resource-file-formats)。图像编码及容器字段在 macOS 隔离回归中验证；Windows Shell 的实际显示仍待平台验收。
