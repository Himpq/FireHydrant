#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cpdelta.py -- 把 33 窗口的 delta 写入 /cp/cpapp-b.bin（原地，不传整文件）。

    python cpdelta.py --mode apply     原厂 → 候选
    python cpdelta.py --mode restore   候选 → 原厂（允许部分应用）
    python cpdelta.py --clean          清掉相机侧残留

载荷是 `../data/cp.delta`（9,536 B）。相机侧模块先逐窗口校验 before，全部
通过才写 after，再逐窗口读回；中途任何一个不符就整批中止。
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from jiritsu import Channel, log

HERE = pathlib.Path(__file__).resolve().parent
DATA = HERE.parent / "data" / "cp.delta"
DELTA_NAME = "cp.delta"
MODE_NAME = "cpdelta.mode"
STAGE = "cpdelta-staged.so"
TRIGGER = "libJit.so"
RESULT = "cpdelta.result"
MODULE = "rampatch_cpdelta.so"

NAMES = {DELTA_NAME, MODE_NAME, STAGE, TRIGGER, RESULT}


def do_clean() -> int:
    with Channel(NAMES) as ch:
        for name in sorted(NAMES):
            data = ch.read(name, missing=True)
            if data is None:
                continue
            ch.remove(name)
            gone = ch.read(name, missing=True) is None
            log("  %-22s removed=%s (%d bytes)" % (name, gone, len(data)))
    log("cleanup done")
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", choices=["apply", "restore"], default="apply")
    ap.add_argument("--module-dir", default=str(HERE.parent / "build"))
    ap.add_argument("--force", action="store_true",
                    help="run even if leftovers from an earlier run are present")
    ap.add_argument("--wait", type=float, default=120.0,
                    help="seconds to wait for the result file (default 120)")
    ap.add_argument("--clean", action="store_true")
    args = ap.parse_args(argv)

    if args.clean:
        return do_clean()

    module_dir = pathlib.Path(args.module_dir).resolve()
    module = module_dir / MODULE
    if not module.is_file():
        log("missing %s -- run ../native/build.py first" % module)
        return 2
    if not DATA.is_file():
        log("missing %s" % DATA)
        return 2

    delta = DATA.read_bytes()
    blob = module.read_bytes()
    log("delta  : %s (%d bytes, %d windows)" % (DATA.name, len(delta), 33))
    log("module : %s (%d bytes)" % (MODULE, len(blob)))
    log("mode   : %s" % args.mode)

    with Channel(NAMES) as ch:
        ch.write(DELTA_NAME, delta)
        ch.write(MODE_NAME, (args.mode + "\n").encode("ascii"))
        data, _ = ch.run_module(blob, STAGE, out_name=RESULT,
                                force=args.force, poll=args.wait)

    text = data.decode("latin1", "replace")
    out = module_dir / "cpdelta_result.txt"
    out.write_bytes(data)
    log("-" * 60)
    log(text.rstrip())
    log("-" * 60)
    log("saved: %s" % out)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception:
        import traceback
        traceback.print_exc()
        log("\nSTOP: check the camera state before retrying (python cpdelta.py --clean).")
        sys.exit(1)
