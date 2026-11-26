#!/usr/bin/env python3
"""
prepare_firmware.py - build the /NX2_23_0_1/ folder expected by the Switch 1 sysmodule.

The sysmodule serves the Switch 2 firmware 23.0.1 (SystemUpdate 0x0400000000000816,
version 0x5C01753C) straight from the SD card as raw NCA files named <ContentId>.nca.
A NCA's ContentId is the first 16 bytes of its SHA-256, so every file can be verified
without any key: this script checks name, size and hash of all 299 expected files.

Input : a folder (searched recursively) containing the firmware NCAs. Both layouts work:
          <id>.nca            for the 149 content NCAs
          <id>.cnmt.nca       for the 150 meta NCAs   (or plain <id>.nca)
        Other files (.cnmt, .tik, ...) are ignored.
Output: <out>/<id>.nca for all 299 files (the meta NCAs lose their ".cnmt" part).
        Copy that folder to the root of the Switch 1 SD card -> sdmc:/NX2_23_0_1/

Usage : python3 prepare_firmware.py SOURCE_DIR [OUT_DIR] [--check-only]
"""
import hashlib
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def sha_id(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(1 << 20)
            if not b:
                break
            h.update(b)
    return h.hexdigest()[:32]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check_only = "--check-only" in sys.argv
    if not args:
        print(__doc__)
        return 2
    src = args[0]
    out = args[1] if len(args) > 1 else "NX2_23_0_1"
    manifest = json.load(open(os.path.join(HERE, "firmware_manifest.json")))["files"]

    found = {}
    for root, _, files in os.walk(src):
        for n in files:
            low = n.lower()
            if not low.endswith(".nca"):
                continue
            cid = low[:-9] if low.endswith(".cnmt.nca") else low[:-4]
            if cid in manifest:
                found.setdefault(cid, os.path.join(root, n))

    missing = sorted(set(manifest) - set(found))
    print(f"expected {len(manifest)} files, found {len(found)}")
    if missing:
        print(f"MISSING {len(missing)} file(s):")
        for m in missing:
            print(f"  {m} ({manifest[m]['kind']}, {manifest[m]['size']} bytes)")
        return 1

    bad = 0
    for i, (cid, path) in enumerate(sorted(found.items()), 1):
        size = os.path.getsize(path)
        if size != manifest[cid]["size"]:
            print(f"\nBAD SIZE  {cid}: {size} != {manifest[cid]['size']}")
            bad += 1
            continue
        if sha_id(path) != cid:
            print(f"\nBAD HASH  {cid}: file is corrupted or modified")
            bad += 1
            continue
        print(f"\r[{i}/{len(found)}] verified", end="", flush=True)
    print()
    if bad:
        print(f"{bad} bad file(s): do NOT use this firmware dump.")
        return 1
    if check_only:
        print("All files verified.")
        return 0

    os.makedirs(out, exist_ok=True)
    for i, (cid, path) in enumerate(sorted(found.items()), 1):
        size = manifest[cid]["size"]
        dst = os.path.join(out, cid + ".nca")
        if os.path.exists(dst) and (
            os.path.samefile(path, dst)
            or (os.path.getsize(dst) == size and sha_id(dst) == cid)
        ):
            pass
        else:
            shutil.copyfile(path, dst)
        print(f"\r[{i}/{len(found)}] prepared", end="", flush=True)
    print()
    print(f"Done: {len(found)} files verified and written to {out}/")
    print("Copy this folder to the SD root of the Switch 1 (sdmc:/NX2_23_0_1/).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
