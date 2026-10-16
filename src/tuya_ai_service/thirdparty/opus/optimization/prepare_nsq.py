#!/usr/bin/env python3
"""Generate NSQ state-cache/RAM candidates without altering upstream sources."""
import argparse
from pathlib import Path
import re


def prepare(source, cache_mode="all"):
    begin = '\nvoid silk_noise_shape_quantizer(\n'
    end = '\nstatic OPUS_INLINE void silk_nsq_scale_states(\n'
    if source.count(begin) != 1:
        raise ValueError("Unexpected NSQ quantizer definition")
    start = source.index(begin)
    finish = source.index(end, start)
    body = source[start:finish]
    # Only the parameter declaration may use the bare state pointer. Passing
    # it to another helper could expose cached fields before the final commit.
    code = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", body)
    if len(re.findall(r"\bNSQ\b(?!\s*->)", code)) != 1:
        raise ValueError("NSQ state pointer escapes the quantizer")
    # These scalars are private to this subframe. Helpers only access the
    # prediction/noise arrays; publish identical final scalar states at exit.
    fields = [
        ("rand_seed", "opus_int32", "cached_seed"),
        ("sDiff_shp_Q14", "opus_int32", "cached_diff"),
        ("sLF_AR_shp_Q14", "opus_int32", "cached_lf"),
        ("sLTP_shp_buf_idx", "opus_int", "cached_shape_index"),
        ("sLTP_buf_idx", "opus_int", "cached_ltp_index"),
    ]
    if cache_mode == "l511g":
        # GCC 10.2 Cortex-M3 spills more with all five scalars live. Cache
        # only the shaping index; retain the full-state candidate on T5AI.
        fields = [field for field in fields if field[0] == "sLTP_shp_buf_idx"]
    elif cache_mode != "all":
        raise ValueError("Unknown NSQ cache mode")
    for field, _, local in fields:
        if f"NSQ->{field}" not in body:
            raise ValueError(f"Missing expected state: {field}")
        body = body.replace(f"NSQ->{field}", local)
    marker = '    for( i = 0; i < length; i++ ) {'
    if body.count(marker) != 1:
        raise ValueError("Unexpected sample loop")
    # Initialize before pointer setup, since the shape index is used there.
    setup = '    shp_lag_ptr  ='
    if body.count(setup) != 1:
        raise ValueError("Unexpected pointer setup")
    declarations = ''.join(f'    {ctype} {local} = NSQ->{field};\n' for field, ctype, local in fields)
    body = body.replace(setup, declarations + '\n' + setup)
    commit = '    /* Update LPC synth buffer */'
    if body.count(commit) != 1:
        raise ValueError("Unexpected state update boundary")
    stores = ''.join(f'    NSQ->{field} = {local};\n' for field, _, local in fields)
    body = body.replace(commit, stores + '\n' + commit)
    return source[:start] + body + source[finish:]


def place_in_l511g_ram(source):
    marker = '\nvoid silk_NSQ_c\n('
    if source.count(marker) != 1:
        raise ValueError("Unexpected NSQ entry point")
    # L511G's linker and boot/wake loader already handle this section family.
    # Keep the arithmetic and inline helpers unchanged; no custom copy routine.
    attribute = '__attribute__((section(".sect_platFMRamcode_text.opus_nsq")))'
    return source.replace(marker, f'\n{attribute}\nvoid silk_NSQ_c\n(')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--no-cache", action="store_true",
                        help="Keep upstream scalar state accesses (RAM-only comparison)")
    parser.add_argument("--cache-mode", choices=("all", "l511g"), default="all",
                        help="Select full scalar caching or Cortex-M3 shaping-index caching")
    parser.add_argument("--l511g-ram", action="store_true",
                        help="Place normal NSQ in the platform MSMB RAM code section")
    args = parser.parse_args()
    output = args.input.read_text(encoding="utf-8")
    if not args.no_cache:
        output = prepare(output, args.cache_mode)
    if args.l511g_ram:
        output = place_in_l511g_ram(output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != output:
        args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
