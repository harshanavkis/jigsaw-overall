# jigsaw-overall

## Setting up packages (Only done once)
```
direnv allow
```

## Building a bootable image

- follow the instructions here: https://nixos.wiki/wiki/Kernel_Debugging_with_QEMU

## Setup environment variables
```
export PRJ_DIR=$PWD
```

## Clone repositories
```
git clone --recursive https://github.com/harshanavkis/spdm-linux.git
git clone --recursive https://github.com/harshanavkis/multiproc-qemu.git
```

## Building multiproc qemu

```
cd $PRJ_DIR/multiproc-qemu
git checkout sec-disagg
mkdir build
cd build
../configure --target-list=x86_64-softmmu --enable-kvm --enable-vfio-user-server --enable-vfio-user-client --disable-werror
make -j$(nproc)
```

## Building linux

```
cd $PRJ_DIR/spdm-linux
git checkout disagg-shmem
cp linux-disagg-shmem-config .config
make olddefconfig
make -j$(nproc)
```

## Running the setup

### Start the device emulation first

```
cd $PRJ_DIR
./dev-proc
```

### Start the VM in a different window

```
./qemu-run.sh $PRJ_DIR/spdm-linux <PATH_TO_VM_IMG>
```
