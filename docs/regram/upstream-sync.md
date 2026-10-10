# 上游同步与 Qt 界面恢复

2026-10-10：将 Telegram Desktop 官方稳定版从 v7.2.9 同步到 v7.3.0（`42f8a36d43b8c805bc821905bea4cfeb3af1d41d`）。包含客户端、协议、资源、构建文件与相应固定版本的子模块。保留本地 AI 工作流说明，未改写当前分支历史。

删除自定义 Liquid Glass、Qt 玻璃布局／截图裁切／颜色变换和独立 AppKit 预览。macOS 返回 Telegram Desktop 原版 Qt 布局和窗口行为；Regram 品牌、用户指定人物图标及 Nagram／AyuGram 业务扩展保留。旧玻璃偏好作为未知键不生效，不清空用户其他设置。

合并处理了 34 处冲突，重点保留防撤回／烧毁标记、消息截图、视频自动播放、社区分组、消息过滤与自定义菜单。上游新增的完整日期标记使用独立位，避免与防撤回标记冲突；资料礼物隐藏和媒体编辑行为合并处理。

Qt 框架保持 6.11.2，最低 macOS 12。7.3.0 新增依赖按上游固定版本准备，Rust 1.96.1 的默认工具链未切换；依赖使用 dev 构建，应用只构建 Debug。

现有功能备份：`.cache/regram/preserved-features-20261010/working-features.tar.gz`（已核对 1201 个文件），另有根仓库工作区／暂存区补丁与子模块版本清单。备份不包含本地 API 密钥文件、账户数据或构建产物。

macOS arm64 Debug 整应用构建通过；隔离写入／重启读取两个进程均 PASS、退出码 0。验证版本 7.3.0、Qt 主窗口、旧玻璃偏好无效、原生预览不能再打开、Regram 设置、幽灵模式、配置未知字段／未来版本保护、SQLCipher 历史与加密快照的重启恢复及错误密钥／损坏保护。实拍及控件截图确认 Qt 布局恢复。

证据 `.cache/regram/restored-qt.mq307j2s/evidence/`，测试构建日志 `.cache/regram/upstream-730-test-build06.log`。测试入口已恢复 no-op，正常产物构建日志 `.cache/regram/qt-730-normal-build.log`。201 个功能选项仍保留；默认关闭 Material 风格开关并恢复 Qt 的弹层圆角／动画，已有非玻璃外观选项仍可选择。

测试使用虚构账户及不可用本机代理，未修改真实账户。Windows／Linux、macOS 12 实机和 M0～M8 全量功能仍需各自验收。

## 上游提交历史

发布分支 `regram` 通过正式合并接入 `telegramdesktop/tdesktop:dev` 的提交历史，合并父提交为 `28ac576967a1026ecc89a927360fa0e138d7c88c`。此前源码快照已整合 v7.3.0 和这些开发分支修复，但缺少上游祖先关系，GitHub 因此显示落后。合并保留已验证的 Regram 产品源码，避免再次应用相同变更产生重复函数，并保留本地工作流文档。Regram 自身的提交会正常显示为 ahead；上游落后数应为零。
