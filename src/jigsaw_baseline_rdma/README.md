# Baseline over RDMA

This setup runs the Coyote/jigsaw_baseline_rdma/sw software and the client connection in this subdirectory to do baseline tests with the hardware in Coyote/jigsaw_baseline/hw.

It runs the Linux guest kernel within a qemu instance, exposes the shmem and communicates with this "proxy" rdma client with the remote fpga.

Note: This setup does not involve a RDMA connection handled within the Coyote hw, instead this is implemented with the normal RDMA CPU API.

## Building

The builds for this setup (over two servers) include:

1. SW RDMA client (subdirectory rdma_client)
2. SW Coyote including the RDMA server (submodules/Coyote/examples/jigsaw_baseline_rdma/sw
3. HW Coyote bitstream (submodules/Coyote/jigsaw_baseline/hw)
4. Kernel (branch ```baseline```)
5. QEMU (branch ```disagg_fake_device```)

These steps are combined in a single interactive build script.
This script splits the steps into two subcommands, one for the device (Coyote HW + RDMA Server Coyote SW) and on for the host (RDMA Client SW + Kernel + QEMU).

```./build.sh```

## Running

Similiar to the split into host and device in the build script, this running script does the same distinction and runs the necessary binaries.
