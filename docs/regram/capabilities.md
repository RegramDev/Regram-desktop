# M0 上游富文本与平台能力初查

本表核对固定 v7.2.9 源码及固定子模块，属于接入导航，不是功能通过记录。所有 RD 条目仍按 `progress.md` 分平台验收。

| 条目 | 已定位的基础 | 尚需验收的差异 |
| --- | --- | --- |
| RD-R01、R02 | `iv/editor/`、`iv_editor_page_blocks.cpp`、`iv_editor_page_list.cpp` | 编辑器与聊天展示、展开／折叠、标题、代码、列表及跨端发送／编辑 |
| RD-R03 | `iv_editor_page_table_grid.cpp` 有 rowspan、colspan 的限制、占位和网格处理 | iOS 合并单元格、表头、高亮样本，窄窗口编辑与复制 |
| RD-R04 | 上游文本实体与编辑器已有链接、表情及拼写相关接口 | UTF-16 偏移、组合表情、粘贴、撤销与平台拼写服务 |
| RD-R05、R10 | `iv_editor_page_media.cpp`、富文本媒体结构及媒体查看器 | 单项剧透、组合布局、枚举与共享媒体、下载及返回路径 |
| RD-R06 | `iv_rich_message_serializer.cpp`、HTML 导入导出、`iv/markdown/` | 重启草稿、转换边界与结构丢失提示 |
| RD-R07、R08 | 富文本页面与块模型及现有媒体渲染 | 公式、嵌套引用、锚点、长内容、流式与思考块的真实样本 |
| RD-R09 | `api/api_rich_tasks.cpp` | 权限、失败回退、快速重复点击、跨端状态 |
| RD-R11 | 富文本序列化与页面模型 | 统一可检索文本提取，原文／译文区分，过滤及保留结构的完整性 |

| 能力 | macOS 源码路径／条件 | Windows 源码路径／条件 | Linux／其他 Unix 源码路径／条件 |
| --- | --- | --- | --- |
| 系统翻译 | `lib_translate/translate_provider_mac_swift.swift`；当前实现需 Swift 6.2 以上、macOS 26 以上及已安装语言包 | `translate_provider_win.h` 在所选基线返回不可用；M4 须提供独立后端 | `translate_provider_linux.cpp` 调用 Crow Translate，需安装该程序；M4 须明确缺失时的后备方式 |
| 通知 | `notifications_manager_mac_un.mm` 等；需实测权限、锁屏与合并行为 | `notifications_manager_win.cpp`；Regram 通知激活类与 AppUserModelID 已独立，但尚未运行验证 | `notifications_manager_linux.cpp` 与生成的 D-Bus 通知绑定；需实测系统服务和沙箱 |
| 网页承载 | `lib_webview` 与原生后端 | `lib_webview` 与平台后端 | `lib_webview`、WebKitGTK 与沙箱路径 |
| 图标 | macOS bundle 和托盘资源；替代图标及 Dock 行为待 M3 | 窗口、安装包、快捷方式与托盘资源待 M3 | 桌面文件、主题图标与托盘资源待 M3 |
| 摄像头／麦克风 | 继承 calls / tgcalls / WebRTC 设备能力 | 同左 | 同左 |
| Force TCP | 需在 `mtproto/` 的连接选择与重连路径接入并验证 | 同左 | 同左 |
| 加密本地保留 | 已定位 `storage/details/storage_file_utilities.*` 的 `PrepareEncrypted` / `DecryptLocal`，原型接入 `regram/archive/snapshot.*` | 使用同一账户本地存储能力，待构建与运行 | 使用同一账户本地存储能力，待构建与运行 |

系统翻译的源码可用性不保证当前设备已下载语言包或能执行翻译；M4 必须通过真实请求验证。Windows 的原生翻译缺口只影响该后端，不排除翻译功能。网页、设备和网络能力也不能仅凭文件存在就标记支持。

本地保留原型使用现有本地加密载荷接口，追加独立的版本化账户载荷，并通过 Qt 原子文件提交保存。损坏、未来版本、账户不匹配时保持原文件。当前原型验证范围是虚构内容的保存、重启读取和清除；尚未接入撤回／编辑／TTL 事件，索引、容量策略、媒体留存和账户退出流程由 M5 完成。
