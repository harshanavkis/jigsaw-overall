# QEMU main branch with only vfio-user EDU (no SPDM) and vsock
./multiproc-qemu/build/qemu-system-x86_64 \
        -m 2G \
        -object memory-backend-memfd,id=sysmem-file,size=2G \
        -numa node,memdev=sysmem-file \
        -smp 8 \
        -kernel ./spdm-linux/arch/x86/boot/bzImage \
        -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
        -drive file=./qemu-image.img,format=raw \
        -object memory-backend-file,size=1M,share=on,mem-path=/dev/shm/ivshmem,id=hostmem \
	-device ivshmem-plain,memdev=hostmem \
	    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
        -net nic,model=e1000 \
        -enable-kvm \
        -nographic \
	-cpu host \
	-drive if=pflash,format=raw,unit=0,readonly=on,file=OVMF_CODE.fd \
	-drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd \
	-machine q35,confidential-guest-support=sev0 \
	-object sev-guest,id=sev0,cbitpos=51,reduced-phys-bits=4,policy=0x0 \
	-d guest_errors,cpu_reset -D qemu.log \
	-device disagg-fake-pci,bar-size=1048576 \
        2>&1 | tee vm.log


