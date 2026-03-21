#!/usr/bin/env bash

if [ $# -lt 1 ]; then
    echo "Error: missing command line argument for $0"
    echo "Usage: $0 <Coyote example directory name (e.g. jigsaw_host_controller)> [force_rebuild]?"
    exit 1
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

COYOTE_SW_DIR="$SRC_DIR/submodules/Coyote/examples/$1/sw"
BIN_NAME="test"

build_coyote_sw() {
    (
        cd $COYOTE_SW_DIR
        mkdir -p build
        cd build
        cmake ..
        make
    )
}

if [ ! -x "$COYOTE_SW_DIR/build/$BIN_NAME" ]; then
    build_coyote_sw
else
    if [ $# -ge 2 ]; then
        if [ "$2" = "force_rebuild" ]; then
            build_coyote_sw
            exit 0
        fi
    fi

    echo "Target for $COYOTE_SW_DIR does already exist. Skipping."
fi

