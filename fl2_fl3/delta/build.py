#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build.py -- 编译 CP delta 模块（AArch64，freestanding）。

    python build.py
"""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "build"
SRC = HERE / "cpdelta.c"

FLAGS = [
    "--target=aarch64-linux-gnu", "-fuse-ld=lld", "-O2", "-fPIC",
    "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fvisibility=hidden", "-nostdlib", "-shared",
    "-Wl,-init,_init", "-Wl,-z,defs", "-Wl,--no-undefined",
    "-Wl,--build-id=none",
]

CANDIDATES = [
    os.environ.get("CLANG"),
    r"C:\Program Files\LLVM\bin\clang.exe",
    shutil.which("clang"),
    shutil.which("clang-18"),
    shutil.which("clang-17"),
]


def find_clang(explicit: str | None) -> str:
    if explicit:
        if not pathlib.Path(explicit).is_file():
            raise SystemExit(f"clang not found: {explicit}")
        return explicit
    for c in CANDIDATES:
        if c and pathlib.Path(c).is_file():
            return c
    raise SystemExit("no clang found; pass --clang or set CLANG")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--clang")
    args = ap.parse_args(argv)
    clang = find_clang(args.clang)
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / "rampatch_cpdelta.so"
    cmd = [clang, *FLAGS, str(SRC), "-o", str(target)]
    result = subprocess.run(cmd, text=True, capture_output=True)
    if result.returncode:
        print("compile failed:", " ".join(cmd), file=sys.stderr)
        print(result.stdout, result.stderr, file=sys.stderr)
        return 1
    size = target.stat().st_size
    if size >= 65536:
        raise SystemExit(f"{target.name} is {size} B; the channel limits a single file to 65536 B")
    print(f"clang : {clang}")
    print(f"  {target.name:<24} {size:6d} B")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
