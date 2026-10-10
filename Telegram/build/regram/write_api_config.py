#!/usr/bin/env python3
import os
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
api_id = os.environ["TDESKTOP_API_ID"]
api_hash = os.environ["TDESKTOP_API_HASH"]
if not re.fullmatch(r"[0-9]+", api_id) or not re.fullmatch(r"[0-9a-fA-F]{32}", api_hash):
    raise SystemExit("Invalid private build API configuration.")
config = root / ".local/regram/api.cmake"
config.parent.mkdir(parents=True, exist_ok=True)
config.write_text(
    f'set(TDESKTOP_API_ID "{api_id}" CACHE STRING "API ID" FORCE)\n'
    f'set(TDESKTOP_API_HASH "{api_hash}" CACHE STRING "API hash" FORCE)\n'
)
config.chmod(0o600)
print("Private build API configuration prepared.")
