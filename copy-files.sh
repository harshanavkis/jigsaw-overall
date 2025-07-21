#!/usr/bin/env bash

linux_dir="./spdm-linux"
image=$1
qemu_dir="$linux_dir/qemu_edu"

to_copy="$qemu_dir/qemu_edu.ko \
    $qemu_dir/my_qemu_edu.ko \
    $qemu_dir/disagg_benchmark.ko"

nix-shell -p gnumake --run "make -C $qemu_dir"
mkdir mount-point
sudo mount -o loop $image mount-point
sudo cp $to_copy mount-point/modules/
sudo umount mount-point
rm -rdf mount-point

