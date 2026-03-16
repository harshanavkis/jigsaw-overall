#!/usr/bin/env bash

host=`hostname`
if [[ $host == "rose" ]]; then
  BDF="c1:00.0"
else
  BDF="e1:00.0"
fi

sudo rmmod coyote_driver

echo 1 | sudo tee /sys/bus/pci/devices/0000:$BDF/remove

echo 1 | sudo tee /sys/bus/pci/rescan

