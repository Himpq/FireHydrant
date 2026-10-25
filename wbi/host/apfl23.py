#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""apfl23.py -- 把 AP 的 ELF 程序头从扩展值改回 FL23 基线。

    python apfl23.py            写入并读回
    python apfl23.py --clean    清掉相机侧残留

只写 /usr/lib/appFw.so 偏移 0x60 的 16 字节（p_filesz / p_memsz）。当前值不是
0x4489230 时中止，不会把未知状态改成别的东西。

模块先挂载 /usr 为可写、写入、fsync、读回校验，再挂回只读。
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from jiritsu import Channel, log

HERE = pathlib.Path(__file__).resolve().parent
RESULT = "rampatch.result"
TRIGGER = "libJit.so"
STAGE = "apfl23-staged.so"
MODULE = "rampatch_apfl23.so"
NAMES = {STAGE, TRIGGER, RESULT}


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
    ap.add_argument("--module-dir", default=str(HERE.parent / "build"))
    ap.add_argument("--clean", action="store_true")
    ap.add_argument("--force", action="store_true",
                    help="run even if leftovers from an earlier run are present")
    ap.add_argument("--wait", type=float, default=60.0,
                    help="seconds to wait for the result file (default 60)")
    args = ap.parse_args(argv)

    if args.clean:
        return do_clean()

    module_dir = pathlib.Path(args.module_dir).resolve()
    module = module_dir / MODULE
    if not module.is_file():
        log("missing %s -- run ../native/build.py first" % module)
        return 2

    blob = module.read_bytes()
    log("module : %s (%d bytes)" % (MODULE, len(blob)))
    log("target : /usr/lib/appFw.so +0x60, 16 bytes (p_filesz / p_memsz)")

    with Channel(NAMES) as ch:
        data, _ = ch.run_module(blob, STAGE, force=args.force, poll=args.wait)

    text = data.decode("latin1", "replace")
    out = module_dir / "apfl23_result.txt"
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
        log("\nSTOP: check the camera state before retrying (python apfl23.py --clean).")
        sys.exit(1)
