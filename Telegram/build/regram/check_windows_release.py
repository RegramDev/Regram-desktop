#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description="Check Windows distribution link settings before a full build.")
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    values = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.split(":", 1)[0]] = value
    expected = {
        "CMAKE_GENERATOR": "Ninja Multi-Config",
        "CMAKE_MSVC_DEBUG_INFORMATION_FORMAT": "",
        "DESKTOP_APP_SPECIAL_TARGET": "win64",
        "DESKTOP_APP_TEST_APPS": "OFF",
    }
    if any(values.get(key) != value for key, value in expected.items()):
        parser.error("Expected a Windows x64 distribution configuration without full debug symbols.")
    if not re.search(r"(?:^|\s)/O2(?:\s|$)", values.get("CMAKE_CXX_FLAGS_RELEASE", "")):
        parser.error("Release optimization is missing.")
    commands = subprocess.run([
        "ninja", "-C", str(build), "-f", "build-Release.ninja",
        "-t", "commands", "Release/Regram.exe",
    ], check=True, capture_output=True, text=True).stdout.splitlines()
    links = [line for line in commands if "vs_link_exe" in line and "Regram.exe" in line]
    if len(links) != 1:
        parser.error("Expected one generated Regram executable link command.")
    link = links[0]
    if not re.search(r"(?:^|\s)/DEBUG:NONE(?:\s|$)", link, re.I):
        parser.error("Distribution linking must skip full PDB generation.")
    if re.search(r"(?:^|\s)/DEBUG(?::(?:FULL|FASTLINK))?(?:\s|$)", link, re.I):
        parser.error("Full PDB generation is still enabled.")
    if not re.search(r"Hostx64[/\\]x64[/\\]link\.exe", link, re.I):
        parser.error("Expected the native x64 MSVC linker.")
    print("Windows x64 Release preflight passed: /O2, native x64 linker, /DEBUG:NONE.")


if __name__ == "__main__":
    main()
