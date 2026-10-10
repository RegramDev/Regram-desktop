#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def run(args, *, cwd=None):
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)


def link(source, target):
    if target.is_symlink():
        if target.resolve() != source.resolve():
            raise RuntimeError(f"Dependency link differs: {target}")
    elif not target.exists():
        target.symlink_to(source, target_is_directory=source.is_dir())
    else:
        raise RuntimeError(f"Refusing to replace dependency: {target}")


def copy_qt_sources(source, target):
    for module in ["", "qtbase", "qtimageformats", "qtshadertools", "qtsvg"]:
        directory = source / module
        files = subprocess.check_output(
            ["git", "-C", str(directory), "ls-files", "-z"]
        ).decode().split("\0")
        for name in filter(None, files):
            original = directory / name
            if not original.is_file():
                continue
            copied = target / module / name
            copied.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, copied)


def main():
    parser = argparse.ArgumentParser(description="Build an isolated macOS arm64 Release.")
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--api-config", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if sys.platform != "darwin" or args.jobs < 1:
        parser.error("Use macOS and a positive job count.")
    root = Path(__file__).resolve().parents[3]
    libraries = root.parent / "Libraries"
    work = (args.work_dir or root / ".local/regram/release").resolve()
    api = (args.api_config or root / ".local/regram/api.cmake").resolve()
    if not api.is_file():
        parser.error("Provide the ignored local API configuration.")
    prepare = (root / "Telegram/build/prepare/prepare.py").read_text()
    qt_config = (root / "Telegram/build/qt_version.py").read_text()
    qt = re.search(r"os.environ\['QT'\] = '([^']+)'", qt_config).group(1)
    rust = re.search(r"^rustToolchain = '([^']+)'", prepare, re.M).group(1)
    sdk = subprocess.check_output(
        ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True
    ).strip()
    os.environ["SDKROOT"] = sdk
    os.environ["MACOSX_DEPLOYMENT_TARGET"] = "12.0"
    os.environ["RUSTUP_HOME"] = str(root.parent / "ThirdParty/rust/rustup")
    os.environ["CARGO_HOME"] = str(root.parent / "ThirdParty/rust/cargo")
    os.environ["RUSTUP_TOOLCHAIN"] = rust
    os.environ["CARGO_TARGET_DIR"] = str(work / "rust-target")
    os.environ["CARGO_BUILD_JOBS"] = str(args.jobs)
    os.environ["PATH"] = os.environ["CARGO_HOME"] + "/bin:" + os.environ["PATH"]
    dependencies = work / "dependencies"
    prefix = dependencies / "local"
    prefix.mkdir(parents=True, exist_ok=True)
    for item in libraries.iterdir():
        if item.name not in {"local", "rnnoise", "tg_owt", "tde2e"}:
            link(item, dependencies / item.name)
    for item in (libraries / "local").iterdir():
        if item.name not in {"lib", f"Qt-{qt}"}:
            link(item, prefix / item.name)
    (prefix / "lib").mkdir(exist_ok=True)
    for item in (libraries / "local/lib").iterdir():
        if item.name != "libtdesktop_rust.a":
            link(item, prefix / "lib" / item.name)
    for name, build_name in [
        ("rnnoise", "rnnoise-build"), ("tg_owt", "rtc-build"), ("tde2e", "tde2e-build"),
    ]:
        directory = dependencies / name
        directory.mkdir(exist_ok=True)
        for item in (libraries / name).iterdir():
            if item.name != "out":
                link(item, directory / item.name)
        (directory / "out").mkdir(exist_ok=True)
        link(work / build_name, directory / "out/Release")
    qt_source = work / "qt-source"
    qt_build = work / "qt-build"
    if not (qt_build / "CMakeCache.txt").exists():
        copy_qt_sources(libraries / f"qt_{qt}", qt_source)
        qt_build.mkdir(parents=True, exist_ok=True)
        run([
            qt_source / "configure", "-prefix", prefix / f"Qt-{qt}",
            "-release", "-force-debug-info", "-opensource", "-confirm-license",
            "-static", "-no-framework", "-opengl", "desktop", "-no-openssl",
            "-securetransport", "-system-webp", "-I", libraries / "local/include",
            "-no-feature-futimens", "-no-feature-brotli",
            "-no-feature-cxx17_filesystem", "-platform", "macx-clang", "--",
            "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
            f"-DCMAKE_OSX_SYSROOT={sdk}", f"-DCMAKE_PREFIX_PATH={libraries / 'local'}",
            "-DQT_NO_HANDLE_APPLE_SINGLE_ARCH_CROSS_COMPILING=ON",
            "-DQT_SYNC_HEADERS_AT_CONFIGURE_TIME=ON",
            "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF",
        ], cwd=qt_build)
    run(["cmake", "--build", qt_build, "--parallel", args.jobs])
    run(["cmake", "--install", qt_build])
    common = [
        "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
        f"-DCMAKE_OSX_SYSROOT={sdk}",
    ]
    run(["cmake", "-S", libraries / "rnnoise", "-B", work / "rnnoise-build", *common])
    run(["cmake", "--build", work / "rnnoise-build", "--parallel", args.jobs])
    rtc_options = ["-DTG_OWT_BUILD_AUDIO_BACKENDS=OFF", "-DTG_OWT_SPECIAL_TARGET=mac"]
    for key, path in [
        ("LIBJPEG", "local/include"), ("OPENSSL", "openssl3/include"),
        ("OPUS", "local/include/opus"), ("LIBVPX", "local/include"),
        ("OPENH264", "local/include"), ("FFMPEG", "local/include"),
    ]:
        rtc_options.append(f"-DTG_OWT_{key}_INCLUDE_PATH={libraries / path}")
    run(["cmake", "-S", libraries / "tg_owt", "-B", work / "rtc-build",
         *common, *rtc_options])
    run(["cmake", "--build", work / "rtc-build", "--parallel", args.jobs])
    run([
        "cmake", "-S", libraries / "tde2e", "-B", work / "tde2e-build", *common,
        "-DOPENSSL_FOUND=1", f"-DOPENSSL_INCLUDE_DIR={libraries / 'openssl3/include'}",
        f"-DOPENSSL_CRYPTO_LIBRARY={libraries / 'openssl3/libcrypto.a'}",
        f"-DOPENSSL_SSL_LIBRARY={libraries / 'openssl3/libssl.a'}",
    ])
    run(["cmake", "--build", work / "tde2e-build", "--parallel", args.jobs])
    run([
        "cargo", "rustc", "--manifest-path", libraries / "tdesktop_rust/Cargo.toml",
        "--locked", "--lib", "--release", "--target", "aarch64-apple-darwin",
        "--config", "profile.release.opt-level='z'",
        "--config", "profile.release.lto='thin'",
        "--config", "profile.release.codegen-units=1",
        "--config", "profile.release.panic='unwind'",
        "--config", "profile.release.package.tlottie.opt-level=3",
    ])
    shutil.copy2(work / "rust-target/aarch64-apple-darwin/release/libtdesktop_rust.a",
                 prefix / "lib/libtdesktop_rust.a")
    os.environ.pop("QT", None)
    run([
        "cmake", "-S", root, "-B", work / "app-build", "-G", "Xcode", "-C", api,
        f"-Dqt_requested={qt}", f"-DREGRAM_DEPENDENCIES_DIR={dependencies}",
        "-DREGRAM_DEBUG_QT_GUI_LIBRARY=", "-DDESKTOP_APP_RELEASE_QT=ON",
        "-DDESKTOP_APP_TEST_APPS=OFF", "-DDESKTOP_APP_MAC_ARCH=arm64",
        "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
        f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO",
    ])
    run(["cmake", "--build", work / "app-build", "--config", "Release",
         "--target", "Telegram", "--parallel", args.jobs, "--", "-arch", "arm64"])
    print(f"Release app: {work / 'app-build/Release/Regram.app'}")


if __name__ == "__main__":
    main()
