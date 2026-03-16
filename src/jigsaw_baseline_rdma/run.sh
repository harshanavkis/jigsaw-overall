#!/usr/bin/env bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

input_yes_no() {
    local prompt="$1"
    local answer

    read -p "$prompt [y/N] " -n 1 answer
    echo ""

    if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
        return 0
    else
        return 1
    fi
}

if [ "$1" = "device" ]; then

    if input_yes_no "Do you only want to remove the driver?"; then
        echo "Removing driver:"
        $SRC_DIR/scripts/run/teardown_coyote.sh
        exit 0
    fi

    if input_yes_no "Do you want to flash the bitstream?"; then
        echo "Flashing bitstream:"
        $SRC_DIR/scripts/run/coyote-flash-bitstream.sh
    fi

    if input_yes_no "Do you want to insert the driver?"; then
        echo "Inserting driver:"
        $SRC_DIR/scripts/run/setup_coyote.sh
    fi

    echo "Running device software:"
    $SRC_DIR/submodules/Coyote/examples/jigsaw_baseline_rdma/sw/build/bin/test

elif [ "$1" = "host" ]; then

    if [ $# -lt 3 ]; then
        echo "Error: missing command line argument for $0 and option host"
        echo "Usage: $0 host <Path of VM image> <Path of OVMF.fd>"
        exit 1
    fi


    if input_yes_no "Do you want to start the proxy application?"; then
        $SCRIPT_DIR/rdma_client/bin/proxy 
        exit 0
    fi

    if input_yes_no "Do you want to start the VM?"; then
        $SRC_DIR/scripts/run/vm.sh VM $2 $3
        exit 0
    fi

else
    echo "Wrong argument for type of build"
    echo "Usage: $0 [device|host]"
    exit 1
fi

