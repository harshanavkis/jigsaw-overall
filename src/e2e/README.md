# End to End

This setup requires two hosts with an FPGA each and a RDMA capable connection between them.

## Building

The builds for this setup (over two servers) include:

1. Kernel (branch ```jigsaw```)
2. Kernel modules
3. QEMU (branch ```disagg_fake_device```)
4. Coyote driver
5. Coyote SW Device (submodules/Coyote/examples/jigsaw_device_controller/sw)
6. Coyote SW Host (submodules/Coyote/examples/jigsaw_host_controller/sw)
7. Coyote HW bitstream Device (submodules/Coyote/jigsaw_device_controller/hw)
8. Coyote HW bitstream Host (submodules/Coyote/jigsaw_host_controller/hw)

These steps are combined in a single build script.

This script splits the steps into two subcommands, one for the device (Coyote Device HW + Coyote Device SW) and on for the host (Coyote Host SW + Coyote Host SW + Kernel + QEMU).

### Host

```./build.py host <Path of VM image>``` to build the host side components.

### Device

```./build.py device``` to build the device side components.

**Note**: You can also build only some of the components. See ```./build.py [host|device] --help``` for more information.

## Running

Similiar to the split into host and device in the build script, this running script does the same distinction and runs the necessary binaries.

### Device

```./run.py device```

#### Cleanup

After running your application, you have to remove the driver. This can be done via running ```./run.py device --remove_driver``` 

### Host

The host has to different components: The QEMU VM and the host FPGA application.

This distinction is split into two different subcommands.

#### VM

```./run.py host vm <Path of VM image> <Path of OVMF.fd>```

#### Proxy

```./run.py host proxy```

**Note:** Running the above command will fail. You have to provide an ip address of the device server interface like this: ```./run.py host proxy -- -i 131.159.102.20```

