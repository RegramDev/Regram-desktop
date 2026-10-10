# Regram Desktop 7.3.0 跨平台安装包

这是基于 Telegram Desktop v7.3.0，并整合发布前最新开发分支修复的 Release 构建。目标包包括 macOS Apple Silicon／Intel、Windows x64 和 Linux x64，实际可下载平台以发布资产为准。macOS 最低系统声明为 12。保留原版 Qt 界面、Regram 品牌及用户指定的默认图标，Liquid Glass 和独立 AppKit 界面已移除。自动更新和上游崩溃上传关闭。

本版包含现有 Nagram／AyuGram 移植，以及消息翻译缓存、草稿格式保护、高级配置备份、聊天规则选择和音频下载后连续转写的修复。Debug 点击卡顿的 Qt 像素循环优化保留为单独的开发构建选项；安装包直接使用优化后的 Release Qt、Rust、WebRTC 和降噪依赖。

DMG 采用拖动至 Applications 的安装方式。应用使用 `dev.regram.desktop` 和独立数据目录。安装包不含登录账号、测试覆盖层或本地 API 配置文件。

当前机器没有 Developer ID Application 分发证书，macOS 包采用 ad-hoc 签名，未经过 Apple 公证。Windows 安装程序尚无 Authenticode 签名。macOS 可能阻止首次启动，请查看“系统设置 → 隐私与安全性”并按系统提示决定是否允许。它不是 Apple 已签名／公证的发行包。

本次以 GitHub 预发布提供。已有全量源码核对及部分运行回归，但全部 M0～M8 功能尚未逐项验收；macOS 12 实机、Intel、Windows、Linux、真实外部服务、通话及系统通知仍有验证缺口。具体差异和测试范围见 [功能核对](feature-checks.md)。

构建、分发签名、公证及复现步骤见 [building.md](building.md)。发布资产附带 SHA-256 和 JSON 清单。
