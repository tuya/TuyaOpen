#!/usr/bin/env python3
"""Host bit-exact check of NSQ candidates; does not measure target performance.

Run from any directory with Python 3, gcc and ar available. No downloads.
SMULL is emulated with int64 arithmetic; actual ARM instructions need board tests.
"""

import concurrent.futures
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


TESTS = Path(__file__).resolve().parent
OPUS_ROOT = TESTS.parent
ROOT = OPUS_ROOT.parents[3]


def run(command, **kwargs):
    subprocess.run([str(arg) for arg in command], check=True, **kwargs)


def verify(work):
    opus = work / "opus"
    shutil.copytree(OPUS_ROOT / "upstream", opus)
    shutil.copytree(OPUS_ROOT / "optimization", work / "optimization")
    # Match 32-bit ARM's portable macro path, not the host's fast int64 path.
    arch = opus / "celt/arch.h"
    text = arch.read_text()
    assert text.count("#define OPUS_FAST_INT64 1") == 1
    arch.write_text(text.replace("#define OPUS_FAST_INT64 1", "#define OPUS_FAST_INT64 0"))
    header = work / "optimization/opus_cortex_m3_macros.h"
    text = header.read_text()
    guard = "#if !defined(__GNUC__) || !defined(__arm__) || !defined(__ARM_ARCH_7M__)"
    asm = '__asm__("smull %0, %1, %2, %3" : "=&r"(low), "=&r"(high) : "r"(a), "r"(b));'
    assert text.count(guard) == text.count(asm) == 1
    header.write_text(text.replace(guard, "#if 0").replace(
        asm, "high = (opus_int32)(((opus_int64)a * b) >> 32); (void)low;"))

    excluded = {"opus_demo", "opus_compare", "repacketizer_demo", "opus_custom_demo",
                "analysis", "mlp", "mlp_data"}
    sources = sorted(path for directory in ("src", "celt", "silk", "silk/fixed")
                     for path in (opus / directory).glob("*.c") if path.stem not in excluded)
    flags = ["gcc", "-std=c99", "-O3", "-DOPUS_BUILD", "-DFIXED_POINT",
             "-DDISABLE_FLOAT_API", "-DVAR_ARRAYS", "-DHAVE_LRINTF", '-DPACKAGE_VERSION="v1.3.1"']
    flags += ["-I" + str(opus / directory) for directory in
              ("include", "include/opus", "src", "celt", "silk", "silk/fixed")]
    script = OPUS_ROOT / "optimization/prepare_nsq.py"
    candidates = {}
    for name, options in (("cache", []), ("ram", ["--no-cache", "--l511g-ram"]),
                          ("cache_ram", ["--l511g-ram"]),
                          ("m3_cache", ["--cache-mode", "l511g"]),
                          ("m3_cache_ram", ["--cache-mode", "l511g", "--l511g-ram"])):
        path = work / (name + ".c")
        run([sys.executable, script, "--input", opus / "silk/NSQ.c", "--output", path, *options])
        candidates[name] = path

    reference = None
    for arithmetic in ("portable", "smull_emulated"):
        opts = [] if arithmetic == "portable" else ["-include", str(header)]
        objdir = work / arithmetic
        objdir.mkdir()

        def compile_source(source):
            obj = objdir / (source.stem + ".o")
            result = subprocess.run(flags + opts + ["-c", str(source), "-o", str(obj)],
                                    capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stderr)
            return obj

        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            objects = list(pool.map(compile_source, sources))
        for name, source in {"upstream": None, **candidates}.items():
            linked = objects
            if source is not None:
                obj = compile_source(source)
                linked = [item for item in objects if item.name != "NSQ.o"] + [obj]
            exe = work / (arithmetic + "_" + name)
            run(flags + ["-I" + str(ROOT / "examples/multimedia/opus_encode/src"),
                         TESTS / "compare_packets.c", *linked, "-lm", "-o", exe])
            result = subprocess.run([str(exe)], check=True, stdout=subprocess.PIPE)
            if reference is None:
                reference = result.stdout
            assert reference == result.stdout, f"Packet mismatch: {arithmetic}/{name}"
            print(f"PASS {arithmetic}/{name}: 3960 packets, sha256="
                  f"{hashlib.sha256(result.stdout).hexdigest()}", flush=True)
    print("Host arithmetic/output checks only; RAM loading and timing require the board.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="opus-nsq-check-") as directory:
        verify(Path(directory))
