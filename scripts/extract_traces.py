#!/usr/bin/env python3
"""Extract bundled replay traces from vortex/logs/*.log.

Each log is parsed into an ordered stream of three event kinds:

  BULK_H2D,<size>                -- standalone H2D DMA (data preload, kernel bin)
  BULK_D2H,<size>                -- standalone D2H DMA (final result readback)
  BUNDLE,<h2d>,<d2h>,<cycles>    -- one START_COMPUTATION on the Jigsaw device:
                                    H2D-in (kernel args + flag resets) +
                                    busy-wait compute + D2H-out (flag readbacks)

The bundling matches how the Jigsaw computation device naturally operates:
one MMIO write to START_COMPUTATION_REG triggers H2D + compute + D2H as a
fused operation. Small DMAs adjacent to a COMPUTE event are folded into
the bundle's H2D/D2H sides. Large DMAs (above `threshold`, default 256 B)
remain standalone.

Sources of truth in the runtime:
  vortex/runtime/stub/vortex.cpp:135  -> H2D log line
  vortex/runtime/stub/vortex.cpp:140  -> D2H log line
  vortex/runtime/stub/vortex.cpp:156  -> "Jigsaw Computation Performance:" header

Outputs:
  - {trace_dir}/<app>.trace    : human-readable bundle trace.
  - traces.h   (kernel C)      : self-contained header for my_qemu_edu.c.
  - traces.hpp (C++)           : self-contained header for sw_no_vm replays.
"""

import argparse
import os
import re
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_LOG_DIR = os.path.join(REPO_ROOT, "vortex", "logs")
DEFAULT_TRACE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "traces")

H2D_RE = re.compile(r"Jigsaw Direction:\s*H2D,\s*Transfer Size:\s*([0-9a-fA-F]+)\s*bytes")
D2H_RE = re.compile(r"Jigsaw Direction:\s*D2H,\s*Transfer Size:\s*([0-9a-fA-F]+)\s*bytes")
HDR_RE = re.compile(r"^Jigsaw Computation Performance:\s*$")
PERF_AGG_RE = re.compile(r"^PERF:\s*instrs=(\d+),\s*cycles=(\d+),\s*IPC=")

KIND_BULK_H2D = 0
KIND_BULK_D2H = 1
KIND_BUNDLE   = 2


def parse_size(tok: str) -> int:
    try:
        return int(tok, 10)
    except ValueError:
        return int(tok, 16)


def extract_raw(log_path: str):
    """Return ordered list of raw events: (H2D, size) | (D2H, size) | (COMPUTE, cycles)."""
    events = []
    in_perf_block = False
    with open(log_path) as f:
        for raw in f:
            line = raw.rstrip("\n")
            if HDR_RE.match(line):
                in_perf_block = True
                continue
            if in_perf_block:
                if line.startswith("PERF:") and "core" not in line:
                    m = PERF_AGG_RE.match(line)
                    if m:
                        events.append(("COMPUTE", int(m.group(2))))
                    in_perf_block = False
                    continue
                if line.startswith("PERF:"):
                    continue
                in_perf_block = False
            m = H2D_RE.search(line)
            if m:
                events.append(("H2D", parse_size(m.group(1))))
                continue
            m = D2H_RE.search(line)
            if m:
                events.append(("D2H", parse_size(m.group(1))))
                continue
    return events


