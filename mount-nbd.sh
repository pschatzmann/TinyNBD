#!/usr/bin/env bash
set -euo pipefail

SERVER="192.168.1.33"
PORT="10809"
EXPORT="ram"
DEVICE="/dev/nbd0"
MOUNT_DIR="esp32-ram"

usage() {
    cat <<EOF
Usage: sudo $0 [OPTIONS]

Mount an ESP32 NBD RAM disk in the user's home directory.

Options:
  -s, --server IP       NBD server IP (default: $SERVER)
  -p, --port PORT       NBD server port (default: $PORT)
  -e, --export NAME     NBD export name (default: $EXPORT)
  -d, --device DEVICE   Local NBD device (default: $DEVICE)
  -m, --mount-dir NAME  Directory under user's home (default: $MOUNT_DIR)
  -h, --help            Show this help

Examples:
  sudo $0
  sudo $0 --server 192.168.1.33 --export ram
  sudo $0 --device /dev/nbd1 --mount-dir remote-disk

The script formats the device if no recognized filesystem is found.
Formatting erases existing data structures on the selected device.
EOF
}

while (($#)); do
    case "$1" in
        -s|--server)    SERVER="$2"; shift 2 ;;
        -p|--port)      PORT="$2"; shift 2 ;;
        -e|--export)    EXPORT="$2"; shift 2 ;;
        -d|--device)    DEVICE="$2"; shift 2 ;;
        -m|--mount-dir) MOUNT_DIR="$2"; shift 2 ;;
        -h|--help)      usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage >&2; exit 2 ;;
    esac
done

# Identify the invoking user's home and IDs.
if [[ -n "${SUDO_USER:-}" ]]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    USER_ID=$(id -u "$SUDO_USER")
    GROUP_ID=$(id -g "$SUDO_USER")
else
    USER_HOME="$HOME"
    USER_ID=$(id -u)
    GROUP_ID=$(id -g)
fi

MOUNTPOINT="$USER_HOME/$MOUNT_DIR"

if [[ $EUID -ne 0 ]]; then
    echo "Please run: sudo $0 [OPTIONS]"
    exit 1
fi

if mountpoint -q "$MOUNTPOINT"; then
    echo "Already mounted at: $MOUNTPOINT"
    exit 0
fi

modprobe nbd max_part=16
mkdir -p "$MOUNTPOINT"

# Reuse an existing connection; otherwise connect.
SIZE=$(blockdev --getsize64 "$DEVICE" 2>/dev/null || echo 0)

if [[ "$SIZE" -eq 0 ]]; then
    echo "Connecting to $SERVER:$PORT, export '$EXPORT'..."
    nbd-client "$SERVER" "$PORT" "$DEVICE" -N "$EXPORT"
fi

udevadm settle
partprobe "$DEVICE" 2>/dev/null || true
udevadm settle

SIZE=$(blockdev --getsize64 "$DEVICE")
echo "Remote disk size: $SIZE bytes"
lsblk -f "$DEVICE"

# Find a recognized filesystem on a partition.
TARGET=""
while read -r PART; do
    if [[ -n "$(blkid -o value -s TYPE "$PART" 2>/dev/null || true)" ]]; then
        TARGET="$PART"
        break
    fi
done < <(lsblk -lnpo NAME "$DEVICE" | tail -n +2)

# Otherwise check the whole device.
if [[ -z "$TARGET" ]] &&
   [[ -n "$(blkid -o value -s TYPE "$DEVICE" 2>/dev/null || true)" ]]; then
    TARGET="$DEVICE"
fi

# Format only when no recognized filesystem exists.
if [[ -z "$TARGET" ]]; then
    echo "No recognized filesystem found."
    echo "Formatting the remote disk will destroy existing data."

    if (( SIZE < 4 * 1024 * 1024 )); then
        mkfs.fat -F 12 "$DEVICE"
    elif (( SIZE < 32 * 1024 * 1024 )); then
        mkfs.fat -F 16 "$DEVICE"
    else
        mkfs.fat -F 32 "$DEVICE"
    fi

    TARGET="$DEVICE"
fi

mount -t vfat -o "uid=$USER_ID,gid=$GROUP_ID" "$TARGET" "$MOUNTPOINT"

echo
echo "Mounted successfully!"
echo "Device: $TARGET"
echo "Path:   $MOUNTPOINT"
df -h "$MOUNTPOINT"
