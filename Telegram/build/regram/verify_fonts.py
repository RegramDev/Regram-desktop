#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request


def main():
    parser = argparse.ArgumentParser(description="Verify the iOS catalog in native Release Qt.")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--fixtures", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    catalog = json.loads((root / "Telegram/Resources/regram/fonts/catalog.json").read_text())
    args.fixtures.mkdir(parents=True, exist_ok=True)
    for entry in catalog["files"]:
        path = args.fixtures / entry["file"]
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            with urllib.request.urlopen(entry["url"], timeout=60) as response:
                data = response.read(entry["bytes"] + 1)
            if len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise RuntimeError("Font download does not match the frozen catalog: " + entry["file"])
            path.write_bytes(data)
    subprocess.run([str(args.executable.resolve()), str(args.fixtures.resolve())], check=True)


if __name__ == "__main__":
    main()