def bundle(events, threshold: int):
    """Fold raw events into BULK_*/BUNDLE entries.

    Returns a list of dicts, each with keys:
      kind          : KIND_BULK_H2D | KIND_BULK_D2H | KIND_BUNDLE
      h2d_size      : bytes (BULK_H2D + BUNDLE)
      d2h_size      : bytes (BULK_D2H + BUNDLE)
      cycles        : (BUNDLE)
      original_count: # of raw events folded
    """
    out = []
    pending_h2d = 0
    pending_h2d_count = 0
    n = len(events)
    i = 0
    while i < n:
        kind, val = events[i]

        if kind == "H2D":
            if val <= threshold:
                pending_h2d += val
                pending_h2d_count += 1
                i += 1
                continue
            # Large H2D: emit as standalone BULK_H2D. Pending small H2Ds
            # carry forward so they can fold into the next bundle (this
            # keeps e.g. the kernel binary separate from the kernel-arg
            # blob that precedes it in the trace, even though the trace
            # ordering listed args first).
            out.append(dict(kind=KIND_BULK_H2D, h2d_size=val, d2h_size=0,
                            cycles=0, original_count=1))
            i += 1
            continue

        if kind == "D2H":
            # Pre-compute D2H (rare): emit pending H2Ds as bulk first if any,
            # then this D2H as bulk.
            if pending_h2d:
                out.append(dict(kind=KIND_BULK_H2D, h2d_size=pending_h2d, d2h_size=0,
                                cycles=0, original_count=pending_h2d_count))
                pending_h2d = 0
                pending_h2d_count = 0
            # Could be small or large; either way, no compute to fold into.
            out.append(dict(kind=KIND_BULK_D2H, h2d_size=0, d2h_size=val,
                            cycles=0, original_count=1))
            i += 1
            continue

        if kind == "COMPUTE":
            cycles = val
            bundle_h2d = pending_h2d
            bundle_count = pending_h2d_count + 1  # +1 for the COMPUTE itself
            pending_h2d = 0
            pending_h2d_count = 0
            # Look forward: fold small D2Hs into the bundle until a non-small-D2H.
            bundle_d2h = 0
            j = i + 1
            while j < n:
                nk, nv = events[j]
                if nk == "D2H" and nv <= threshold:
                    bundle_d2h += nv
                    bundle_count += 1
                    j += 1
                else:
                    break
            out.append(dict(kind=KIND_BUNDLE, h2d_size=bundle_h2d, d2h_size=bundle_d2h,
                            cycles=cycles, original_count=bundle_count))
            i = j
            continue

        raise ValueError(f"unknown event kind: {kind}")

    # Flush trailing small H2Ds (no compute followed): emit as bulk H2D.
    if pending_h2d:
        out.append(dict(kind=KIND_BULK_H2D, h2d_size=pending_h2d, d2h_size=0,
                        cycles=0, original_count=pending_h2d_count))

    return out


def write_text_trace(out_path: str, bundles):
    with open(out_path, "w") as f:
        for b in bundles:
            if b["kind"] == KIND_BULK_H2D:
                f.write(f"BULK_H2D,{b['h2d_size']},{b['original_count']}\n")
            elif b["kind"] == KIND_BULK_D2H:
                f.write(f"BULK_D2H,{b['d2h_size']},{b['original_count']}\n")
            elif b["kind"] == KIND_BUNDLE:
                f.write(f"BUNDLE,{b['h2d_size']},{b['d2h_size']},{b['cycles']},{b['original_count']}\n")


