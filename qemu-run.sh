# # QEMU with NVMe SPDM tests
# ./spdm-qemu/build/qemu-system-x86_64 \
#         -m 4G \
#         -smp 8 \
#         -kernel $1/arch/x86/boot/bzImage \
#         -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
#         -drive file=$2/qemu-image.img,format=raw \
# 	-drive file=$2/blknvme,if=none,id=mynvme,format=raw \
# 	-device nvme,drive=mynvme,serial=deadbeef,spdm=2323 \
# 	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
#         -net nic,model=e1000 \
#         -enable-kvm \
#         -nographic \
#         -pidfile vm.pid \
# 	-machine q35 \
#         2>&1 | tee vm.log

# # QEMU with only EDU 
# ./spdm-qemu/build/qemu-system-x86_64 \
#         -m 4G \
#         -smp 8 \
#         -kernel $1/arch/x86/boot/bzImage \
#         -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
#         -drive file=$2/qemu-image.img,format=raw \
#         -device edu,spdm=2323 \
# 	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
#         -net nic,model=e1000 \
#         -enable-kvm \
#         -nographic \
#         -pidfile vm.pid \
# 	-machine q35 \
#         2>&1 | tee vm.log

# # QEMU with only EDU and vsock
# ./spdm-qemu/build/qemu-system-x86_64 \
#         -m 4G \
#         -smp 8 \
#         -kernel $1/arch/x86/boot/bzImage \
#         -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
#         -drive file=$2/qemu-image.img,format=raw \
#         -device edu,spdm=2323 \
#         -device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=3 \
# 	    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
#         -net nic,model=e1000 \
#         -enable-kvm \
#         -nographic \
#         -pidfile vm.pid \
# 	    -machine q35 \
#         2>&1 | tee vm.log

# QEMU main branch with only vfio-user EDU (no SPDM) and vsock
./multiproc-qemu/build/qemu-system-x86_64 \
        -m 2G \
        -object memory-backend-memfd,id=sysmem-file,size=2G \
        -numa node,memdev=sysmem-file \
        -smp 8 \
        -kernel $1/arch/x86/boot/bzImage \
        -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
        -drive file=$2/qemu-image.img,format=raw \
        -object memory-backend-file,size=1M,share=on,mem-path=/dev/shm/ivshmem,id=hostmem \
        -device ivshmem-plain,memdev=hostmem \
        -device vfio-user-pci,socket=/tmp/remotesock \
        -device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=3 \
	    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
        -net nic,model=e1000 \
        -enable-kvm \
        -nographic \
        -pidfile vm.pid \
	-machine q35 \
        2>&1 | tee vm.log
