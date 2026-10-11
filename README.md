# Regram Desktop

[English](README.md) · [简体中文](README.zh-CN.md)

<p><img src="Telegram/Resources/art/icon256.png" width="96" alt="Regram icon"></p>

Regram is an independent, cross-platform Telegram client built on Telegram Desktop. It combines features inspired by [Nagram-Qt](https://github.com/NextAlone/Nagram-Qt) and [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) under the Regram name, while keeping the original Qt interface.

## Downloads

Get available installers and checksums from [GitHub Releases](https://github.com/RegramDev/Regram-desktop/releases). Read the notes for each release before installing. Platform support is represented by the assets actually published for that release.

| Platform | Release package | Architecture |
| --- | --- | --- |
| macOS 12 or newer | Drag-to-Applications DMG | Apple Silicon arm64 / Intel x86_64 |
| Windows | Setup installer / portable ZIP | x64 |
| Linux | Compressed bundle with a user-level installer | x64 |

Builds currently use ad-hoc signing on macOS and are unsigned on Windows. They are not Apple-notarized or Authenticode-signed releases. The declared macOS 12 minimum still needs testing on a macOS 12 machine. Linux bundles follow the upstream Rocky Linux 8 build environment and require a desktop runtime.

## Features

- Keyword and regular-expression message filtering, chat-specific rules, and rule import/export.
- Local message history and anti-recall controls for messages received by the client.
- Ghost-mode controls, privacy preferences, and additional account and chat options.
- Message and draft translation, formatting protection, and configurable providers.
- Inline rich-text translation, whole-chat translation, and language exclusions.
- Additional appearance, media, notification, screenshot, and context-menu settings.
- Regram settings, settings search, and portable preference backups.

Current source consolidates Regram settings and includes iOS filter and settings backups, inline rich-text translation, nickname mentions, local user-message hiding, draggable context-menu ordering, independent chat fonts, notification policies, and experimental media scheduling. See the [iOS parity record](docs/regram/ios-parity.md) for implementation and verification status. Package manifests identify the exact source included in each installer.

Liquid Glass and the separate AppKit interface have been removed. Regram retains Telegram Desktop's Qt layout and uses independent application and data identifiers. Upstream automatic updates and crash uploads are disabled; update through Regram Releases.

Porting is ongoing. This list does **not** mean every Nagram/AyuGram feature or every M0–M8 item has passed acceptance testing. See the [feature checks](docs/regram/feature-checks.md), [feature matrix](docs/regram/regram-desktop-feature-matrix.md), and [development record](docs/regram/progress.md) for implemented paths, known differences, and test coverage.

## Building

Start with the [Regram build and packaging guide](docs/regram/building.md). This release workflow validates source and regression tests in Release before producing distribution packages. Private Telegram API credentials belong in the ignored `.local/regram/api.cmake` file or GitHub Actions secrets, never in source control.

The [Regram Release workflow](.github/workflows/regram-release.yml) builds Windows x64, Linux x64, and macOS Intel on native runners. The isolated macOS release script supports Apple Silicon and Intel through `--arch arm64` or `--arch x86_64`, using separate build directories. Installers include checksums and build manifests; account data, build logs, and local credential files are excluded.

Before a release, check the latest upstream stable release and development commits, integrate applicable source updates while retaining Regram features, then push the synchronized source and build the packages. The stable baseline is v7.3.1; the [upstream sync record](docs/regram/upstream-sync.json) identifies the additional development fixes used by the release.

## Documentation

- [Build and packaging instructions](docs/regram/building.md)
- [Release build planning and failure recovery (Chinese)](docs/regram/build-plan.zh-CN.md)
- [Feature coverage and remaining differences](docs/regram/feature-checks.md)
- [Nagram integration and performance work](docs/regram/nagram-latest.md)
- [Measured Debug UI performance](docs/regram/debug-click-performance.md)
- [Reference projects and pinned imports](docs/regram/references.md)
- [Porting plan](docs/regram/regram-desktop-plan.md)

## License and credits

Regram preserves the upstream [GPLv3 license with OpenSSL exception](LICENSE) and [third-party legal notices](LEGAL). Telegram Desktop, Nagram-Qt, AyuGram Desktop, and their contributors retain credit for their code. Regram is an independent client and is not an official Telegram product.

## Third-party

* Qt 6.11.2 ([LGPL](http://doc.qt.io/qt-6/lgpl.html)) and Qt 5.15 ([LGPL](http://doc.qt.io/qt-5/lgpl.html)) slightly patched
* OpenSSL 3 ([Apache License 2.0](https://openssl-library.org/source/license/apache-license-2.0.txt))
* WebRTC ([New BSD License](https://github.com/desktop-app/tg_owt/blob/master/LICENSE))
* zlib ([zlib License](http://www.zlib.net/zlib_license.html))
* LZMA SDK 9.20 ([public domain](http://www.7-zip.org/sdk.html))
* liblzma ([public domain](http://tukaani.org/xz/))
* Google Breakpad ([License](https://chromium.googlesource.com/breakpad/breakpad/+/master/LICENSE))
* Google Crashpad ([Apache License 2.0](https://chromium.googlesource.com/crashpad/crashpad/+/master/LICENSE))
* GYP ([BSD License](https://github.com/bnoordhuis/gyp/blob/master/LICENSE))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* OpenAL Soft ([LGPL](https://github.com/kcat/openal-soft/blob/master/COPYING))
* Opus codec ([BSD License](http://www.opus-codec.org/license/))
* FFmpeg ([LGPL](https://www.ffmpeg.org/legal.html))
* Guideline Support Library ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* Range-v3 ([Boost License](https://github.com/ericniebler/range-v3/blob/master/LICENSE.txt))
* Open Sans font ([Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html))
* Vazirmatn font ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazirmatn/blob/master/OFL.txt))
* Emoji alpha codes ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* xxHash ([BSD License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE))
* QR Code generator ([MIT License](https://github.com/nayuki/QR-Code-generator#license))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Hunspell ([LGPL](https://github.com/hunspell/hunspell/blob/master/COPYING.LESSER))
* Ada ([Apache License 2.0](https://github.com/ada-url/ada/blob/main/LICENSE-APACHE))
* zxcvbn-c ([MIT License](https://github.com/tsyrogit/zxcvbn-c/blob/master/LICENSE.txt))
* FrequencyWords by Hermit Dave ([Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/))

Message filtering follows the Regram iOS rule model and JSON format. See [usage and compatibility](docs/regram/message-filters.md). Retired Desktop-only rules no longer run.
