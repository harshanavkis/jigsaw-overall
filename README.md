# jigsaw-overall

## One-time setup (for two-host version):

Following generic instructions are applicable to the rdma, tcp and ethernet version. Thus if you have already used one of those, skip the [One-time setup](#one-time-setup-for-two-host-version) part and continue at [TCP specifc setup](#tcp-specific-setup).

[One-time setup](#one-time-setup-for-two-host-version) only covers the VM-host server.

### Setup packages
```
direnv allow
``` 

### Build a bootable image:

- Follow the instructions here: https://nixos.wiki/wiki/Kernel_Debugging_with_QEMU

### Setup environment variables
```
export PRJ_DIR=$PWD
```

### Clone repositories
```
git clone --recursive https://github.com/maxjae/spdm-linux.git
git clone --recursive https://github.com/maxjae/multiproc-qemu.git
```

### Build multiproc-qemu

```
cd $PRJ_DIR/multiproc-qemu
git checkout disagg-fake-device
mkdir build
cd build
../configure --target-list=x86_64-softmmu --enable-kvm --disable-werror
make -j$(nproc)
```

### Build linux

#### Initial setup: 

```
cd $PRJ_DIR/spdm-linux
git checkout disagg-two-hosts
cp linux-disagg-shmem-config .config
make olddefconfig
```

#### Additional configuration:

```
make menuconfig
```

If wanted, select following configs with =y:

For CVM setup: 
- Processor type and features -> AMD Secure Memory Encryption (SME) support

For faster cryptography:
- Cryptographic API -> Accelerated Cryptographic Algorithms for CPU (x86) -> Ciphers: AES, modes: ECB, CBC, CTS, XTR, XTS, GCM (AES-NI)

#### Build:

```
make -j$(nproc)
```

### Building kernel modules

Compiled modules have to be copied into image.

#### 1. Option: Use provided script
(Mounts the image and just copies files to /modules/)
```
cd $PRJ_DIR
./copy-modules [Path to VM image]
```

#### 2. Option: Build modules and insert yourself
```
cd $PRJ_DIR/spdm-linux/qemu_edu
make
```
Copy *.ko files into image. 

### Optional: CVM setup

Copy OVMF files:
```
cp $(nix eval --raw nixpkgs#OVMF)/FV/OVMF_CODE.fd $PRJ_DIR/OVMF_CODE.fd
cp $(nix eval --raw nixpkgs#OVMF)/FV/OVMF_VARS.fd $PRJ_DIR/OVMF_VARS.fd
```

## TCP specific setup

Following instructions are only valid for the TCP version.

#### On VM-Host server

Build the proxy:

```
cd $PRJ_DIR/src/proxy
make
```

#### On other server

Build device emulation:

```
cd $PRJ_DIR/src/edu_simple
make
```

## Run the TCP setup

The process on the device side has to be started first.

### Device emulation (e.g. wilfred)

```
cd $PRJ_DIR/src/edu_simple
./bin/edu_device
        --localAddr [IP address of local interface]
        --localPort [Port to use]
```

### VM-Host (e.g. amy)

#### Proxy:
```
cd $PRJ_DIR/src/proxy
./bin/proxy
        --remoteAddr [IP address of remote host's interface]
        --remotePort [Port of remote host]
        --localAddr [IP address of local interface]
        --localPort [Local port to use]
```
The ip address
 of the interface can be found with e.g. 
```
ip a
```

#### VM:

```
cd $PRJ_DIR
sudo ./qemu-run.sh [Path to VM image]
```