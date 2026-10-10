#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description="Check Release startup in a fresh temporary profile.")
parser.add_argument("--executable", type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="regram-launch-") as directory:
    profile = Path(directory)
    with (profile / "stdout.log").open("w") as log:
        process = subprocess.Popen([
            str(args.executable.resolve()), "-many", "-noupdate", "-workdir", directory,
        ], stdout=log, stderr=subprocess.STDOUT)
        try:
            time.sleep(12)
            if process.poll() is not None:
                raise SystemExit(f"Release exited early: {process.returncode}")
            print("Release stayed running for 12 seconds with a fresh profile.")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
