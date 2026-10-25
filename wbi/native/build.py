#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build.py -- compile the four WBI modules for aarch64 Linux.

    python build.py                 # finds clang, builds into ../build/
    python build.py --clang <path>  # explicit clang

Needs clang with the aarch64 target and lld.  The modules are freestanding:
no libc, no link against anything, entry point `_init`.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "build"

VARIANTS = [
    (211, "wbihdr", "rampatch_wbihdr.so"),
    (212, "wbirewrite", "rampatch_wbirewrite.so"),
    (213, "wbiclear", "rampatch_wbiclear.so"),
    (214, "wbirestore", "rampatch_wbirestore.so"),
    (215, "apfl23", "rampatch_apfl23.so"),
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


FLAGS = [
    "--target=aarch64-linux-gnu", "-fuse-ld=lld", "-O2", "-fPIC",
    "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fvisibility=hidden", "-nostdlib", "-shared",
    "-Wl,-init,_init", "-Wl,-z,defs", "-Wl,--no-undefined",
    "-Wl,--build-id=none",
]


def compile_to(clang: str, extra: list[str], src: pathlib.Path, out_name: str) -> pathlib.Path:
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / out_name
    cmd = [clang, *FLAGS, *extra, "-I", str(HERE), str(src), "-o", str(target)]
    result = subprocess.run(cmd, text=True, capture_output=True)
    if result.returncode:
        print("compile failed:", " ".join(cmd), file=sys.stderr)
        print(result.stdout, result.stderr, file=sys.stderr)
        raise SystemExit(1)
    size = target.stat().st_size
    if size >= 65536:
        raise SystemExit(f"{out_name} is {size} B; the channel limits a single file to 65536 B")
    return target


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--clang")
    args = ap.parse_args(argv)
    clang = find_clang(args.clang)
    print(f"clang : {clang}")
    for apply_id, name, out_name in VARIANTS:
        p = compile_to(clang,
                       [f"-DPATCH_APPLY={apply_id}", f'-DMODE_NAME="{name}"'],
                       HERE / "jiritsu_module.c", out_name)
        print(f"  {out_name:<28} {p.stat().st_size:6d} B  (PATCH_APPLY={apply_id})")
    print(f"output: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
