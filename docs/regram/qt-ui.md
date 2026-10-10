# Regram Qt 界面

2026-10-10：按最新要求恢复 Telegram Desktop 的 Qt 界面。移除 Liquid Glass 开关、材质、颜色变换、专用布局／裁切以及独立 AppKit 预览、导航和快捷键拦截。不存在原生预览环境变量入口。macOS 系统窗口控制继续由原版 Qt 窗口管理，最低系统要求保持 macOS 12。

Regram 品牌、人物图标、本地 API 配置及已移植的 Nagram／AyuGram 业务功能保留。已有玻璃偏好作为未知键不生效，不清空账户或其他设置。

同步的上游版本与验收记录见 [上游同步](upstream-sync.md)。此次同步不表示 M0～M8 全量功能移植已经验收完成。

当前新增功能和公共 Qt 绘制性能记录见 [Nagram 最新接入](nagram-latest.md)。