def emit_header(out_path: str, app_bundles: dict, mode: str, threshold: int):
    """Emit a self-contained header with embedded bundle data.

    mode='c'   -> kernel header (uses linux/types.h, u8/u64).
    mode='cpp' -> C++ header (uses cstdint, uint8_t/uint64_t).
    """
    assert mode in ("c", "cpp")

    # Compute max sizes for buffer sizing on the consumer side.
    max_bytes = 0
    for bundles in app_bundles.values():
        for b in bundles:
            for f in ("h2d_size", "d2h_size"):
                if b[f] > max_bytes:
                    max_bytes = b[f]

    if mode == "c":
        type_u8, type_u64, type_int = "u8", "u64", "int"
        types_include = "#include <linux/types.h>\n#include <linux/kernel.h>  /* ARRAY_SIZE */"
        guard = "JIGSAW_TRACES_H"
        nspace_open, nspace_close = "", ""
    else:
        type_u8, type_u64, type_int = "uint8_t", "uint64_t", "int"
        types_include = "#include <cstdint>\n#include <cstddef>"
        guard = "JIGSAW_TRACES_HPP"
        nspace_open, nspace_close = "namespace jigsaw_traces_ns {\n", "} // namespace jigsaw_traces_ns\n"

    L = []
    L.append("/* AUTO-GENERATED by extract_traces.py. Do not edit. */")
    L.append("/* Include from exactly one translation unit. */")
    L.append(f"#ifndef {guard}")
    L.append(f"#define {guard}")
    L.append("")
    L.append(types_include)
    L.append("")
    L.append("enum trace_kind {")
    L.append(f"    TRACE_BULK_H2D = {KIND_BULK_H2D},")
    L.append(f"    TRACE_BULK_D2H = {KIND_BULK_D2H},")
    L.append(f"    TRACE_BUNDLE   = {KIND_BUNDLE},")
    L.append("};")
    L.append("")
    L.append("struct trace_event {")
    L.append(f"    {type_u8}  kind;")
    L.append(f"    {type_u64} h2d_size;       /* bytes; BULK_H2D, BUNDLE */")
    L.append(f"    {type_u64} d2h_size;       /* bytes; BULK_D2H, BUNDLE */")
    L.append(f"    {type_u64} cycles;         /* BUNDLE only */")
    L.append(f"    {type_int} original_count; /* raw events folded into this entry */")
    L.append("};")
    L.append("")
    L.append(f"#define JIGSAW_TRACE_MAX_BYTES {max_bytes}ULL")
    L.append(f"#define JIGSAW_BUNDLE_THRESHOLD {threshold}ULL  /* small <=> <= this */")
    L.append("")

    if nspace_open:
        L.append(nspace_open)

    for app in sorted(app_bundles):
        bundles = app_bundles[app]
        L.append(f"static const struct trace_event tr_{app}[] = {{")
        for b in bundles:
            L.append(f"    {{ {b['kind']}, {b['h2d_size']}ULL, {b['d2h_size']}ULL, {b['cycles']}ULL, {b['original_count']} }},")
        L.append("};")
        L.append("")

    L.append("struct trace_app {")
    L.append("    const char *name;")
    L.append("    const struct trace_event *events;")
    if mode == "c":
        L.append("    size_t n;")
    else:
        L.append("    std::size_t n;")
    L.append("};")
    L.append("")
    L.append("static const struct trace_app jigsaw_traces[] = {")
    for app in sorted(app_bundles):
        L.append(f'    {{ "{app}", tr_{app}, ARRAY_SIZE(tr_{app}) }},' if mode == "c"
                 else f'    {{ "{app}", tr_{app}, sizeof(tr_{app})/sizeof(tr_{app}[0]) }},')
    L.append("};")
    L.append("")

    if nspace_close:
        L.append(nspace_close)

    L.append(f"#endif /* {guard} */")

    with open(out_path, "w") as f:
        f.write("\n".join(L) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log-dir", default=DEFAULT_LOG_DIR)
    ap.add_argument("--trace-dir", default=DEFAULT_TRACE_DIR)
    ap.add_argument("--header-c", help="Emit kernel C header at this path (e.g. traces.h)")
    ap.add_argument("--header-cpp", action="append", default=[],
                    help="Emit C++ header at this path. Repeat for multiple destinations.")
    ap.add_argument("--threshold", type=int, default=256,
                    help="Bundle threshold in bytes (small DMAs fold into bundles)")
    args = ap.parse_args()

    os.makedirs(args.trace_dir, exist_ok=True)
    logs = sorted(p for p in os.listdir(args.log_dir) if p.endswith(".log"))
    summary = []
    app_bundles = {}
    for log in logs:
        app = os.path.splitext(log)[0]
        raw = extract_raw(os.path.join(args.log_dir, log))
        bundles = bundle(raw, args.threshold)
        app_bundles[app] = bundles
        write_text_trace(os.path.join(args.trace_dir, f"{app}.trace"), bundles)

        n_bulk_h2d = sum(1 for b in bundles if b["kind"] == KIND_BULK_H2D)
        n_bulk_d2h = sum(1 for b in bundles if b["kind"] == KIND_BULK_D2H)
        n_bundle   = sum(1 for b in bundles if b["kind"] == KIND_BUNDLE)
        max_xfer = max(
            (b["h2d_size"] for b in bundles if b["kind"] != KIND_BULK_D2H),
            default=0)
        max_xfer = max(max_xfer,
            max((b["d2h_size"] for b in bundles if b["kind"] != KIND_BULK_H2D), default=0))
        summary.append((app, len(bundles), n_bulk_h2d, n_bulk_d2h, n_bundle, max_xfer))

    if args.header_c:
        emit_header(args.header_c, app_bundles, "c", args.threshold)
        print(f"Kernel header written to {args.header_c}")
    for cpp_path in args.header_cpp:
        emit_header(cpp_path, app_bundles, "cpp", args.threshold)
        print(f"C++ header written to {cpp_path}")

    print(f"\n{'app':<14}{'entries':>8}{'BULK_H2D':>10}{'BULK_D2H':>10}{'BUNDLE':>8}{'max_xfer':>11}")
    for row in summary:
        print(f"{row[0]:<14}{row[1]:>8}{row[2]:>10}{row[3]:>10}{row[4]:>8}{row[5]:>11}")
    print(f"\nTraces written to {args.trace_dir}/  (threshold={args.threshold} B)")


if __name__ == "__main__":
    sys.exit(main())
