# Jigsaw Trace Benchmark

This document describes the **trace benchmark** that runs at the end of the
three `sw_no_vm` examples under `coyote-stuff/Coyote/examples/`:

- `jigsaw_baseline/sw_no_vm`        — local FPGA, no virtualization
- `jigsaw_minus_nw/sw_no_vm`        — local FPGA, jigsaw MMIO/DMA protocol, no network
- `jigsaw_host_controller/sw_no_vm` — host↔device FPGAs, jigsaw protocol over RDMA

The same trace stream and replay logic is used in all three; only the
transport differs. The trace section is the third block of each binary,
after the DMA-only and computation benchmarks.

## What the trace benchmark does

It replays per-application event streams that were captured from real
Vortex workloads, so latencies/throughputs measured here correspond to
the kind of DMA + compute pattern those apps actually produce, rather
than synthetic sweeps over sizes.

Each application is replayed `TRACE_N_RUNS` times back-to-back (5 by
default; defined as a `static constexpr int` near the top of each
`main.cpp`). Per-event elapsed time and a small amount of metadata are
recorded for every run; afterwards a per-app summary is printed with
**one row per `(app, run)` pair** so you can see run-to-run variance
without re-running the binary.

## Where the traces come from

The embedded `traces.hpp` in each `sw_no_vm/src/` is **auto-generated**
by [`scripts/extract_traces.py`](scripts/extract_traces.py) from
`vortex/logs/<app>.log`. Each Vortex run emits lines of the form:

```
Jigsaw Direction: H2D, Transfer Size: <N> bytes
Jigsaw Direction: D2H, Transfer Size: <N> bytes
Jigsaw Computation Performance:
  PERF: instrs=..., cycles=<C>, IPC=...
```

The extractor folds adjacent **small** DMAs into the closest compute
event so the replay matches how the Jigsaw device controller actually
operates: **one MMIO write to `START_COMPUTE` triggers H2D + compute +
D2H as a fused operation.** Large standalone DMAs (above 256 B by
default) stay as standalone bulk events.

To regenerate the headers after collecting new logs:

```
python3 jigsaw-overall/scripts/extract_traces.py
```

The script writes `traces.hpp` (C++) into each `sw_no_vm/src/` directory
and a sister `traces.h` for the kernel-mode QEMU stub.

## Trace format

`traces.hpp` declares one C array per app and a `jigsaw_traces[]` table
listing them all. Each event is a `trace_event`:

```cpp
struct trace_event {
    uint8_t  kind;              // 0=BULK_H2D, 1=BULK_D2H, 2=BUNDLE
    uint64_t h2d_size;          // bytes; used by BULK_H2D and BUNDLE
    uint64_t d2h_size;          // bytes; used by BULK_D2H and BUNDLE
    uint64_t cycles;            // BUNDLE only — synthetic spin count
    int      original_count;    // raw events folded into this entry
};
```

The three event kinds:

| Kind         | Action                                                                  |
|--------------|-------------------------------------------------------------------------|
| `BULK_H2D`   | Standalone host→device DMA. Issued via `DMA_CMD = 1`.                   |
| `BULK_D2H`   | Standalone device→host DMA. Issued via `DMA_CMD = 3`.                   |
| `BUNDLE`     | H2D + busy-wait compute + D2H, kicked by `START_COMPUTE = 1`.           |

A `BUNDLE` with `d2h_size == 0` skips the D2H phase, and similarly for
`h2d_size == 0` (in correct bitstreams; see *DMA size constraints* below).

`cycles` is purely a benchmark knob — the integer number of FPGA cycles
the `computation_engine` busy-waits for to model "compute". There is no
`cycles` register on a real accelerator.

## Replay protocol

Each variant defines a `do_bulk(size, d2h)` and a
`do_bundle(h2d, d2h, cycles)` lambda inside `run_trace_benchmark`. They
each:

1. Stage the per-event device registers (addresses, lengths, status-clear).
2. Issue the kick (`DMA_CMD` for bulk, `START_COMPUTE` for bundle).
3. Poll `DMA_STATUS` for completion (`& 0x1` for bulk, `& 0x3` for bundle).

The `host_controller` variant performs each register write/read via the
**jigsaw MMIO protocol over RDMA** (a full network round-trip per CSR
access). The other two write CSRs directly over local AXI-Lite.

## Timing contract — what's inside vs outside `total_us`

This was deliberately tuned to mirror the per-event work a real device
would actually do. Outside the timed window are things that either don't
exist on a real device or are set once at thread/vFPGA acquisition.

