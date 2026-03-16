# Fix for Coyote driver build failure on TUM DSE cluster

Usually the kernel headers for the system's current running kernel, can be found in ```/lib/modules/$(shell uname -r)/build```. Sometimes, e.g. on NixOS, those headers might not be located there.

This file suggests an alternitave approach when running the system on the TUM DSE cluster.

You will need to clone the config for the cluster and change the driver Makefile in Coyote.

## Get Kernel headers via Git

```shell
git clone --recursive https://github.com/TUM-DSE/doctor-cluster-config.git
cd doctor-cluster-config
nix build <PREPEND_FULL_PATH>/doctor-cluster-config#nixosConfigurations.<clara or amy>.config.boot.kernelPackages.kernel.dev
``` 

Those commands will create a result-dev folder which contains the headers.

## Change the Makefile

The value of ```KERNELDIR``` in ```jigsaw-overall/submodules/Coyote/driver/Makefile``` has to be changed like this:

```shell
KERNELDIR ?= <PREPEND_FULL_PATH>/doctor-cluster-config/result-dev/lib/modules/<KERNEL-Version>/build
```

