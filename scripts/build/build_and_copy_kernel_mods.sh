#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Missing image name."
    echo "Usage: $0 <Path of image>"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

linux_dir="$SRC_DIR/submodules/spdm-linux"
image=$1
modules_dir="$linux_dir/qemu_edu"

mkdir mount-point
sudo mount -o loop $image mount-point
sudo mkdir -p mount-point/modules
sudo cp $modules_dir/*.ko mount-point/modules/
sudo umount mount-point
rm -rdf mount-point

