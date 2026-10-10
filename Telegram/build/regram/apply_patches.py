#!/usr/bin/env python3

import argparse
import json
from pathlib import Path
import subprocess
import sys


def git(module, *arguments):
    return subprocess.run(
        ["git", "-C", str(module), *arguments],
        capture_output=True,
        text=True,
        check=False,
    )


def prepare(root, manifest):
    pending = []
    for entry in manifest["patches"]:
        module = root / entry["module"]
        patch = root / entry["patch"]
        head = git(module, "rev-parse", "HEAD")
        if head.returncode or head.stdout.strip() != entry["base"]:
            raise RuntimeError(f"{entry['module']}: expected revision {entry['base']}")
        if not patch.is_file():
            raise RuntimeError(f"Missing patch: {entry['patch']}")
        applied = git(module, "apply", "--reverse", "--check", str(patch))
        if applied.returncode == 0:
            print(f"{entry['module']}: patch already applied")
            continue
        applicable = git(module, "apply", "--check", str(patch))
        if applicable.returncode:
            raise RuntimeError(
                f"{entry['module']}: patch conflicts with local changes; "
                f"files preserved\n{applicable.stderr.strip()}"
            )
        pending.append((module, patch))
    return pending


def main():
    parser = argparse.ArgumentParser(description="Install Regram library extensions")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    manifest_path = root / "Telegram/build/regram/patches/manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text())
        if manifest["version"] != 1:
            raise RuntimeError("Unsupported library patch manifest version")
        pending = prepare(root, manifest)
        if not arguments.apply:
            if pending:
                raise RuntimeError("Library extensions missing; rerun with --apply")
            return 0
        for module, patch in pending:
            result = git(module, "apply", str(patch))
            if result.returncode:
                raise RuntimeError(result.stderr.strip())
            print(f"{module.relative_to(root)}: patch applied")
        return 0
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"Regram library setup: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
