#!/usr/bin/env bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

QEMU_DIR="$SRC_DIR/submodules/qemu"
QEMU_EXPECTED_BRANCH="disagg-fake-device"

qemu_current_branch="$(git -C "$QEMU_DIR" branch --show-current)"

build_qemu() {
    (
        cd $QEMU_DIR
        git checkout $QEMU_EXPECTED_BRANCH
        mkdir -p $QEMU_DIR/build
        cd build
        ../configure --target-list=x86_64-softmmu --enable-kvm
        make -j$(nproc)
    )
}

if [ "$qemu_current_branch" = "$QEMU_EXPECTED_BRANCH" ]; then
    if [ ! -x "$QEMU_DIR/build/qemu-system-x86_64" ]; then
        build_qemu
    else
        if [ $# -ge 1 ]; then
            if [ "$1" = "force_rebuild" ]; then
                build_qemu
                exit 0
            fi
        fi

        echo "QEMU target was already built. Skipping."
    fi
else
    build_qemu
fi

