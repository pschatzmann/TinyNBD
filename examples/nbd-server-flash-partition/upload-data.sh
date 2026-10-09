#!/bin/bash
# Builds a LittleFS image from the data folder and writes it to the "littlefs"
# partition (see partitions.csv). Usage: ./upload-data.sh [serial-port]
set -e
PORT=${1:-/dev/ttyUSB0}
OFFSET=0x290000  # offset of the littlefs partition in partitions.csv
SIZE=0x160000    # size of the littlefs partition in partitions.csv
DIR=$(cd "$(dirname "$0")" && pwd)
TOOLS=~/.arduino15/packages/esp32/tools
MKLITTLEFS=$(ls -d $TOOLS/mklittlefs/*/mklittlefs | sort -V | tail -1)
ESPTOOL=$(ls -d $TOOLS/esptool_py/*/esptool | sort -V | tail -1)
IMAGE=$(mktemp --suffix=.bin)
trap 'rm -f "$IMAGE"' EXIT

"$MKLITTLEFS" -c "$DIR/data" -b 4096 -p 256 -s $SIZE "$IMAGE"
"$ESPTOOL" --port "$PORT" write-flash $OFFSET "$IMAGE"
