# Debug 点击卡顿修复

2026-10-10。针对登录账号后打开设置、菜单与弹层非常卡的反馈，使用真实登录资料的独立副本复现：312 个聊天、1710×979 逻辑窗口、DPR 2、macOS Apple Silicon、Qt 6.11.2 Debug。原始 940 个账号数据文件在测试前后逐一计算 SHA-256，内容没有改变。副本先在网络阻断环境启用不发送已读／动态回执及在线状态的设置，再联网加载聊天数据，仅执行菜单、设置和弹层导航。

实际 Qt 输入／事件循环测量发现：输入处理大多几十毫秒，随后一次重绘却可占用主线程 200～350 ms。QRhi 与旧后端对比均复现，切换后端不能解决。独立混合测试确认本机 Qt Debug 的未优化像素循环是主要开销：2400×1700 图像的 30 组背景混合从 7793.7 ms 降至 301.2 ms。四种像素格式、从 SourceOver 到 Exclusion 的组合绘制 SHA-256 完全相同。

保留 Debug 配置、调试符号、原有 Qt 断言和 libc++ 检查，仅优化 ARM64 的三份纯像素计算源文件：`qcompositionfunctions.cpp`、`qdrawhelper.cpp`、`qdrawhelper_neon.cpp`。应用业务代码继续使用普通 Debug 编译。没有构建 Release、修改 Qt 版本、删减动画或更换界面；共享的 Qt 安装与源码不改动。准备出的独立通用静态库保留原来的 x86_64 部分。

真实数据的相同场景对照，每种操作连续测试三次：

| 操作 | 输入／动作处理中位数，前→后 | 最长重绘，前→后 | 最长事件循环间隔，前→后 |
| --- | --- | --- | --- |
| 主菜单 | 59.3→32.5 ms | 353.9→27.5 ms | 468.0→52.4 ms |
| 原版设置入口 | 40.7→21.3 ms | 234.4→25.4 ms | 285.0→50.9 ms |
| 关闭原版设置 | 15.5→7.8 ms | 295.3→27.6 ms | 234.7→35.4 ms |
| Regram 设置 | 16.0→10.6 ms | 214.2→31.0 ms | 218.8→76.3 ms |
| 外观页 | 31.7→17.6 ms | 126.4→21.7 ms | 130.6→41.5 ms |
| 圆角弹层 | 18.1→10.1 ms | 305.0→22.9 ms | 278.7→44.9 ms |

第二次无构建负载的冷启动重复运行也通过，共 54 次优化后的真实数据导航检查。第一次打开原版设置约 48.9 ms，后两次约 31～32 ms；该轮设置重绘最大约 21 ms，事件循环间隔最大约 35 ms。一轮与最终构建重叠的测试出现首次创建 1.37 s 的异常，保留证据但不作为空闲机器的对照；后台构建与首次加载仍会影响响应，不能把上述数字当作所有机器／全部场景的保证。

完整测量见 [debug-click-performance.json](debug-click-performance.json)。普通启动入口已恢复 no-op。最终程序仍为 `out/Debug/Regram.app`，版本 7.3.0，最低系统声明 macOS 12。测试中没有发送消息、退出账号或删除原始数据。

## 重建相同 Debug 图形依赖

只对已经配置为 Debug 的本机 Qt 构建树执行：

```sh
python3 Telegram/build/regram/prepare_debug_qt.py \
  --qt-build ../Libraries/qt_6.11.2 \
  --qt-prefix ../Libraries/local/Qt-6.11.2 \
  --output .local/regram/qt-debug-pixels

env -u QT cmake -S . -B out -C .local/regram/api.cmake \
  -DREGRAM_DEBUG_QT_GUI_LIBRARY="$PWD/.local/regram/qt-debug-pixels/libQt6Gui_debug.a"
cmake --build out --config Debug --target Telegram --parallel 4
```

Qt 目录以实际安装版本为准。CMake 检查版本、Debug 标记、原始／新库校验和，并同时替换标准 Qt 导入目标和项目自己的静态 Qt 链接路径，防止准备了优化库却仍链接旧库。清空 `REGRAM_DEBUG_QT_GUI_LIBRARY` 可恢复普通 Debug Qt。像素校验与混合探针位于 `regram/tests/debug_graphics_probe.cpp`，需启用独立测试工程的 `REGRAM_TEST_DEBUG_GRAPHICS`。
