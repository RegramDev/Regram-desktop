# Regram Desktop 7.3.1 · regram.2

本版 macOS/Linux 安装包源码为 `898623d0ec217c7610d11328d4c705af16de6d95`，Windows 源码为 `b9b60b0b3d728bfbe214b17a798aa34b280c18f9`。两个 macOS 包来自本地独立 Release 构建。Windows 的后续提交仅修改分发流程和文档，应用功能源码相同；每个平台以包清单为来源依据。`v7.3.1-regram.2` 作为独立预发布提供，不混用旧 `regram.1` 的产物。

| 平台 | 构建与打包 | 运行证据 |
| --- | --- | --- |
| macOS Apple Silicon | 完整 Release 编译、链接和 DMG 校验通过；已交付 | DMG 中的应用启动；私有真实账号副本设置与菜单检查 |
| macOS Intel | 完整 Release 编译、链接和 DMG 校验通过；已交付 | Apple Silicon 上通过 Rosetta 启动；原生 Intel 未测试 |
| Windows x64 | [单平台重跑成功](https://github.com/RegramDev/Regram-desktop/actions/runs/38191607882)；安装版及便携版已交付 | 92 份字体、静默安装、安装文件一致性及独立启动检查通过；架构、来源和 SHA-256 已核对 |
| Linux x64 | [原生 Release 编译与打包通过](https://github.com/RegramDev/Regram-desktop/actions/runs/38171026920)；包已交付 | 92 份字体、独立目录安装和 12 秒启动检查通过；来源、ELF 架构和 SHA-256 一致 |

macOS 最低版本为 12.0，使用 ad-hoc 签名，未公证；最低系统上的运行尚未验证。两个包的应用标识均为 `dev.regram.desktop`，架构与来源清单匹配，包内不包含账号数据。

| 文件 | SHA-256 |
| --- | --- |
| Regram-7.3.1-macOS-arm64.dmg | `7ec0c6a0381bea86f39834f4535fe3655fcdeabccff5545dc19a24c0a3c9c353` |
| Regram-7.3.1-macOS-x86_64.dmg | `384b997f5612cff648ea19a1260c4ba97c1c6fb89448a2aefecd0766b200e3bf` |
| Regram-7.3.1-Linux-x64.tar.xz | `ecd3868523cb7525d09a744a923c7dd0dc8843b18f058aa3ac4793fadcc60281` |
| Regram-7.3.1-Windows-x64-setup.exe | `8570aca70c74ac118ee9191250abfb03fd21ef2b9e9b2eda2c6e0f5100e18884` |
| Regram-7.3.1-Windows-x64-portable.zip | `14cf9d5ca89c0a93b952f58674eec2c2f2f9b739e007569132c1fe61dbd9df58` |

本轮通过 33 组功能回归和 5 组专项 Release 测试，72 个受影响生产编译单元通过实际优化参数检查，92 份 macOS 字体通过文件大小、哈希、Qt 注册及预览渲染。两份 DMG、Windows 安装版／便携版、Linux 包及各自校验文件、清单已上传本版；远端资产摘要与本地文件一致。Windows 和 Linux 各自的 92 份字体检查通过。Windows 链接从最后一个源码项至成功完成约 14 分 29 秒；首轮 LNK1102 的同阶段约 19 分钟后失败。依赖缓存恢复 5 分 23 秒、准备 1 秒，未重新编译 Qt。Windows 分发包不附 PDB，安装程序未签名。

已在正式包的真实账号副本确认设置入口和标题、旧说明移除、无 Yandex 的翻译来源列表、置顶下方整聊翻译栏、过滤器与输入设置、消息菜单拖拽和取消恢复。频道和群聊的滚动输入检查完成，但没有帧率测量。格式面板自动输入未取得可靠结果；系统 Animation Hitches 采集输出未完成，未接受为帧率证据；全部消息操作、翻译后端、富文本交互和完整 iOS 对齐仍待验收。具体实现边界见 [iOS 对齐记录](ios-parity.md)，整体状态保持未完成。

发布准备时重新检查 Telegram Desktop 的稳定版及 dev：最新稳定版为 7.3.1，dev 为 `6fed91ffab9861771a75f29df65031b3c80b6941`，相对本版没有缺失提交。四个平台核验后再次检查上游，稳定版和 dev 均未变化。

## English

Packages use source `898623d0ec217c7610d11328d4c705af16de6d95`. Both macOS architectures passed full Release compilation, linking, DMG integrity/signature checks and packaged startup. Intel startup was checked with Rosetta on Apple Silicon. Linux x64 passed native compilation, 92 font checks, installation and fresh-profile startup; source, ELF architecture and checksums match. The Windows-only rerun from `b9b60b0b3d728bfbe214b17a798aa34b280c18f9` passed compilation, linking, 92 font checks, installation and startup. Its application source matches the other packages; distribution tooling skips full PDB generation while retaining `/O2`. Packages are published as a prerelease with per-platform provenance.

Requires macOS 12.0 or later. Packages are ad-hoc signed and not notarized; macOS 12 and native Intel hardware remain untested. All 33 regression groups, five focused Release suites, 72 affected compilation-unit checks and 92 macOS font checks passed. Limited real-account GUI checks passed, including settings navigation, translation controls and menu reordering. Scroll input was exercised without an FPS benchmark. Formatting-panel interactions, all message actions and full iOS parity remain unverified.
