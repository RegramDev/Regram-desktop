#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile


def run(args, *, capture=False):
    result = subprocess.run([str(arg) for arg in args], check=True,
                            stdout=subprocess.PIPE if capture else None,
                            text=capture)
    return result.stdout if capture else None


def main():
    parser = argparse.ArgumentParser(description="Package a macOS Regram Release as DMG.")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--identity", help="Developer ID Application signing identity")
    parser.add_argument("--notary-profile", help="Stored notarytool keychain profile")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("macOS packaging tools are required.")
    if args.notary_profile and not args.identity:
        parser.error("Notarization requires a Developer ID Application identity.")
    if args.identity and not args.identity.startswith("Developer ID Application:"):
        parser.error("Use a Developer ID Application identity for macOS distribution.")
    root = Path(__file__).resolve().parents[3]
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    app = build / "Release/Regram.app"
    cache = (build / "CMakeCache.txt").read_text()
    if "DESKTOP_APP_TEST_APPS:BOOL=OFF" not in cache:
        parser.error("Configure the Release build with test applications disabled.")
    if not app.is_dir():
        parser.error("Build Release/Regram.app first; Debug bundles are not accepted.")
    with (app / "Contents/Info.plist").open("rb") as handle:
        info = plistlib.load(handle)
    if info.get("CFBundleIdentifier") != "dev.regram.desktop":
        parser.error("The bundle must use the production Regram identifier.")
    if info.get("LSMinimumSystemVersion") != "12.0":
        parser.error("The bundle must declare the supported macOS 12 deployment target.")
    version = info["CFBundleShortVersionString"]
    if not re.fullmatch(r"[0-9.]+", version):
        parser.error("Unexpected bundle version.")
    executable = app / "Contents/MacOS/Regram"
    architectures = run(["lipo", "-archs", executable], capture=True).split()
    architecture = {("arm64",): "arm64", ("x86_64",): "x86_64"}.get(
        tuple(architectures), "universal" if set(architectures) == {"arm64", "x86_64"} else None)
    if architecture is None:
        parser.error("Unexpected executable architectures.")
    for dependency in run(["otool", "-L", executable], capture=True).splitlines()[1:]:
        path = dependency.strip().split(" (", 1)[0]
        if path and not path.endswith(":") and not path.startswith(("/System/Library/", "/usr/lib/")):
            parser.error(f"Unbundled dynamic dependency: {path}")
    forbidden = {"tdata", "DebugLogs", "TelegramForcePortable", "RegramForcePortable"}
    if any(path.name in forbidden for path in app.rglob("*")):
        parser.error("Private profile data must never enter the application bundle.")
    stem = f"Regram-{version}-macOS-{architecture}"
    output.mkdir(parents=True, exist_ok=True)
    dmg = output / f"{stem}.dmg"
    if dmg.exists():
        parser.error(f"Refusing to replace an existing installer: {dmg}")
    with tempfile.TemporaryDirectory(prefix="regram-package-") as temporary:
        staging = Path(temporary)
        packaged = staging / "Regram.app"
        run(["ditto", "--noqtn", app, packaged])
        run(["xattr", "-cr", packaged])
        run(["strip", "-S", "-x", packaged / "Contents/MacOS/Regram"])
        signing = ["codesign", "--force", "--sign", args.identity or "-",
                   "--options", "runtime", "--entitlements",
                   root / "Telegram/Telegram/Telegram.entitlements"]
        if args.identity:
            signing.append("--timestamp")
        run([*signing, packaged])
        run(["codesign", "--verify", "--strict", "--deep", packaged])
        (staging / "Applications").symlink_to("/Applications", target_is_directory=True)
        shutil.copy2(root / "LICENSE", staging / "LICENSE")
        shutil.copy2(root / "LEGAL", staging / "LEGAL")
        instructions = (
            f"Regram Desktop {version}\n\n"
            "Drag Regram.app to Applications, then launch it from Applications.\n"
            "Requires macOS 12 or newer. This package is for " + architecture + ".\n"
            "Source: https://github.com/RegramDev/Regram-desktop\n"
        )
        if not args.notary_profile:
            instructions += (
                "\nThis build is not Apple-notarized. macOS may block its first launch.\n"
                "Review the release source, then use Privacy & Security > Open Anyway\n"
                "if you choose to allow it.\n"
            )
        (staging / "Install.txt").write_text(instructions)
        run(["hdiutil", "create", "-volname", f"Regram {version}", "-srcfolder", staging,
             "-format", "UDZO", "-fs", "HFS+", dmg])
    if args.identity:
        run(["codesign", "--sign", args.identity, "--timestamp", dmg])
    if args.notary_profile:
        result = json.loads(run([
            "xcrun", "notarytool", "submit", dmg, "--keychain-profile",
            args.notary_profile, "--wait", "--output-format", "json",
        ], capture=True))
        if result.get("status") != "Accepted":
            raise RuntimeError("Apple notarization was not accepted.")
        run(["xcrun", "stapler", "staple", dmg])
        run(["xcrun", "stapler", "validate", dmg])
    run(["hdiutil", "verify", dmg])
    with dmg.open("rb") as handle:
        checksum = hashlib.file_digest(handle, "sha256").hexdigest()
    (output / f"{stem}.sha256").write_text(f"{checksum}  {dmg.name}\n")
    manifest = {
        "product": "Regram Desktop", "version": version, "configuration": "Release",
        "architecture": architecture, "minimum_macos": "12.0",
        "macos_12_runtime_verified": False,
        "bundle_identifier": info["CFBundleIdentifier"],
        "signature": "Developer ID Application" if args.identity else "ad-hoc",
        "notarized": bool(args.notary_profile), "sha256": checksum,
        "bytes": dmg.stat().st_size,
        "source_commit": run(["git", "-C", root, "rev-parse", "HEAD"], capture=True).strip(),
        "working_tree_dirty": bool(run([
            "git", "-C", root, "status", "--porcelain", "--ignore-submodules=dirty",
        ], capture=True)),
        "submodule_patches": json.loads(
            (root / "Telegram/build/regram/patches/manifest.json").read_text()),
    }
    (output / f"{stem}.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
    print(f"Installer: {dmg}")


if __name__ == "__main__":
    main()
