#!/usr/bin/env bash

if [ $# -lt 1 ]; then
    echo "Error: missing command line argument for $0"
    echo "Usage: $0 <Coyote example directory name (e.g. jigsaw_host_controller)> [force_rebuild]?"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

COYOTE_HW_DIR="$SRC_DIR/submodules/Coyote/examples/$1/hw"

build_coyote_hw() {
    (
     xilinx-shell <<EOF
        cd $COYOTE_HW_DIR
        mkdir build
        cd build
        /usr/bin/cmake .. -DFDEV_NAME=u280
        make project && make bitgen
EOF
    )
}

if [ ! -f "$COYOTE_HW_DIR/build/bitstreams/cyt_top.bit" ]; then
    build_coyote_hw
else
    if [ $# -ge 2 ]; then
        if [ "$2" = "force_rebuild" ]; then
            build_coyote_hw
            exit 0
        fi
    fi

    echo "Bitstream does already exist. Skipping!"
fi

