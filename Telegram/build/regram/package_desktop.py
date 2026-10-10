#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description="Package Windows or Linux x64 Release binaries.")
    parser.add_argument("--platform", choices=["windows", "linux"], required=True)
    parser.add_argument("--build-dir", type=Path, default=Path("out"))
    parser.add_argument("--output-dir", type=Path, default=Path(".local/regram/installers"))
    parser.add_argument("--iscc", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    cache = (build / "CMakeCache.txt").read_text()
    if "DESKTOP_APP_TEST_APPS:BOOL=OFF" not in cache:
        parser.error("Disable test applications before packaging.")
    version = re.search(r"^AppVersionStr\s+(\S+)",
                        (root / "Telegram/build/version").read_text(), re.M).group(1)
    binary = build / "Release" / ("Regram.exe" if args.platform == "windows" else "Regram")
    if not binary.is_file():
        parser.error("Build the Regram Release executable first.")
    if args.platform == "windows":
        with binary.open("rb") as handle:
            if handle.read(2) != b"MZ":
                parser.error("Expected a Windows PE executable.")
            handle.seek(0x3C)
            pe_offset = int.from_bytes(handle.read(4), "little")
            handle.seek(pe_offset)
            if handle.read(6) != b"PE\0\0\x64\x86":
                parser.error("Expected a Windows x64 executable.")
        if not args.iscc or not args.iscc.is_file():
            parser.error("Provide the installed Inno Setup compiler.")
    else:
        with binary.open("rb") as handle:
            header = handle.read(20)
        if header[:5] != b"\x7fELF\x02" or header[18:20] != b"\x3e\x00":
            parser.error("Expected a Linux x64 ELF executable.")
    stem = f"Regram-{version}-{'Windows' if args.platform == 'windows' else 'Linux'}-x64"
    output.mkdir(parents=True, exist_ok=True)
    if any(output.glob(f"{stem}*")):
        parser.error("Refusing to replace existing release artifacts.")
    artifacts = []
    with tempfile.TemporaryDirectory(prefix="regram-package-") as temporary:
        staging = Path(temporary) / "Regram"
        staging.mkdir()
        shutil.copy2(binary, staging / binary.name)
        for name in ["LICENSE", "LEGAL"]:
            shutil.copy2(root / name, staging / name)
        if args.platform == "windows":
            modules = staging / "modules/x64/d3d"
            modules.mkdir(parents=True)
            shutil.copy2(root / "cmake/win_directx_helper/modules/x64/d3d/d3dcompiler_47.dll",
                         modules / "d3dcompiler_47.dll")
            portable = output / f"{stem}-portable.zip"
            with zipfile.ZipFile(portable, "w", zipfile.ZIP_DEFLATED) as archive:
                for path in staging.rglob("*"):
                    if path.is_file():
                        archive.write(path, path.relative_to(staging.parent))
            subprocess.run([
                str(args.iscc), f"/DMyAppVersion={version}", f"/DMyAppVersionFull={version}",
                "/DMyBuildTarget=win64", f"/DReleasePath={staging}",
                f"/DSourcePath={root / 'Telegram/build'}\\", "/DDisableSigning",
                f"/DMyOutputBaseFilename={stem}-setup", f"/O{output}",
                str(root / "Telegram/build/setup.iss"),
            ], check=True)
            artifacts = [portable, output / f"{stem}-setup.exe"]
        else:
            subprocess.run(["strip", "--strip-unneeded", str(staging / binary.name)], check=True)
            shutil.copy2(root / "lib/xdg/dev.regram.desktop.desktop", staging / "dev.regram.desktop.desktop")
            shutil.copy2(root / "Telegram/Resources/art/icon256.png", staging / "dev.regram.desktop.png")
            (staging / "install.sh").write_text(
                '#!/bin/sh\nset -eu\n'
                'source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                'install_home=${REGRAM_INSTALL_HOME:-"$HOME"}\n'
                'data_dir=${XDG_DATA_HOME:-"$install_home/.local/share"}\n'
                'target_dir="$data_dir/regram"\n'
                'mkdir -p "$target_dir" "$install_home/.local/bin" "$data_dir/applications" '
                '"$data_dir/icons/hicolor/256x256/apps"\n'
                'install -m 755 "$source_dir/Regram" "$target_dir/Regram"\n'
                'install -m 644 "$source_dir/LICENSE" "$source_dir/LEGAL" "$target_dir/"\n'
                'ln -sfn "$target_dir/Regram" "$install_home/.local/bin/Regram"\n'
                "escaped_dir=$(printf '%s' \"$target_dir\" | sed 's/[\\\\&|]/\\\\&/g')\n"
                'sed "s|^Exec=Regram -- %U$|Exec=\\\"$escaped_dir/Regram\\\" -- %U|; '
                's|^Exec=Regram -quit$|Exec=\\\"$escaped_dir/Regram\\\" -quit|; '
                's|^TryExec=.*|TryExec=$escaped_dir/Regram|; '
                's|^DBusActivatable=.*|DBusActivatable=false|" '
                '"$source_dir/dev.regram.desktop.desktop" > "$data_dir/applications/dev.regram.desktop.desktop"\n'
                'install -m 644 "$source_dir/dev.regram.desktop.png" '
                '"$data_dir/icons/hicolor/256x256/apps/dev.regram.desktop.png"\n'
                'printf "Installed Regram to %s\\n" "$target_dir"\n'
            )
            (staging / "install.sh").chmod(0o755)
            (staging / "Install.txt").write_text(
                "Run ./install.sh to install for the current user, or launch ./Regram directly.\n"
                "Source: https://github.com/RegramDev/Regram-desktop\n"
            )
            archive = output / f"{stem}.tar.xz"
            with tarfile.open(archive, "w:xz") as handle:
                handle.add(staging, arcname="Regram")
            artifacts = [archive]
    provenance = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    files = []
    for artifact in artifacts:
        with artifact.open("rb") as handle:
            digest = hashlib.file_digest(handle, "sha256").hexdigest()
        files.append({"name": artifact.name, "sha256": digest, "bytes": artifact.stat().st_size})
    (output / f"{stem}.sha256").write_text("".join(f"{item['sha256']}  {item['name']}\n" for item in files))
    (output / f"{stem}.json").write_text(json.dumps({
        "product": "Regram Desktop", "version": version, "configuration": "Release",
        "platform": args.platform, "architecture": "x64", "source_commit": provenance,
        "signature": "unsigned", "artifacts": files,
    }, indent=2) + "\n")
    print("Packaged:", ", ".join(path.name for path in artifacts))


if __name__ == "__main__":
    main()
