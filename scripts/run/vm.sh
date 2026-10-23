#!/usr/bin/env bash

if [ $# -lt 4 ]; then
	echo "Error: missing command line arguments"
	echo "Usage: ./vm.sh <VM type: [CVM|VM]> <Path of VM image> <Path of OVMF.fd> <Log name>"
	exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

KERNEL_BIN="$SRC_DIR/submodules/spdm-linux/arch/x86/boot/bzImage"
QEMU_BIN="$SRC_DIR/submodules/qemu/build/qemu-system-x86_64"

# The QEMU submodule is built inside this repo's nix-shell, whose nixpkgs is
# unpinned: the store paths baked into the binary's RUNPATH can vanish on a
# channel update or nix GC, after which the loader reports e.g.
# "libudev.so.1: cannot open shared object file". Resolve any now-missing
# runtime libs from whatever copy the store currently has — the loader only
# needs the sonames present (the features they back are unused in these VMs).
missing=$(ldd "$QEMU_BIN" 2>/dev/null | awk '/not found/ {print $1}')
if [ -n "$missing" ]; then
	libpath=""
	for lib in $missing; do
		found=$(find /nix/store -mindepth 3 -maxdepth 3 -name "$lib" -print -quit 2>/dev/null)
		if [ -z "$found" ]; then
			echo "Error: cannot resolve $lib for $QEMU_BIN (searched /nix/store)"
			exit 1
		fi
		libpath="$libpath$(dirname "$found"):"
	done
	export LD_LIBRARY_PATH="$libpath$LD_LIBRARY_PATH"
	echo "Resolved missing QEMU libs via LD_LIBRARY_PATH: $libpath"
fi

case $1 in
	CVM)
		$QEMU_BIN \
			-m 8G \
			-object memory-backend-memfd,id=sysmem-file,size=8G \
			-numa node,memdev=sysmem-file \
			-smp 8 \
			-kernel $KERNEL_BIN \
			-append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr log_buf_len=16M" \
			-drive file=$2,format=raw \
			-object memory-backend-file,size=1M,share=on,mem-path=/dev/shm/ivshmem,id=hostmem \
			-device ivshmem-plain,memdev=hostmem \
			-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
			-net nic,model=e1000 \
			-enable-kvm \
			-nographic \
			-cpu EPYC-v4,+vpclmulqdq \
			-no-reboot \
			-machine q35,confidential-guest-support=sev0 \
			-bios $3 \
			-device disagg-fake-pci,bar-size=1048576 \
			-object sev-snp-guest,id=sev0,cbitpos=51,reduced-phys-bits=1,policy=0x30000 \
			2>&1 | tee "$4"
		;;
	VM)
		numactl --cpunodebind=1 --membind=1 taskset -c 38-45 $QEMU_BIN \
			-m 8G \
			-object memory-backend-memfd,id=sysmem-file,size=8G,host-nodes=1,policy=bind,hugetlb=on,hugetlbsize=2M \
			-numa node,memdev=sysmem-file \
			-smp 8 \
			-kernel $KERNEL_BIN \
			-append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr swiotlb=noforce iommu=off log_buf_len=16M" \
			-drive file=$2,format=raw \
			-object memory-backend-file,size=16M,share=on,mem-path=/dev/hugepages/ivshmem,id=hostmem,host-nodes=1,policy=bind,prealloc=on \
			-device ivshmem-plain,memdev=hostmem \
			-enable-kvm \
			-nographic \
			-cpu EPYC-v4,+vpclmulqdq,+invtsc \
			-no-reboot \
			-machine q35,accel=kvm,kernel_irqchip=on \
			-device disagg-fake-pci,bar-size=2097152 \
			2>&1 | tee "$4"
		;;
	*)
		echo "Error: Invalid option for VM type"
		echo "Usage: ./vm.sh <VM type: [CVM|VM]> <Path of VM image> <Path of OVMF.fd> <Log name>"
		exit 1
		;;
esac


