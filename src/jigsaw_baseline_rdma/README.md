# Baseline over RDMA

This setup runs the Coyote/jigsaw_baseline_rdma/sw software and the client connection in this subdirectory to do baseline tests with the hardware in Coyote/jigsaw_baseline/hw.

It runs the Linux guest kernel within a qemu instance, exposes the shmem and communicates with this "proxy" rdma client with the remote fpga.

Note: This setup does not involve a RDMA connection handled by the Coyote HW implementation, instead this is implemented with the normal RDMA CPU API.

## Building

The builds for this setup (over two servers) include:

1. Kernel (branch ```baseline```)
2. Kernel modules
3. QEMU (branch ```disagg_fake_device```)
4. RDMA SW client (subdirectory rdma_client)
5. Coyote driver
6. Coyote SW including the RDMA server (submodules/Coyote/examples/jigsaw_baseline_rdma/sw)
7. Coyote HW bitstream (submodules/Coyote/jigsaw_baseline/hw)

These steps are combined in a single interactive build script.
This script splits the steps into two subcommands, one for the device (Coyote HW + RDMA Server Coyote SW) and on for the host (RDMA Client SW + Kernel + QEMU).

```./build.sh device``` to build the device side components.

```./build.sh host <Path of VM image>``` to build the host side components.

Note: You can also run the script with ```./build.sh single``` to be able to choose one of the targets to build (This option will do force builds, so regardless if target does already exist).

## Running

Similiar to the split into host and device in the build script, this running script does the same distinction and runs the necessary binaries.
```./run.sh device``` to run the device side components.

```./run.sh host <Path of VM image> <Path of OVMF.fd>``` to run the host side components.

Important note: After running your application, you have to remove the driver. This can be done via running the ```./run.sh host``` script again and choosing the option to remove the driver.
