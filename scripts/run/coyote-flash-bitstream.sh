#!/usr/bin/env bash

if [ $# -lt 1 ]; then
    echo "Error: missing command line argument for $0"
    echo "Usage: $0 <Coyote example directory name (e.g. jigsaw_host_controller)>"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

COYOTE_DIR="$SRC_DIR/submodules/Coyote"
COYOTE_HW_DIR="$SRC_DIR/submodules/Coyote/examples/$1/hw"

flash_bitstream() {
    (
        xilinx-shell
        vivado -mode batch -nolog -nojournal -source $COYOTE_DIR/program_fpga.tcl -tclargs $COYOTE_HW_DIR/build/bitstreams/cyt_top.bit
    )
}

if [ ! -f "$COYOTE_HW_DIR/build/bitstreams/cyt_top.bit" ]; then
    echo "Error: Bitstream for $1 does not exist"
    exit 1
else
    flash_bitstream
fi

