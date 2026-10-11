#!/usr/bin/env python3
"""Index every declared extension option, advanced field and retained hook."""

import argparse
import json
import re
import subprocess
from collections import Counter
from pathlib import Path


def read_strings(path):
    return {
        match[1]: match[2]
        for match in re.finditer(
            r'"(lng_[^"]+|ayu_[^"]+)"\s*=\s*"((?:[^"\\]|\\.)*)";',
            path.read_text(encoding="utf-8"),
        )
    }


def collect(root):
    source = root / "Telegram/SourceFiles"
    paths = subprocess.check_output(
        ["rg", "--files", str(source)], text=True
    ).splitlines()
    paths = [
        Path(path)
        for path in paths
        if Path(path).suffix in {".h", ".cpp", ".mm"}
        and not {"libs", "test", "tests"}.intersection(Path(path).parts)
    ]
    declaration = re.compile(
        r'\b(k\w+)\s*=\s*(?:Nagram::)?Option<([^>]+)>\s*\{'
        r'\s*"(nagram\.[^"]+)"(?P<rest>.{0,1600}?)'
        r'"(lng_[^"]+)"', re.S
    )
    options = []
    for path in paths:
        if "regram" not in path.parts or path.suffix != ".h":
            continue
        text = path.read_text(encoding="utf-8")
        for match in declaration.finditer(text):
            scope = re.search(r"Scope::(Device|Account)", match["rest"])
            category = re.search(r"Category::(\w+)", match["rest"])
            options.append({
                "key": match[3], "symbol": match[1], "type": match[2],
                "scope": scope[1] if scope else "Unknown",
                "category": category[1] if category else "Unknown",
                "title_key": match[5],
                "declaration": str(path.relative_to(root)),
            })
    targets = {item["symbol"] for item in options}
    consumers = {symbol: [] for symbol in targets}
    for path in paths:
        if path.suffix == ".h":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        matches = set(re.findall(r"\b[A-Za-z_]\w*", text)).intersection(targets)
        for symbol in matches:
            consumers[symbol].append(str(path.relative_to(root)))
    strings = read_strings(root / "Telegram/Resources/langs/lang.strings")
    strings.update(read_strings(root / "Telegram/Resources/langs/nagram/nagram.strings"))
    for item in options:
        item["consumers"] = consumers[item["symbol"]]
        item["localized_title_present"] = item["title_key"] in strings
        item["title"] = strings.get(item["title_key"], "")
        item["evidence_level"] = "Static-reference"
        item["runtime_acceptance"] = "Not inferred from references"
    pinned = json.loads((root / "docs/regram/reference-feature-inventory.json").read_text())
    advanced = []
    for group in pinned["advanced_settings"]:
        text = (root / group["source"]).read_text()
        for key in group["keys"]:
            advanced.append({
                "group": group["group"], "key": key, "source": group["source"],
                "serialized_key_present": f'"{key}"' in text,
                "runtime_acceptance": "Not inferred from serialization",
            })
    hooks = json.loads((root / "docs/regram/nagram-hook-audit.json").read_text())
    checked_hooks = []
    for hook in hooks["hooks"]:
        locations = []
        for relative in hook["local_files"]:
            path = source / relative
            if path.exists() and hook["hook"] in path.read_text(errors="replace"):
                locations.append(str(path.relative_to(root)))
        checked_hooks.append({
            **hook, "verified_locations": locations,
            "policy_exception": hook["status"] != "integrated",
            "runtime_acceptance": "Not inferred from hook presence",
        })
    matrix = []
    for line in (root / "docs/regram/regram-desktop-feature-matrix.md").read_text().splitlines():
        if not line.startswith("| RD-") or line.startswith("| RD-X"):
            continue
        fields = [part.strip() for part in line.split("|")][1:-1]
        matrix.append({"id": fields[0], "feature": fields[1], "criterion": fields[3]})
    retired_path = root / "docs/regram/feature-retirements.json"
    retired = json.loads(retired_path.read_text()).get("options", {}) if retired_path.exists() else {}
    for item in options:
        if item["key"] in retired:
            item["retired_per_user"] = retired[item["key"]]
    missing = [
        item["key"] for item in options
        if (not item["consumers"] or not item["localized_title_present"]) and item["key"] not in retired
    ]
    duplicate = [key for key, count in Counter(item["key"] for item in options).items() if count != 1]
    hook_gaps = [
        item["hook"] for item in checked_hooks
        if not item["policy_exception"] and not item["verified_locations"]
    ]
    retained = {item["key"] for item in pinned["options"]}
    current = {item["key"] for item in options}
    return {
        "schema_version": 1,
        "scope": "Declared extension inventory with explicit user retirements; static checks do not imply runtime acceptance",
        "user_retirements": retired,
        "reference_nagram": pinned["reference_nagram"],
        "counts": {"options": len(options), "advanced_fields": len(advanced),
                   "hooks": len(checked_hooks), "matrix_rows": len(matrix)},
        "checks": {"option_consumer_or_title_gaps": missing, "duplicate_options": duplicate,
                   "removed_retained_options": sorted(retained - current),
                   "new_options": sorted(current - retained), "hook_gaps": hook_gaps},
        "options": options, "advanced_fields": advanced, "hooks": checked_hooks,
        "matrix": matrix,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = collect(args.root.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"counts": report["counts"], "checks": report["checks"]}, ensure_ascii=False))
    failed = any(report["checks"][key] for key in (
        "option_consumer_or_title_gaps", "duplicate_options", "removed_retained_options", "hook_gaps"
    ))
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
