#!/usr/bin/env python3

import argparse
import hashlib
import json
import sys
from pathlib import Path


def _resolve_path(value: str) -> Path | None:
    if not value:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = (Path.cwd() / path).resolve()
    return path


def _fingerprint_file(value: str) -> dict | None:
    path = _resolve_path(value)
    if path is None:
        return None

    entry = {"path": str(path)}
    if path.is_file():
        entry["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    else:
        entry["missing"] = True
    return entry


def _state_from_args(args: argparse.Namespace) -> dict:
    return {
        "board": args.board,
        "board_variant": args.board_variant,
        "preview_version_2": args.preview_version_2,
        "user_c_modules": _fingerprint_file(args.user_c_modules),
        "frozen_manifest": _fingerprint_file(args.frozen_manifest),
    }


def _load_state(path: Path) -> dict | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text())


def cmd_check(args: argparse.Namespace) -> int:
    build_dir = Path(args.build_dir)
    state_file = Path(args.state_file)
    if not build_dir.exists():
        return 0

    current_state = _state_from_args(args)
    previous_state = _load_state(state_file)
    if previous_state == current_state:
        return 0

    if previous_state is None:
        print(
            f"No prior build input state recorded for {build_dir}; forcing clean rebuild.",
            file=sys.stderr,
        )
    else:
        print(
            f"Build input state changed for {build_dir}; forcing clean rebuild.",
            file=sys.stderr,
        )
    return 2


def cmd_write(args: argparse.Namespace) -> int:
    state_file = Path(args.state_file)
    state_file.parent.mkdir(parents=True, exist_ok=True)
    state_file.write_text(json.dumps(_state_from_args(args), indent=2, sort_keys=True) + "\n")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Track build inputs that require a clean ESP32 firmware rebuild.",
    )
    parser.add_argument("command", choices=("check", "write"))
    parser.add_argument("--state-file", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--board", default="")
    parser.add_argument("--board-variant", default="")
    parser.add_argument("--user-c-modules", default="")
    parser.add_argument("--frozen-manifest", default="")
    parser.add_argument("--preview-version-2", default="")
    args = parser.parse_args()

    if args.command == "check":
        return cmd_check(args)
    return cmd_write(args)


if __name__ == "__main__":
    raise SystemExit(main())
