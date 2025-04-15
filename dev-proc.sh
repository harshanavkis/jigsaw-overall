# Emulation of device in external process
./multiproc-qemu/build/qemu-system-x86_64 \
    -machine x-remote,vfio-user=on \
    -device edu,id=edu0 \
    -nographic \
    -object x-vfio-user-server,id=vfioobj1,type=unix,path=/tmp/remotesock,device=edu0 \
    -monitor unix:/tmp/rem-sock,server,nowait
