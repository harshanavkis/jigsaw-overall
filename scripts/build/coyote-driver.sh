#!/usr/bin/env bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"

COYOTE_DIR="$SRC_DIR/submodules/coyote"

if [ ! -d "/lib/modules/$(shell uname -r)/build" ]; then
    echo "Error: The kernel context in which the Coyote driver (jigsaw-overall/submodules/Coyote/driver/Makefile) is built can not be found on your system!"
    echo "If you're on the TUM LS1 cluster, try the approach described in jigsaw-overall/scripts/build/coyote-driver-dse.md."
    echo "Else, try to set the "KERNELDIR" parameter in jigsaw-overall/submodules/Coyote/driver/Makefile to your kernel header location"
    echo "After you have fixed the issure, rerun your command which triggered the error"
    exit 1
fi

(
    cd $COYOTE_DIR/driver
    make
)

