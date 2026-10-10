# Regram Desktop

[English](README.md) · [简体中文](README.zh-CN.md)

<p><img src="Telegram/Resources/art/icon256.png" width="96" alt="Regram 图标"></p>

Regram 是基于 Telegram Desktop 的独立跨平台 Telegram 客户端。项目参考并移植 [Nagram-Qt](https://github.com/NextAlone/Nagram-Qt) 和 [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) 的功能，统一使用 Regram 品牌，保留原版 Qt 界面。

## 下载与安装

在 [GitHub Releases](https://github.com/RegramDev/Regram-desktop/releases) 下载已发布的安装包和校验文件，安装前请查看对应版本说明。某个平台的安装包实际发布后，才代表该版本提供此平台的产物。

| 平台 | 安装包形式 | 架构 |
| --- | --- | --- |
| macOS 12 及以上 | 拖动至 Applications 的 DMG | Apple Silicon arm64／Intel x86_64 |
| Windows | 安装程序／便携 ZIP | x64 |
| Linux | 带当前用户安装脚本的压缩包 | x64 |

当前 macOS 包使用 ad-hoc 签名，尚无 Apple Developer ID 分发签名和公证；Windows 包尚无 Authenticode 签名。首次启动可能出现系统安全提示。macOS 12 是最低系统声明，仍需在 macOS 12 实机验证。Linux 包沿用上游 Rocky Linux 8 构建环境，需要相应桌面运行库。

## 功能

- 关键词／正则消息过滤、指定聊天规则和规则导入导出。
- 已接收消息的本地历史记录及防撤回控制。
- 幽灵模式、隐私偏好及更多账号／聊天选项。
- 消息与草稿翻译、格式保护及可配置的翻译提供方。
- 音频转写、下载后连续转写和批量处理。
- 更多外观、媒体、通知、截图和右键菜单设置。
- Regram 设置、设置搜索及可携带的偏好备份。

新版本正在合并 Regram 设置，并加入 iOS 过滤器备份、快速翻译、本地会员显示修复、聊天独立输入设置和 iOS 字体目录。具体完成与验证情况见 [iOS 功能对齐记录](docs/regram/ios-parity.md)；这些源码改动尚未进入现有 7.3.1 安装包。

Liquid Glass 和独立 AppKit 界面已移除，使用 Telegram Desktop 原版 Qt 布局。Regram 的应用标识和数据目录与官方客户端分离。上游自动更新和崩溃上传已关闭，请通过 Regram Releases 更新。

功能移植仍在进行。上述功能列表**不代表 Nagram／AyuGram 所有功能或 M0～M8 全部条目已经验收**。实现路径、已知差异和运行验证范围见 [功能核对](docs/regram/feature-checks.md)、[功能矩阵](docs/regram/regram-desktop-feature-matrix.md) 和 [开发记录](docs/regram/progress.md)。

## 源码构建

从 [Regram 构建与打包指南](docs/regram/building.md) 开始。开发和回归使用 Debug，分发安装包使用 Release。Telegram API 凭据放在 Git 忽略的 `.local/regram/api.cmake` 或 GitHub Actions Secrets 中，不写入公共源码。

[Regram Release 工作流](.github/workflows/regram-release.yml) 在对应系统上构建 Windows x64、Linux x64 和 macOS Intel。Apple Silicon 使用独立 macOS Release 脚本。安装包提供校验值及构建清单，不携带账号数据、构建日志或本地凭据文件。

发布前检查上游最新稳定版及开发分支更新，保留 Regram 功能并整合适用的源码变更后，再推送同步后的源码并构建。当前稳定基线为 v7.3.1，额外的开发分支修复以 [上游同步记录](docs/regram/upstream-sync.json) 中的提交为准。

## 文档

- [构建与安装包制作](docs/regram/building.md)
- [功能核对与尚存差异](docs/regram/feature-checks.md)
- [Nagram 接入和性能工作](docs/regram/nagram-latest.md)
- [Debug 界面性能实测](docs/regram/debug-click-performance.md)
- [参考项目和固定版本](docs/regram/references.md)
- [功能移植计划](docs/regram/regram-desktop-plan.md)

## 许可证与致谢

项目保留上游 [GPLv3 许可证及 OpenSSL 例外](LICENSE) 和 [第三方法律声明](LEGAL)，并保留 Telegram Desktop、Nagram-Qt、AyuGram Desktop 及其贡献者的代码署名。Regram 是独立客户端，并非 Telegram 官方产品。完整第三方库及许可证链接见 [英文 README](README.md#third-party)。

消息过滤仅保留 Regram iOS 的规则模型和 JSON 格式，参见[使用与兼容说明](docs/regram/message-filters.md)。旧桌面专有过滤规则不再执行。
