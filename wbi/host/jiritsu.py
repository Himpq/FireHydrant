#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""jiritsu.py -- minimal Jiritsu file channel (Sony camera, service identity).

Scope: open the camera, raise/enter the service identity, authenticate, and
expose the four file commands the WBI tools need:

    15  select path
    16  write  (<= 65536 B per call)
    17  read
    18  remove

Only names inside /tmp/jiritsu are touched; the caller declares which names it
may use.

Transport sequence (same one PMCA-RE documents for this body):
    MscContext + VendorSpecificContext -> listDevices
    -> MSC 0x0E76 getDeviceInfo (manufacturer == Sony)
    -> auth.start()/authenticate()/stop()
    -> poll for service identity 0x0336 -> authenticate
    -> SonySenserDevice -> Jiritsu 15/16/17/18

Requires PMCA-RE (`pmca` package) importable.  Put it next to this file as
`pmca-re/`, set PMCA_PATH, or install it.
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
import time

MSC_PID = 0x0E76          # A6700 mass-storage identity (A7C II uses 0x0E8a)
SVC_PID = 0x0336          # service identity that appears after authentication
PREFIX = "/tmp/jiritsu/"
MAX_TEMP = 65536


def _add_pmca_to_path() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.environ.get("PMCA_PATH"),
        os.path.join(here, "pmca-re"),
        os.path.join(here, "..", "pmca-re"),
        os.path.join(here, "..", "..", "fl2_fl3", "pmca-re"),
    ]
    for p in candidates:
        if p and os.path.isdir(os.path.join(p, "pmca")):
            sys.path.insert(0, os.path.abspath(p))
            return


_add_pmca_to_path()

try:
    from pmca.usb.sony import (SONY_ID_VENDOR, SonySenserAuthDevice,  # noqa: E402
                               SonySenserDevice)
    from pmca.usb.driver.generic import GenericUsbDriver, MscBbbDriver  # noqa: E402
    from pmca.usb.driver.generic import libusb as pmca_libusb  # noqa: E402
    from pmca.usb import MscDevice  # noqa: E402
except ImportError as exc:  # pragma: no cover - environment issue
    raise SystemExit("PMCA-RE not found (need the `pmca` package): %s" % exc)


def log(msg: str) -> None:
    try:
        print(msg, flush=True)
    except UnicodeEncodeError:
        print(msg.encode("gbk", "replace").decode("gbk"), flush=True)


def _dev_key(dev):
    a = getattr(dev, "address", None)
    if a is None:
        a = getattr(getattr(dev, "dev", None), "address", None)
    return a if a is not None else hex(id(dev))


def _open_contexts():
    return [pmca_libusb.MscContext(), pmca_libusb.VendorSpecificContext()]


def _list_sony(ctxs, quiet=False):
    found = []
    for ctx in ctxs:
        try:
            for dev in ctx.listDevices(SONY_ID_VENDOR):
                try:
                    found.append(ctx.openDevice(dev))
                except Exception as exc:
                    if not quiet:
                        log("     (open failed) %r" % (exc,))
        except Exception as exc:
            if not quiet:
                log("     (context failed) %r" % (exc,))
    seen, uniq = set(), []
    for drv in found:
        k = _dev_key(drv)
        if k not in seen:
            seen.add(k)
            uniq.append(drv)
    return uniq


def _authenticate(drv):
    auth = SonySenserAuthDevice(drv)
    try:
        auth.start()
    except Exception:
        pass
    auth.authenticate()
    return auth


def _usb_reset(drv):
    dev = getattr(getattr(drv, "backend", None), "dev", None) or getattr(drv, "dev", None)
    if dev is None or not hasattr(dev, "reset"):
        raise RuntimeError("cannot find the pyusb device object")
    import usb.util
    try:
        usb.util.dispose_resources(dev)
    except Exception:
        pass
    dev.reset()
    time.sleep(2.5)


def _set_timeout(drv, ms=20000):
    for chain in (("backend", "dev"), ("dev",)):
        obj = drv
        for attr in chain:
            obj = getattr(obj, attr, None)
            if obj is None:
                break
        if obj is not None and hasattr(obj, "default_timeout"):
            obj.default_timeout = ms


