#!/bin/sh

FILE="ku_ipc"
MAJOR=$(awk "\$2==\"$FILE\" {print \$1}" /proc/devices)

mknod /dev/$FILE c $MAJOR 0
