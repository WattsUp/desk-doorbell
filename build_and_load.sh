#!/bin/sh

set -e

mkdir -p build
cd build
cmake -G Ninja ..
ninja

echo "Rebooting into BOOTSEL"
/mnt/c/picotool.exe reboot -f -u
sleep 1
echo "Loading"
/mnt/c/picotool.exe load DeskDoorbell.uf2
sleep 0.1
echo "Rebooting"
/mnt/c/picotool.exe reboot
