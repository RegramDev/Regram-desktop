# Regram Desktop 7.3.1 · regram.2

本版安装包源码为 `898623d0ec217c7610d11328d4c705af16de6d95`。两个 macOS 包来自本地独立 Release 构建；Windows、Linux 构建使用同一提交。`v7.3.1-regram.2` 是独立 Release 草稿，不混用旧 `regram.1` 的来源或产物。

| 平台 | 构建与打包 | 运行证据 |
| --- | --- | --- |
| macOS Apple Silicon | 完整 Release 编译、链接和 DMG 校验通过；已交付 | DMG 中的应用启动；私有真实账号副本设置与菜单检查 |
| macOS Intel | 完整 Release 编译、链接和 DMG 校验通过；已交付 | Apple Silicon 上通过 Rosetta 启动；原生 Intel 未测试 |
| Windows x64 | [Actions 构建中](https://github.com/RegramDev/Regram-desktop/actions/runs/38171024897) | 待安装、字体及启动检查 |
| Linux x64 | [原生 Release 编译与打包通过](https://github.com/RegramDev/Regram-desktop/actions/runs/38171026920)；包已交付 | 92 份字体、独立目录安装和 12 秒启动检查通过；来源、ELF 架构和 SHA-256 一致 |

macOS 最低版本为 12.0，使用 ad-hoc 签名，未公证；最低系统上的运行尚未验证。两个包的应用标识均为 `dev.regram.desktop`，架构与来源清单匹配，包内不包含账号数据。

| 文件 | SHA-256 |
| --- | --- |
| Regram-7.3.1-macOS-arm64.dmg | `7ec0c6a0381bea86f39834f4535fe3655fcdeabccff5545dc19a24c0a3c9c353` |
| Regram-7.3.1-macOS-x86_64.dmg | `384b997f5612cff648ea19a1260c4ba97c1c6fb89448a2aefecd0766b200e3bf` |
| Regram-7.3.1-Linux-x64.tar.xz | `ecd3868523cb7525d09a744a923c7dd0dc8843b18f058aa3ac4793fadcc60281` |

本轮通过 33 组功能回归和 5 组专项 Release 测试，72 个受影响生产编译单元通过实际优化参数检查，92 份 macOS 字体通过文件大小、哈希、Qt 注册及预览渲染。两份 DMG、Linux 包和各自校验文件、清单已上传新草稿，远端资产摘要与本地文件一致。

已在正式包的真实账号副本确认设置入口和标题、旧说明移除、无 Yandex 的翻译来源列表、置顶下方整聊翻译栏、过滤器与输入设置、消息菜单拖拽和取消恢复。频道和群聊的滚动输入检查完成，但没有帧率测量。格式面板自动输入未取得可靠结果；系统 Animation Hitches 采集输出未完成，未接受为帧率证据；全部消息操作、翻译后端、富文本交互和完整 iOS 对齐仍待验收。具体实现边界见 [iOS 对齐记录](ios-parity.md)，整体状态保持未完成。

发布准备时重新检查 Telegram Desktop 的稳定版及 dev：最新稳定版为 7.3.1，dev 为 `6fed91ffab9861771a75f29df65031b3c80b6941`，相对本版没有缺失提交。公开发布前仍须再次检查上游，并等待各平台核验结果。

## English

Packages use source `898623d0ec217c7610d11328d4c705af16de6d95`. Both macOS architectures passed full Release compilation, linking, DMG integrity/signature checks and packaged startup. Intel startup was checked with Rosetta on Apple Silicon. Linux x64 passed native compilation, 92 font checks, installation and fresh-profile startup; source, ELF architecture and checksums match. Windows remains in progress at the linked run. The new release remains a draft with its own provenance.

Requires macOS 12.0 or later. Packages are ad-hoc signed and not notarized; macOS 12 and native Intel hardware remain untested. All 33 regression groups, five focused Release suites, 72 affected compilation-unit checks and 92 macOS font checks passed. Limited real-account GUI checks passed, including settings navigation, translation controls and menu reordering. Scroll input was exercised without an FPS benchmark. Formatting-panel interactions, all message actions and full iOS parity remain unverified.
