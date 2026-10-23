# Trace Replay Notes

`scripts/extract_traces.py` generates bundled replay traces from
`vortex/logs/*.log`.

Generated outputs:

- `scripts/traces/*.trace`: human-readable traces.
- `submodules/spdm-linux/qemu_edu/traces.h`: kernel replay header.
- `coyote-stuff/Coyote/examples/*/sw_no_vm/src/traces.hpp`: C++ replay headers.

The current host-controller trace set has a maximum single DMA payload of
`14550656` bytes, so VM/proxy shared memory must be sized large enough for
that payload plus the 4 KiB DMA offset used by the replay path.

Useful VM replay output:

- `TRACE_EVENT: app,run,event,kind,h2d_bytes,d2h_bytes,cycles`
- `TRACE_SUMMARY: app,run,n_events,n_bulk_h2d,n_bulk_d2h,n_bundle,total_h2d_ns,total_d2h_ns,total_bundle_ns,total_ns,total_h2d_bytes,total_d2h_bytes,total_cycles`
