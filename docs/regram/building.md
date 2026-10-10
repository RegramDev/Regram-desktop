# Regram Desktop 构建与验证

当前源码基线为 Telegram Desktop v7.3.0，分支 `regram/port`。保留上游 CMake 目标 `Telegram`，Regram 可执行文件输出名为 `Regram`。开发及回归使用 Debug；制作用户明确请求的安装包时使用独立的 Release 构建。

本地凭据在 Git 忽略的 `.local/regram/api.cmake` 中。新机器复制 `Telegram/build/regram/api.cmake.example` 到该位置后自行填写；CI 使用私有变量传入 `TDESKTOP_API_ID`、`TDESKTOP_API_HASH`。不要把实际 hash 写入公共配置或命令示例。

macOS 使用 `docs/building-mac.md` 的官方依赖准备路径。完整 Xcode 通过当前进程的 `DEVELOPER_DIR` 使用；不要修改系统默认 Xcode。若默认 Command Line Tools 的 SDK 与选定 Xcode 不一致，同时设置 `SDKROOT` 为该 Xcode 的 macOS SDK 路径。

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
git submodule update --init --recursive
python3 Telegram/build/regram/apply_patches.py --apply
Telegram/build/prepare/mac.sh skip-release silent
```

AyuGram 的 codegen、lib_ui、lib_tl 扩展由 `Telegram/build/regram/patches/manifest.json` 固定版本并保存补丁。安装脚本先检查全部补丁，冲突时保留本地文件；重复执行会识别已安装的扩展。CMake 配置也检查扩展是否完整。

首次配置前查看 `../Libraries/local/Qt-*`，使用实际准备好的版本。下面的 6.11.2 是本基线准备脚本的版本；目录未生成前不能配置整应用。

```sh
QT=6.11.2 cmake -S . -B out -G Xcode \
  -C .local/regram/api.cmake \
  -D CMAKE_OSX_SYSROOT="$SDKROOT"
cmake --build out --config Debug --target Telegram
```

已有 `out/` 树重新配置时保留缓存的 Qt 版本，不导出 `QT`：

```sh
env -u QT cmake -S . -B out -C .local/regram/api.cmake
```

产物为 `out/Debug/Regram.app`。便携目录为 `RegramForcePortable`；正常用户目录和官方 Telegram 分离。首次运行不会主动把 Telegram 链接的默认处理程序改为 Regram。

## macOS Release 安装包

先按前述路径准备源码依赖及本地 API 配置。下面的脚本在 Git 忽略的 `.local/regram/release/` 内复制已打补丁的 Qt 源码、编译 arm64 Release Qt、Rust、WebRTC 与降噪库，并建立独立依赖目录及 Xcode 构建树。它保留现有 Debug Qt、构建产物和账号目录。Qt 使用带符号的优化配置；Regram 应用使用 Release。首次编译较耗时，后续增量执行。

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
python3 Telegram/build/regram/build_macos_release.py --jobs 4
python3 Telegram/build/regram/package_macos.py \
  --build-dir .local/regram/release/app-build \
  --output-dir .local/regram/installers
```

打包脚本只接收 `Release/Regram.app` 和生产标识 `dev.regram.desktop`，拒绝 Debug／启用测试应用的构建、外部动态库以及包内账号数据。DMG 包含应用、Applications 快捷入口、安装说明、LICENSE 和 LEGAL，并生成 SHA-256 及机器可读发布清单。构建目录、凭据、日志和账号不上传。

默认只做 ad-hoc 签名，**不等于 Apple Developer ID 分发签名或公证**。持有合适证书时可显式提供 `--identity 'Developer ID Application: …'`；提供 `--notary-profile <已存储的钥匙串配置名>` 后脚本等待 Apple 接受公证并装订票据。未公证的下载包可能被 Gatekeeper 阻止，需按安装说明查看系统“隐私与安全性”提示。

当前包面向 Apple Silicon，最低声明 macOS 12；macOS 12 实机和 Intel 包需分别验证。完整功能移植的验收边界见 [feature-checks.md](feature-checks.md)，不能用安装包生成成功替代这些测试。

Windows x64 使用 `docs/building-win.md`，在 x64 Native Tools Command Prompt 中准备依赖并配置，再执行 `cmake --build out --config Debug --target Telegram`。本轮尚未在 Windows 执行构建或运行。

Linux x64 使用 `docs/building-linux.md` 或 `Telegram/build/docker/centos_env/build_debug.sh`，不得把 Docker/Linux 树交给原生 Windows CMake 构建。本轮尚未在 Linux 执行构建或运行。

策略层可在整应用依赖准备期间独立验证。需要 C++20、CMake 和 ICU 64 以上；本机验证使用 ICU 78.3。测试没有真实消息、账户凭据或联网请求。

```sh
cmake -S Telegram/SourceFiles/regram/tests -B .cache/regram/policy-tests \
  -G Ninja -D CMAKE_BUILD_TYPE=Debug -D ICU_ROOT=/opt/homebrew/opt/icu4c \
  -D CMAKE_CXX_FLAGS='-Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build .cache/regram/policy-tests
ctest --test-dir .cache/regram/policy-tests --output-on-failure
```

ICU 策略与导入导出已经接入整应用过滤入口，复用 Ayu 的静态 ICU 子模块。独立测试可选 `REGRAM_TEST_JSON=ON` 和 `REGRAM_TEST_SQLCIPHER=ON`；后者需 OpenSSL。应用实际过滤与存储行为仍需运行验证。Foundation 对照仅验证样本在本机框架上的结果，不能代替 iOS 实机或完整规则兼容验收。
