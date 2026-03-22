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

These steps are combined in a single build script.

This script splits the steps into two subcommands, one for the device (Coyote HW + RDMA Server Coyote SW) and on for the host (RDMA Client SW + Kernel + QEMU).

Running following commands does build **all** components of host device.

```./build.py device``` to build the device side components.

```./build.py host <Path of VM image>``` to build the host side components.

Note: You can also build only some of the components. See ```./build.py [host|device] --help``` for more information.

## Running

Similiar to the split into host and device in the build script, this running script does the same distinction and runs the necessary binaries.

### Device

```./run.py device```

#### Cleanup

After running your application, you have to remove the driver. This can be done via running ```./run.py device --remove_driver``` 


### Host

The host has to different components: The QEMU VM and the proxy/shmem application.

This distinction is split into two different subcommands.

#### VM

```./run.py host VM <Path of VM image> <Path of OVMF.fd>```

#### Proxy

```./run.py host proxy```

**Note:** Running the above command will fail, it says to specify a IP address and a port. This can be done like this: ```./run.py host proxy -- -a <IP> -p <Port>. (The -- is important)