**Outside `total_us` (NOT counted, set once at start, or per BUNDLE):**

- `MMIO_VADDR`        — set once at startup (`host_controller`, `minus_nw`)
- `COYOTE_PID`        — set once at startup (all three variants)
- `CYCLES_COMPUTE`    — synthetic; not a real-device register

**Inside `total_us` (per event, COUNTED):**

- `DMA_SRC_ADDR`, `DMA_DST_ADDR` — vary per request in real workloads
- `DMA_H2D_LEN` / `DMA_D2H_LEN`  — vary per event
- `DMA_STATUS = 0`               — clear before kick (poll-completion artifact)
- The kick MMIO (`DMA_CMD` or `START_COMPUTE`)
- The completion poll on `DMA_STATUS`

The trailing `DMA_STATUS = 0` cleanup write in `baseline` is post-clock
and not counted.

A consequence of this split: in `host_controller`, every CSR write
inside the window is an RDMA round-trip, so `total_us` for a small BULK
is dominated by the setup writes + kick + polling RTTs, not by the
actual data transfer. That's a faithful measurement of what it costs
the host to issue one event over the network.

## DMA size constraints (`clamp_dma`)

Coyote's data path is 512-bit (64-byte) AXI streams end-to-end. Lengths
that aren't multiples of a cacheline desync the meta/payload pipeline
(observed: a 74-byte H2D inside a BUNDLE wedges the device RDMA SQ on
the `host_controller` variant — the host RDMA fetches 2 cachelines
while the device DMA truncates to 1, leaving a stray beat with no
consumer).

`clamp_dma` therefore:

1. Caps the size at the available DMA buffer capacity.
2. **Rounds non-zero sizes up to the next 64-byte multiple.**
3. Keeps `0` as `0` so the device's `computation_engine` skips that phase.

If your bitstream predates the `dma_d2h_len == 0` skip in
`computation_engine.sv` (e.g.
`jigsaw_device_controller/hw/build_may3_1800`), you'll also need a
`d2h ≥ 64` floor in `do_bundle` because the older FSM unconditionally
pulses `comp_dma_start` regardless of `dma_d2h_len`. The
`host_controller` source carries that workaround; remove it once you
reflash with current sources.

## Output format

For every event, two lines are emitted to `stderr`:

```
[trace]   <app> ev=<i>/<n> kind=<KIND> h2d_raw=<N> d2h_raw=<N> cycles=<C> ... (h2d=<clamped> d2h=<clamped>) done <us> us
```

After all apps finish, a per-app summary CSV goes to `stdout` with one
row per `(app, run)`:

```
app, run, n_events, n_bulk_h2d, n_bulk_d2h, n_bundle,
total_h2d_us, total_d2h_us, total_bundle_us, total_us,
total_h2d_bytes, total_d2h_bytes, total_cycles
```

`total_us` for a `(app, run)` row is the sum of `total_us` over that
run's events. With the default `TRACE_N_RUNS = 5`, you'll get 5 rows per
app, ordered by app then by run.

## Running

Both sides need to be running for `host_controller`:

```
# device node
./jigsaw_device_controller/sw/build/test [--dump-debug]

# host node — pass the device's OOB IP for the QP exchange
./jigsaw_host_controller/sw_no_vm/build/test -i <device_ip>
```

`baseline` and `minus_nw` run as a single binary on a node with the
matching bitstream loaded:

```
./jigsaw_baseline/sw_no_vm/build/test
./jigsaw_minus_nw/sw_no_vm/build/test
```

Useful flags on `host_controller`:

- `--skip_comp` / `-s`     skip the synthetic computation-benchmark block
- `--trace_mmio`           log every jigsaw MMIO transaction
- `--dump_host_debug`      periodically dump host-side debug CSRs while polling
- `--dump-device-debug`    ask the device to dump its debug CSRs after each run
                          (requires the device side to be started with `--dump-debug`)

## Variant differences at a glance

| Variant            | Transport | MMIO cost                       | DMA path                                  |
|--------------------|-----------|---------------------------------|-------------------------------------------|
| `baseline`         | local     | direct AXI-Lite                 | local DMA                                 |
| `minus_nw`         | local     | jigsaw MMIO via local fabric    | local DMA via local fabric                |
| `host_controller`  | RDMA      | jigsaw MMIO via RDMA round-trip | DMA over RDMA (host↔device FPGA)          |

Comparing `total_us` across variants for the same trace event tells you
how much of the per-event cost is transport vs the actual transfer.
