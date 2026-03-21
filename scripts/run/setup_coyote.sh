#!/usr/bin/env bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SRC_DIR="$(realpath "$SCRIPT_DIR/../..")"
COYOTE_DIR="$SRC_DIR/submodules/Coyote"

host=`hostname`
if [[ $host == "rose" ]]; then
  BDF="c1:00.0"
else
  BDF="e1:00.0"
fi

echo 1 | sudo tee /sys/bus/pci/devices/0000:$BDF/remove
echo 1 | sudo tee /sys/bus/pci/rescan

if [[ $host == "clara" ]]; then
  echo "Installing driver for clara."
  sudo insmod $COYOTE_DIR/driver/build/coyote_driver.ko ip_addr=0x0a000002 mac_addr=000A350E24F2
elif [[ $host == "amy" ]]; then
  echo "Installing driver for amy."
  sudo insmod $COYOTE_DIR/driver/build/coyote_driver.ko ip_addr=0x0a000001 mac_addr=000A350E24D6
elif [[ $host == "rose" ]]; then
  echo "Installing driver for rose."
  sudo insmod $COYOTE_DIR/driver/build/coyote_driver.ko ip_addr=0x0a000003 mac_addr=000A350E24E6
fi
