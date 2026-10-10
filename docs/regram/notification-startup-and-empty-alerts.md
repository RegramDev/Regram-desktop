# 通知启动与偶发空提醒

调查日期：2026-09-30。用户最终确认 `You have a new message` 是概率触发，并不限于刚安装。首次安装的数据发布窗口只是一个候选原因；账户、共享容器、数据库、解密、网络和到期回退都需要检查。没有扩展进程日志，不能确认扩展完全未启动，也不能确认每次都是系统超时。

## Nagram 的已确认实现

源码固定为 [Nagram 提交 9900847](https://github.com/NextAlone/Nagram-iOS/blob/9900847ec3251b4c8c77afc2289dc353fd3c973e/Telegram/NotificationService/Sources/NotificationService.swift)。该提交已对应 12.9.4 开发状态，与提供的 12.9.2 IPA 分别取证，不能把最新源码的每个分支都归到旧包。

12.9.2 样本的 NotificationService 可执行文件已经解密，SHA-256 为 `1fe5fafeab8d73ee2c1aa7119c18104392b45a96f261c0230ca5cbf554783f80`。样本包含六个扩展，NotificationService 的 Info 声明 `NagramNotificationFilteringEnabled=true`，签名声明通知过滤 entitlement。该声明不证明第三方重签后仍获得相同权限。

源码包含以下机制：

- 独立账户状态管理器打开失败后重试，最多三次，额外等待为 0.2 秒和 0.4 秒。
- `dismissAfterDelivery` 标记已读、已显示及同步控制消息；判定空内容时同时考虑正文、附件和发送者。
- 已 dismiss 的内容不再进入后续媒体和 inline emoji 补充路径。
- 普通消息更新 badge 时检查 dismiss 状态；读/删同步分支可以单独计算当前账户的未读数。

`NagramNotificationFilteringEnabled` 在已检查的二进制引用和当前源码中用于诊断日志。当前源码在相应生成分支清除声音，没有据此直接返回空 `UNNotificationContent`，也没有 Regram 的 passive 通知延迟清理路径。无内容完成和到期路径仍可能返回初始推送。不能据此声称 Nagram 能完全阻止所有占位提醒。

## 已发现的启动与等待问题

Regram 原来只读一次 AccountManager 账户快照。密钥尚未发布时，第一条加密推送可能找不到对应账户，随后被视为其他会话。账户数据库打开失败也没有短重试。通知 token 注册与账户通知路由数据写入原来分别启动，存在服务端已经可以发推送、扩展仍没有路由数据的时间窗口。

扩展初始化还通过没有超时的 semaphore 等待日志设置事务；这会让整个工作队列等待，发生时间不限于首次安装。原始加密载荷处理只有 Base64 解码检查；底层解密有固定范围读取，因此异常短载荷需要在调用前拒绝。这些是源码确认的等待和容错缺口，尚没有证据证明用户遇到的每次占位提醒都由它们引起。

通知路由使用 Telegram 上游的 `accountBackupData` 和 master notification key。这里的 backup 指 AccountManager 保存的账户元数据，与已删除的 Regram 会话备份功能不同；本次没有恢复那个页面或 Keychain 备份模块。

系统是否调用扩展还取决于推送格式、扩展签名与安装状态。[Apple 的通知扩展说明](https://developer.apple.com/library/archive/documentation/NetworkingInternet/Conceptual/RemoteNotificationsPG/ModifyingNotifications.html)要求 alert 和 `mutable-content=1`；没有及时调用完成回调时，系统显示原始内容。因此仅凭那句英文不能分辨未调用、启动失败、解密失败、超时或正常隐私预览。

[Apple 的 filtering entitlement 说明](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.usernotifications.filtering)要求 Apple 授权，且 `apns-push-type` 为 **alert**。空内容过滤不是任意重签包自动拥有的能力；参考 IPA 中的签名声明也不能代替安装包的实际权限。

## Regram 本次实现

1. 按 Nagram 的次数和延迟重试独立账户打开。另在共享容器解析失败、密钥匹配账户快照未就绪时采用相同的有限重试；各阶段最多增加 0.6 秒等待，不无限等待。AccountManager 的账户记录来自内存中的 atomic-state 快照，因此路由未匹配时延迟后重新打开元数据，并将刷新后的 manager 交给账户打开和未读数计算；仅对原实例重复订阅无法看到主 App 的跨进程写入。
2. 注册加密 APNs token 前，先完成通知路由元数据的 AccountManager 写入。此顺序属于针对 Regram 首次安装窗口的补充，不归为 Nagram 的已恢复实现。
3. 已读和重复消息清空后保留明确的 dismiss 标记，后续不补回媒体或 inline emoji。正常隐私占位文字、带附件或发送者的真实通知不会仅因没有普通正文被判为空。
4. 控制推送不从原始 payload 恢复 badge；两种权限路径都保留读/删同步后从当前账户计算的 badge，避免未读角标停留在旧值。过滤权限分支不填写标题、正文、附件或声音。
5. 每次接收创建独立的请求 generation，避免系统重复使用通知 identifier 时串用回调。检查归属与写入内容在同一锁内完成；完成回调一次性取得其内容、原始推送和 badge 快照，旧请求或到期后的回调不能更新新请求。新请求替换仍在处理的旧请求时，先完成旧请求的安全回退。
6. 保留 Regram 的 passive 静音及 `empty-notification` 清理兜底。有过滤权限时返回不含 alert 字段的内容，仅保留已知角标；无内容的加密推送走既有静音路径，未加密推送保留原内容。
7. 删除日志设置事务的同步等待，在账户共享快照到达后异步应用日志配置。消息或故事的 alert 成功解密后先保存可用内容，再等待网络和附件补全。增加独立队列上的 24 秒主动完成期限，与系统到期回调共用一次性完成逻辑；网络或附件迟迟未完成时使用已有解密内容，尚无内容时才走静音兜底。该期限是 Regram 补充，不归为 Nagram 已确认机制。
8. 加密载荷需包含完整的 8 字节 key id、16 字节 message key 和至少一个 16 字节密文块；密钥需满足底层切片长度。异常长度进入安全回退，不进入固定范围读取。[Telegram 推送加密说明](https://core.telegram.org/api/push-updates)、[MTProto 结构](https://core.telegram.org/mtproto/description)支持该结构校验；解密完整性校验仍由上游实现执行。

passive、主动期限和延迟清理不能保证扩展被系统强制结束后仍执行完毕。系统完全没有调用扩展的情况无法靠扩展内部重试修复。无法及时解密的真实推送也可能只能留下静音回退和角标，需要用测试账户检查该权衡。未按英文正文做一刀切删除。

通知策略放在 `Regram/RGSimpleSettings/Sources/NotificationPolicy.swift`；扩展集成在 `Telegram/NotificationService/`，注册顺序在 `SharedAccountContext.swift`。上游改动标有 `MARK: Regram`。

## 验证与真机范围

策略回归覆盖显式控制消息、附件通知、隐私正文、原始 badge 恢复限制、有限重试预算和异常载荷长度。常规默认与试验默认均通过；`//Telegram:Regram` 完整 arm64 优化构建及后续增量构建通过，最后单独重新编译了保留已计算角标的通知分支。

最终 b34577 LCSign 参考 IPA 重新解包验证通过：主 App 和六个扩展、13 个 arm64 Mach-O、ad-hoc 签名及可执行权限均符合预期；没有描述文件，Bundle ID 和 entitlements 与 b34576 一致。通知主动期限、启动失败回退及异常载荷检查标记已在通知扩展中确认。最终源码快照的 42 个 Swift/BUILD/strings 文件与构建输入一致。构建使用 Xcode 26.6（17F113）、iPhoneOS SDK 26.5，最低 iOS 13.0。

最终 IPA SHA-256：`6a7536e4e559ea6f381754da38a34ab9a36a58c67ffc376a53f518e1383bcbec`。本地构建日志、源码快照和包验证记录位于被忽略的 `build-input/notification-quality-34577/`。

本机没有连接的真机，本次未复现系统启动过程。设备验证需要检查日常使用、锁屏、前后台切换、网络变差或代理不可用时的正常新消息、隐私预览、附件、已读/删除同步、重复推送和到期回退；另外检查首次启动、登录和第一条推送。应使用测试账户区分首次安装与覆盖升级，不卸载保存真实账户的安装来做首次安装测试。记录扩展是否启动、密钥是否匹配、账户是否打开以及完成原因，不在报告中保存消息正文或密钥。
