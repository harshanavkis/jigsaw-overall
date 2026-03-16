#!/usr/bin/env bash
set -e

if [ $# -lt 1 ]; then
    echo "Error: missing command line argument for $0"
    echo "Usage: $0 [device|host|single]"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

if [ "$1" = "device" ]; then

    echo "Building Coyote driver:"
    $SRC_DIR/scripts/build/coyote-driver.sh

    echo "Building Coyote software:"
    $SRC_DIR/scripts/build/coyote-sw.sh jigsaw_baseline_rdma

    echo "Generating bitstream:"
    # It is not an error to specify 'jigsaw_baselin' here.
    # This setup uses the same bitstream, and does not provide one itself.
    #$SRC_DIR/scripts/build/coyote-hw.sh jigsaw_baseline

elif [ "$1" = "host" ]; then

    if [ $# -lt 2 ]; then
        echo "Error: missing command line argument for $0 with option host"
        echo "Usage: $0 host <Path of VM image>"
        exit 1
    fi

    echo "Building kernel:"
    $SRC_DIR/scripts/build/kernel.sh baseline

    echo "Building and copying kernel modules:"
    $SRC_DIR/scripts/build/build_and_copy_kernel_mods.sh $2

    echo "Building QEMU:"
    $SRC_DIR/scripts/build/qemu.sh

    echo "Building RDMA client:"
    (
        cd $SCRIPT_DIR/rdma_client
        make
    )

elif [ "$1" = "single" ]; then
    echo "What should be built?"
    echo "(Builds the target even, when it already exists)"
    echo "1. Kernel (host)"
    echo "2. Kernel modules (host)"
    echo "3. QEMU (host)"
    echo "4. RDMA SW client; shmem; proxy (host)"
    echo "5. Coyote SW including the RDMA server; proxy (device)"
    echo "6. Coyote driver (device)"
    echo "7. Coyote HW bitstream (device)"
    read -p "> " choice

    case "$choice" in
        1)
            echo "Building kernel:"
            $SRC_DIR/scripts/build/kernel.sh baseline force_rebuild
            ;;
        2)
            read -p "Path of VM image: " image_path 
            echo "Building and copying kernel modules:"
            $SRC_DIR/scripts/build/build_and_copy_kernel_mods.sh $image_path
            ;;
        3)
            echo "Building QEMU:"
            $SRC_DIR/scripts/build/qemu.sh force_rebuild
            ;;
        4)
            echo "Building RDMA client:"
            (
                cd $SCRIPT_DIR/rdma_client
                make
            )
            ;;
        5)
            echo "Building Coyote software:"
            $SRC_DIR/scripts/build/coyote-sw.sh jigsaw_baseline_rdma force_rebuild
            ;;
        6)
            echo "Building Coyote driver:"
            $SRC_DIR/scripts/build/coyote-driver.sh
            ;;
        7)
            echo "Generating bitstream:"
            # It is not an error to specify 'jigsaw_baselin' here.
            # This setup uses the same bitstream, and does not provide one itself.
            $SRC_DIR/scripts/build/coyote-hw.sh jigsaw_baseline force_rebuild
            ;;
        *)
            echo "Invalid option"
            ;;
    esac

else
    echo "Wrong argument for type of build"
    echo "Usage: $0 [device|host|single]"
    exit 1
fi

