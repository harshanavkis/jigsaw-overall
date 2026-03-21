# Baseline

This setup runs the Coyote/jigsaw_baseline/sw software and the hardware in Coyote/jigsaw_baseline/hw.

It runs the Linux guest kernel within a qemu instance, exposes the shmem and communicates.

## Building

The builds for this setup (over two servers) include:

1. Kernel (branch ```jigsaw```)
2. Kernel modules
3. QEMU (branch ```disagg_fake_device```)
4. Coyote driver
5. Coyote SW including the shared memory (submodules/Coyote/examples/jigsaw_baseline/sw)
6. Coyote HW bitstream (submodules/Coyote/jigsaw_baseline/hw)

These steps are combined in a single build script.

Running following commands does build **all** components of host device.

```./build.py``` to build the device side components.

Note: You can also build only some of the components. See ```./build.py --help``` for more information.

## Running

The run script splits into ```proxy``` and ```vm``` commands.

### Proxy 

```sudo ./run.py proxy```

#### Cleanup

After running your application, you have to remove the driver. This can be done via running ```./run.py proxy --remove_driver``` 

### VM

```./run.py vm <Path of VM image> <Path of OVMF.fd>```

