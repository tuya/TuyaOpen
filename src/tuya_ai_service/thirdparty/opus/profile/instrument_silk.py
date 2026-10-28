#!/usr/bin/env python3
"""Generate an instrumented copy of Opus 1.3.1 SILK; leave upstream untouched."""
import argparse
from pathlib import Path
import re


def wrap_calls(text, stages):
    for function, stage in stages:
        pattern = rf"(?m)^( +)({function}\([^;]+;)"
        def wrap(match):
            indent, call = match.groups()
            return (f"{indent}{{\n{indent}    uint64_t start = tuyaopen_opus_profile_now();\n"
                    f"{indent}    {call}\n"
                    f"{indent}    tuyaopen_opus_profile_record(OPUS_PROFILE_{stage}, "
                    f"tuyaopen_opus_profile_now() - start);\n{indent}}}")
        text, count = re.subn(pattern, wrap, text)
        if count != 1:
            raise ValueError(f"Expected one {function} call, found {count}")
    return text


def instrument(source, filename="encode_frame_FIX.c"):
    text = '#include "opus_profile.h"\n' + source
    if filename == "find_pred_coefs_FIX.c":
        return wrap_calls(text, [
            ("silk_find_LTP_FIX", "PRED_LTP_ANALYSIS"),
            ("silk_quant_LTP_gains", "PRED_LTP_QUANT"),
            ("silk_LTP_analysis_filter_FIX", "PRED_LTP_FILTER"),
            ("silk_scale_copy_vector16", "PRED_UNVOICED_SCALE"),
            ("silk_find_LPC_FIX", "PRED_LPC"),
            ("silk_process_NLSFs", "PRED_NLSF"),
            ("silk_residual_energy_FIX", "PRED_RESIDUAL"),
        ])
    if filename in ("NSQ.c", "NSQ_del_dec.c"):
        delayed = filename == "NSQ_del_dec.c"
        return wrap_calls(text, [
            ("silk_LPC_analysis_filter", "NSQ_REWHITE"),
            ("silk_nsq_del_dec_scale_states" if delayed else "silk_nsq_scale_states", "NSQ_SCALE"),
            ("silk_noise_shape_quantizer_del_dec" if delayed else "silk_noise_shape_quantizer",
             "NSQ_DELAYED" if delayed else "NSQ_NORMAL"),
        ])
    if filename != "encode_frame_FIX.c":
        raise ValueError(f"Unsupported source: {filename}")
    stages = [
        ("silk_find_pitch_lags_FIX", "PITCH"),
        ("silk_noise_shape_analysis_FIX", "SHAPE"),
        ("silk_find_pred_coefs_FIX", "PREDICT"),
        ("silk_process_gains_FIX", "GAINS"),
        ("silk_LBRR_encode_FIX", "LBRR"),
    ]
    text = wrap_calls(text, stages)

    # Cover both normal and delayed-decision NSQ with one exclusive interval.
    begin = "                if( psEnc->sCmn.nStatesDelayedDecision > 1 || psEnc->sCmn.warping_Q16 > 0 ) {"
    end = "                if ( iter == maxIter && !found_lower ) {"
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError("Unexpected NSQ block layout")
    text = text.replace(begin, "                {\n                uint64_t start = tuyaopen_opus_profile_now();\n" + begin)
    text = text.replace(end, "                tuyaopen_opus_profile_record(OPUS_PROFILE_NSQ, tuyaopen_opus_profile_now() - start);\n                }\n\n" + end)

    # Include normal entropy coding and the final damage-control fallback.
    pattern = r"(?m)^( +)(silk_encode_indices\([^;]+;(?:\s|/\*[\s\S]*?\*/)*silk_encode_pulses\([^;]+;)"
    def entropy(match):
        indent, calls = match.groups()
        return (f"{indent}{{\n{indent}    uint64_t start = tuyaopen_opus_profile_now();\n"
                f"{indent}    {calls}\n"
                f"{indent}    tuyaopen_opus_profile_record(OPUS_PROFILE_ENTROPY, "
                f"tuyaopen_opus_profile_now() - start);\n{indent}}}")
    text, count = re.subn(pattern, entropy, text)
    if count != 2:
        raise ValueError(f"Expected two entropy coding blocks, found {count}")
    loop = "        for( iter = 0; ; iter++ ) {"
    if text.count(loop) != 1:
        raise ValueError("Unexpected bitrate-control loop layout")
    return text.replace(loop, loop + "\n            tuyaopen_opus_profile_iteration((unsigned int)iter);")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = instrument(args.input.read_text(encoding="utf-8"), args.input.name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != output:
        args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
