#!/usr/bin/env bash

if [ $# -lt 1 ]; then
    echo "Error: missing command line argument for $0"
    echo "Usage: $0 <kernel-branch-name> [force_rebuild]?"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

KERNEL_DIR="$SRC_DIR/submodules/spdm-linux"
KERNEL_EXPECTED_BRANCH="$1"

kernel_current_branch="$(git -C "$KERNEL_DIR" branch --show-current)"

build_kernel() {
    (
        cd $KERNEL_DIR
        git checkout $KERNEL_EXPECTED_BRANCH
        cp linux-disagg-shmem-config .config
        make olddefconfig
        make -j$(nproc)
    )
}

if [ "$kernel_current_branch" = "$KERNEL_EXPECTED_BRANCH" ]; then
    if [ ! -f "$KERNEL_DIR/arch/x86/boot/bzImage" ]; then
        build_kernel
    else
        if [ $# -ge 2 ]; then
            if [ "$2" = "force_rebuild" ]; then
                build_kernel
                exit 0
            fi
        fi

        echo "Kernel was already built. Skipping."
    fi
else
    build_kernel
fi

