#!/usr/bin/env python3
"""Prepare an isolated Qt Debug archive with optimized ARM rendering primitives."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

RENDER_SOURCES = (
    "painting/qcompositionfunctions.cpp",
    "painting/qdrawhelper.cpp",
    "painting/qdrawhelper_neon.cpp",
    "painting/qgrayraster.c",
    "painting/qimagescale.cpp",
    "painting/qpaintengine_raster.cpp",
    "text/qtextengine.cpp",
    "text/qtextlayout.cpp",
    "text/qfontengine.cpp",
    "text/coretext/qfontengine_coretext.mm",
)


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def compiler_args(qt_build, source, output):
    target = f"qtbase/src/gui/CMakeFiles/Gui.dir/{source}.o"
    lines = subprocess.check_output(
        ["ninja", "-t", "commands", target], cwd=qt_build, text=True).splitlines()
    command = next(line for line in reversed(lines)
        if " -c " in line and f"/{source}" in line)
    original = shlex.split(command)
    if any(value.startswith(("-DNDEBUG", "-DQT_NO_DEBUG"))
            or value in ("&&", ";") for value in original):
        raise RuntimeError("Only an ordinary Qt Debug compile command is supported.")
    result = []
    position = 0
    while position < len(original):
        value = original[position]
        if value in ("-o", "-MT", "-MF", "-arch", "-include"):
            position += 2
        elif value == "-MD" or value.startswith(("-include", "-O")):
            position += 1
        elif value.startswith("-Xarch_"):
            position += 2
        else:
            result.append(value)
            position += 1
    result += ["-arch", "arm64", "-O2", "-g", "-o", str(output)]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-build", type=Path, required=True)
    parser.add_argument("--qt-prefix", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.qt_build.resolve()
    prefix = args.qt_prefix.resolve()
    output = args.output.resolve()
    cache = (build / "CMakeCache.txt").read_text()
    if not re.search(r"^CMAKE_BUILD_TYPE:STRING=Debug$", cache, re.MULTILINE):
        raise RuntimeError("The Qt build tree must use Debug.")
    original = prefix / "lib/libQt6Gui_debug.a"
    version_file = prefix / "lib/cmake/Qt6Gui/Qt6GuiConfigVersionImpl.cmake"
    version = re.search(r'set\(PACKAGE_VERSION "([^"]+)"\)',
        version_file.read_text()).group(1)
    architectures = subprocess.check_output(
        ["xcrun", "lipo", "-archs", str(original)], text=True).split()
    if "arm64" not in architectures:
        raise RuntimeError("The installed Qt Gui Debug archive needs arm64.")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="pixel-build.", dir=output) as temporary:
        work = Path(temporary)
        sources = []
        for relative in RENDER_SOURCES:
            name = Path(relative).name
            print(f"Compiling Debug rendering primitive: {name}", flush=True)
            obj = work / f"{name}.o"
            command = compiler_args(build, relative, obj)
            with (output / f"{name}.log").open("w") as log:
                run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT)
            source = build / "qtbase/src/gui" / relative
            sources.append({"source": str(source), "sha256": digest(source)})
        arm = work / "arm64.a"
        if len(architectures) == 1:
            import shutil
            shutil.copyfile(original, arm)
        else:
            run(["xcrun", "lipo", "-thin", "arm64", str(original),
                "-output", str(arm)])
        run(["xcrun", "ar", "-r", str(arm)]
            + [str(work / f"{Path(relative).name}.o") for relative in RENDER_SOURCES])
        run(["xcrun", "ranlib", str(arm)])
        archive = work / "libQt6Gui_debug.a"
        if len(architectures) == 1:
            os.replace(arm, archive)
        else:
            slices = [arm]
            for arch in architectures:
                if arch != "arm64":
                    part = work / f"{arch}.a"
                    run(["xcrun", "lipo", "-thin", arch, str(original),
                        "-output", str(part)])
                    slices.append(part)
            run(["xcrun", "lipo", "-create"] + list(map(str, slices))
                + ["-output", str(archive)])
        manifest = {
            "version": 1, "configuration": "Debug", "qt": version,
            "architectures": architectures, "optimized_architecture": "arm64",
            "optimized_sources": sources,
            "debug_symbols": True, "original_assertions_and_hardening": True,
            "optimization": "-O2, no fast-math",
            "original_archive_sha256": digest(original),
            "archive_sha256": digest(archive),
            "shared_qt_archive_unchanged": True,
        }
        os.replace(archive, output / archive.name)
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(output / "libQt6Gui_debug.a")


if __name__ == "__main__":
    main()