class Channel:
    """Authenticated Jiritsu file channel restricted to declared names."""

    def __init__(self, allowed, wait=45):
        self.allowed = set(allowed)
        self.wait = wait
        self.healthy = True

    # -- lifecycle ---------------------------------------------------------
    def __enter__(self):
        self.ctxs = _open_contexts()
        devs = _list_sony(self.ctxs)
        log("visible Sony devices: %d" % len(devs))
        for drv in devs:
            log("  id = %s" % (drv.getId(),))

        svc = [d for d in devs if d.getId()[1] == SVC_PID]
        if svc:
            log("service identity already present -- authenticating directly")
            self.auth = _authenticate(svc[0])
            self.drv = svc[0]
        else:
            msc = None
            for drv in devs:
                if not isinstance(drv, (MscBbbDriver, GenericUsbDriver)):
                    continue
                try:
                    info = MscDevice(drv).getDeviceInfo()
                    man = getattr(info, "manufacturer", "")
                    if isinstance(man, (bytes, bytearray)):
                        man = man.decode("utf-8", "ignore")
                    if "Sony" in str(man):
                        msc = drv
                        log("MSC device: pid=0x%04X model=%s"
                            % (drv.getId()[1], getattr(info, "model", "?")))
                        break
                except Exception:
                    continue
            if msc is None:
                raise RuntimeError("no Sony MSC camera and no service identity; "
                                   "set the camera to the mass-storage USB mode and replug")
            log("asking the camera to raise the service identity ...")
            auth = SonySenserAuthDevice(msc)
            try:
                auth.start()
            except Exception as exc:
                log("start warning: %r" % (exc,))
            auth.authenticate()
            log("round 1 authentication passed")
            try:
                auth.stop()
            except Exception:
                pass

            deadline = time.time() + self.wait
            while time.time() < deadline:
                time.sleep(1.0)
                for drv in _list_sony(self.ctxs, quiet=True):
                    if drv.getId()[1] != SVC_PID:
                        continue
                    try:
                        self.auth = _authenticate(drv)
                        self.drv = drv
                        break
                    except Exception as exc:
                        log("  0x0336 authentication failed %r, retrying" % (exc,))
                else:
                    continue
                break
            else:
                raise RuntimeError("service identity 0x0336 did not appear within %d s"
                                   % self.wait)
            log("round 2 authentication passed")

        _set_timeout(self.drv)
        self.channel = SonySenserDevice(self.auth.driver)
        return self

    def __exit__(self, *args):
        if getattr(self, "healthy", False):
            try:
                self.auth.stop()
            except Exception:
                pass

    # -- protocol ----------------------------------------------------------
    def _call(self, fn, *args):
        if not self.healthy:
            raise RuntimeError("session state is unknown; stopping further commands")
        try:
            return fn(*args)
        except Exception:
            self.healthy = False
            raise

    def _cmd(self, cmd, data=b"", missing=False):
        if cmd not in (15, 16, 17, 18):
            raise ValueError("jiritsu command not allowed")
        response, out = self._call(self.channel.sendSenserPacket, 0x40,
                                   struct.pack("<HH", 0x1001, cmd) + data)
        if response or len(out) < 4:
            raise RuntimeError("malformed Jiritsu response %r" % (response,))
        status = struct.unpack_from("<I", out)[0]
        if missing and status == 0x10000006:
            return None
        if status:
            raise RuntimeError("Jiritsu status 0x%x" % status)
        return out[4:]

    def _select(self, name):
        if name not in self.allowed:
            raise ValueError("name not allowed: %r" % (name,))
        if not name or "/" in name or "\0" in name or len(name) > 200:
            raise ValueError("bad name: %r" % (name,))
        self._cmd(15, b"\0" + (PREFIX + name).encode("ascii") + b"\0")

    # -- file operations ---------------------------------------------------
    def read(self, name, missing=False):
        self._select(name)
        return self._cmd(17, b"\0\xff\0", missing)

    def write(self, name, data):
        if not data:
            raise ValueError("empty write not allowed")
        if len(data) > MAX_TEMP:
            raise ValueError("file too large (%d > %d)" % (len(data), MAX_TEMP))
        self._select(name)
        self._cmd(16, b"\0\xff\0" + data)

    def remove(self, name):
        self._select(name)
        self._cmd(18, b"\0\xff")

    def run_module(self, blob, stage_name, out_name="rampatch.result",
                   trigger_name="libJit.so", poll=120, interval=0.5,
                   force=False):
        """Stage a module, trigger it, and return its result text.

        Refuses to start when leftovers from an earlier run are still on the
        camera, unless force is set -- a stale libJit.so is otherwise replayed.
        """
        digest = hashlib.sha256(blob).hexdigest()
        leftover = {n: self.read(n, missing=True) for n in (stage_name, trigger_name, out_name)}
        stale = [n for n, v in leftover.items() if v is not None]
        if stale and not force:
            raise RuntimeError("leftover files on camera: %s (use --clean or --force)"
                               % ", ".join(stale))
        self.write(stage_name, blob)
        back = self.read(stage_name)
        if not back or hashlib.sha256(back).hexdigest() != digest:
            raise RuntimeError("stage readback mismatch -- nothing triggered")
        if leftover.get(out_name) is not None:
            self.remove(out_name)
        self.write(trigger_name, blob)
        deadline = time.time() + poll * interval
        data = None
        while time.time() < deadline:
            data = self.read(out_name, missing=True)
            if data:
                break
            time.sleep(interval)
        if not data:
            raise RuntimeError("no result from the module")
        return data, leftover
