#!/usr/bin/env python3
"""Build/test RunClock without npm dependencies. PS5 uses Zig + payload SDK."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)

parser = argparse.ArgumentParser()
parser.add_argument("target", choices=["test", "host", "ps5"])
args = parser.parse_args()
build = ROOT / "build"
build.mkdir(exist_ok=True)
cc = os.environ.get("CC", "cc")
common = ["-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror", "-Isrc", "-Ithird_party/prospero-menu-sdk"]
if args.target == "test":
    exe = build / ("test_core.exe" if os.name == "nt" else "test_core")
    run([cc, *common, "src/runclock.c", "tests/test_core.c", "-o", exe])
    run([exe])
    run([os.environ.get("NODE", "node"), "tests/test_timer.cjs"])
elif args.target == "host":
    run([cc, *common, "src/runclock.c", "native/main.c", "third_party/prospero-menu-sdk/pmo.c", "-pthread", "-o", build / "runclock-host"])
    print("Built build/runclock-host. For standalone protocol testing: run ../tests/integration.mjs from the project root.")
else:
    sdk_var = os.environ.get("PS5_PAYLOAD_SDK")
    if not sdk_var:
        sys.exit("Set PS5_PAYLOAD_SDK to the extracted v0.43 ps5-payload-sdk directory. See docs/BUILD.md.")
    sdk = Path(sdk_var).resolve()
    zig = os.environ.get("ZIG", "zig")
    for item in ["target/include", "target/lib/crt1.o", "target/lib/libc.a", "ldscripts/elf_x86_64.x"]:
        if not (sdk / item).exists(): sys.exit("SDK input missing: " + str(sdk / item))
    out = build / "ps5" / "runclock"
    out.mkdir(parents=True, exist_ok=True)
    objects = []
    for source in ["src/runclock.c", "native/main.c", "third_party/prospero-menu-sdk/pmo.c"]:
        obj = build / (Path(source).stem + ".ps5.o")
        run([zig, "cc", *common, "-Os", "-target", "x86_64-linux-none", "-U__linux__", "-D__FreeBSD__=11", "-D__PS5__", "-isystem", sdk / "target/include", "-fPIE", "-fno-stack-protector", "-fno-plt", "-femulated-tls", "-nostdlib", "-c", source, "-o", obj])
        objects.append(obj)
    run([zig, "ld.lld", "--no-dependent-libraries", "--eh-frame-hdr", "-pie", "--hash-style=gnu", "-z", "max-page-size=0x4000", "-T", sdk / "ldscripts/elf_x86_64.x", *objects, sdk / "target/lib/crt1.o", "-L", sdk / "target/lib", "--start-group", sdk / "target/lib/libc.a", "-ldl", "--end-group", "-lkernel_web", "-lSceLibcInternal", "-lSceNet", "-o", out / "runclock.ovl"])
    for name in ["overlay.json", "route.txt"]: shutil.copy2(ROOT / "native" / name, out / name)
    print("Built", out, "(compiled only; PS5 runtime has not been validated)")
