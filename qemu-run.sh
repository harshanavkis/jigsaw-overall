# QEMU main branch with only vfio-user EDU (no SPDM) and vsock
./multiproc-qemu/build/qemu-system-x86_64 \
        -m 2G \
        -object memory-backend-memfd,id=sysmem-file,size=2G \
        -numa node,memdev=sysmem-file \
        -smp 8 \
        -kernel ./spdm-linux/arch/x86/boot/bzImage \
        -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
        -drive file=$1,format=raw \
        -object memory-backend-file,size=1M,share=on,mem-path=/dev/shm/ivshmem,id=hostmem \
	-device ivshmem-plain,memdev=hostmem \
	    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
        -net nic,model=e1000 \
        -enable-kvm \
        -nographic \
	-machine q35 \
	-cpu host \
	-device disagg-fake-pci,bar-size=1048576 \
        2>&1 | tee vm.log


	#-bios seabios/out/bios.bin \
	#-chardev file,path=debug_seabios.log,id=seabios -device isa-debugcon,iobase=0x402,chardev=seabios \
