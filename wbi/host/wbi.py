#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""wbi.py -- inspect, clear or restore the WBI snapshot header sector.

    python wbi.py hdr       read-only dump of the 88 B header + partition probes
    python wbi.py rewrite   write the same 512 B back (write-channel self-test)
    python wbi.py clear     backup the 512 B header, then zero it
    python wbi.py restore   write the backup back
    python wbi.py clean     remove the camera-side leftovers of any run

None of these change imaging data: the only byte range involved is the first
512 B of /dev/nflasha5, which is the boot snapshot header.  `clear` makes the
next cold boot ignore the stored snapshot, so the loader takes its segment
layout from the file instead.

Build the modules first:  python ../native/build.py
"""
from __future__ import annotations

import argparse
import os
import pathlib
import sys

from jiritsu import Channel, log

HERE = pathlib.Path(__file__).resolve().parent
RESULT = "rampatch.result"
TRIGGER = "libJit.so"

TOOLS = {
    "hdr": ("rampatch_wbihdr.so", "wbihdr"),
    "rewrite": ("rampatch_wbirewrite.so", "wbirewrite"),
    "clear": ("rampatch_wbiclear.so", "wbiclear"),
    "restore": ("rampatch_wbirestore.so", "wbirestore"),
}

ALL_NAMES = {f"{n}-staged.so" for _, n in TOOLS.values()} | {TRIGGER, RESULT}


def do_clean(module_dir: pathlib.Path) -> int:
    with Channel(ALL_NAMES) as ch:
        for name in sorted(ALL_NAMES):
            data = ch.read(name, missing=True)
            if data is None:
                continue
            ch.remove(name)
            gone = ch.read(name, missing=True) is None
            log("  %-22s removed=%s (%d bytes)" % (name, gone, len(data)))
    log("cleanup done")
    return 0


def do_run(tool: str, module_dir: pathlib.Path, force: bool, wait: float) -> int:
    so_name, tag = TOOLS[tool]
    module = module_dir / so_name
    if not module.is_file():
        log("missing %s -- run ../native/build.py first" % module)
        return 2
    blob = module.read_bytes()
    stage = f"{tag}-staged.so"
    allowed = {stage, TRIGGER, RESULT}
    log("tool   : %s" % tool)
    log("module : %s (%d bytes)" % (so_name, len(blob)))
    if tool in ("clear", "restore", "rewrite"):
        log("warning: this writes %d bytes at /dev/nflasha5 offset 0"
            % (512 if tool != "rewrite" else 512))
    with Channel(allowed) as ch:
        data, _ = ch.run_module(blob, stage, force=force, poll=wait)
    text = data.decode("latin1", "replace")
    out = module_dir / f"{tag}_result.txt"
    out.write_bytes(data)
    log("-" * 60)
    log(text.rstrip())
    log("-" * 60)
    log("saved: %s" % out)
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tool", choices=sorted(TOOLS) + ["clean"])
    ap.add_argument("--module-dir", default=str(HERE.parent / "build"))
    ap.add_argument("--force", action="store_true",
                    help="run even if leftovers from an earlier run are present")
    ap.add_argument("--wait", type=float, default=60.0,
                    help="seconds to wait for the result file (default 60)")
    args = ap.parse_args(argv)
    module_dir = pathlib.Path(args.module_dir).resolve()
    if args.tool == "clean":
        return do_clean(module_dir)
    return do_run(args.tool, module_dir, args.force, args.wait)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception:
        import traceback
        traceback.print_exc()
        log("\nSTOP: check the camera state before retrying (python wbi.py clean).")
        sys.exit(1)
