#!/usr/bin/env bash
set -euo pipefail

DEVICE="/dev/nbd0"
MOUNT_DIR="esp32-ram"

usage() {
    cat <<EOF
Usage: sudo $0 [OPTIONS]

Unmount the ESP32 NBD RAM disk and disconnect the NBD device.

Options:
  -d, --device DEVICE   Local NBD device (default: $DEVICE)
  -m, --mount-dir NAME  Directory under user's home (default: $MOUNT_DIR)
  -h, --help            Show this help

Examples:
  sudo $0
  sudo $0 --device /dev/nbd1
  sudo $0 --mount-dir remote-disk

The script performs a normal unmount before disconnecting the NBD device.
It does not force-unmount a busy filesystem.
EOF
}

while (($#)); do
    case "$1" in
        -d|--device)
            [[ $# -ge 2 ]] || { echo "Missing device value"; exit 2; }
            DEVICE="$2"
            shift 2
            ;;
        -m|--mount-dir)
            [[ $# -ge 2 ]] || { echo "Missing mount directory value"; exit 2; }
            MOUNT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage >&2
            exit 2
            ;;
    esac
done

# Find the invoking user's home directory when run with sudo.
if [[ -n "${SUDO_USER:-}" ]]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
else
    USER_HOME="$HOME"
fi

MOUNTPOINT="$USER_HOME/$MOUNT_DIR"

if [[ $EUID -ne 0 ]]; then
    echo "Please run: sudo $0 [OPTIONS]"
    exit 1
fi

# Unmount first, if mounted.
if mountpoint -q "$MOUNTPOINT"; then
    echo "Unmounting $MOUNTPOINT..."
    sync
    umount "$MOUNTPOINT"
else
    echo "$MOUNTPOINT is not mounted."
fi

# Disconnect the NBD device if connected.
SIZE=$(blockdev --getsize64 "$DEVICE" 2>/dev/null || echo 0)

if [[ "$SIZE" -gt 0 ]]; then
    echo "Disconnecting $DEVICE..."
    nbd-client -d "$DEVICE"
else
    echo "$DEVICE is not connected."
fi

echo "Done."

