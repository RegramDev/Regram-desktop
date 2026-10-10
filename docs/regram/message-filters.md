# 消息过滤 / Message filters

参考 Regram iOS `a5c80b6d5ff8b995cd6e4d5737a058dd894e3293` 的 `MessageFilter.swift` 和 `RGContentFilter.swift`。桌面端保留 Qt 界面，规则和文件格式一致。

进入 **设置 → Regram 设置 → 规则 → 消息过滤**，添加关键词或正则表达式。匹配不区分大小写，作用于收到的消息原文；发送的消息保持显示。搜索和共享媒体保留原文。每条规则可停用、编辑、删除，范围可选全部聊天或指定聊天。**保留规则优先于隐藏规则**，无效正则或无法识别的范围不会隐藏消息。

消息或选中文字的右键菜单可“添加到消息过滤”，打开编辑器确认后保存。从用户资料或消息右键菜单可隐藏用户或机器人；“隐藏的用户”列表可解除隐藏。只匹配实际发送者，不匹配转发来源、机器人署名、表情回应或正在输入状态。聊天内停用关键词过滤不会解除隐藏用户。

导出固定为 `regram-message-filter.json`，可在 Regram iOS 中导入。导入合并已有规则，以关键词、类型、保留动作和聊天范围去重，保留启用状态。聊天中的同名或编号副本 JSON 文件可右键“添加到消息过滤”；未下载的文件先下载，解析及确认后才添加。不会因收到文件而自动启用规则。

“临时显示过滤消息”只恢复当前账号、当前聊天的显示，五分钟后恢复过滤。“过滤消息通知”默认关闭，临时显示不改变它的判断。规则文件不包含隐藏用户、通知开关或聊天停用列表，也不自动同步设备。

已移除“共享过滤”、遮盖／替换、反向匹配、跨账号全局规则、话题规则／继承和单条消息隐藏。旧配置及历史数据库保留，不继续执行旧过滤系统。若升级前关闭了过滤总开关，会将已有规则迁移为逐条停用，避免升级后突然隐藏消息。本机旧 Desktop v1/v2 存储中受支持的聊天规则仍可读取，话题与未知范围保持停用；公开导入不再接受 Desktop 专用格式。导出前需删除或重新选择这些无法表示的范围，避免丢失或扩大规则。

## JSON

```json
[
  {
    "id": "6e8ce87c-33cd-4f25-af3e-d2a58e4e911c",
    "pattern": "广告",
    "isRegex": false,
    "peerIds": [],
    "isEnabled": true,
    "isException": false
  }
]
```

`peerIds` 是 iOS 编码的整数聊天 ID，空数组表示所有聊天；请用界面选取聊天，不手工填 Telegram 显示 ID。旧四字段 iOS 文件仍可导入，缺少的开关默认启用、缺少的保留标记默认关闭。

## English

Open **Settings → Regram Settings → Rules → Message filters**. Add a case-insensitive keyword or regex, choose all chats or specific chats, and optionally mark it as a keep rule. Keep rules win regardless of order. Rules match incoming original message text. Invalid patterns and unsupported scopes stay inactive.

Import and export use the same JSON array as Regram iOS and the filename `regram-message-filter.json`. Imports merge and deduplicate without replacing existing rules, preserving each rule's enabled state. Right-click an exported JSON attachment to add its rules after downloading, validation and confirmation. Selected message text can also open a new rule editor.

Hidden users are matched only against the actual sender, including bots. They remain hidden when keyword filtering is disabled for a chat. Use the hidden-user list to restore them. Temporary reveal lasts five minutes and affects display only; optional notification filtering defaults to off. Rule exports exclude hidden users and local switches.

Shared regex filters, content replacement/masking, reversed matching, global/topic inheritance and individual-message hiding have been retired. Stored legacy data is preserved but the retired engines no longer execute. Public import/export accepts only the iOS format; internal legacy topic scopes remain disabled until deliberately removed or reassigned.

Verification evidence is recorded separately in [ios-parity.md](ios-parity.md); source integration alone does not prove every GUI interaction or platform runtime.
