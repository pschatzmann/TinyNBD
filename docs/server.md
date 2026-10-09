# NBD Server

`NBDServer` exports storage attached to an Arduino (typically an ESP32) as a Network Block Device. Any Arduino server that provides `accept()` works, for example `WiFiServer`, `NetworkServer` or `EthernetServer`.

nbd (Network Block Device) is a simple TCP protocol that makes a remote disk look like a local one. The server exports raw storage as a list of fixed-size blocks, and the client reads and writes these blocks by offset. Unlike a network file system such as SMB or NFS, the server knows nothing about files: the client sees a plain disk (e.g. `/dev/nbd0` on Linux) and can partition it, format it and mount it with any file system. nbd is built into the Linux kernel and is supported by qemu and nbdkit.

## Features

- Fixed newstyle handshake: `NBD_OPT_GO`, `NBD_OPT_INFO`, `NBD_OPT_LIST`, `NBD_OPT_EXPORT_NAME`, `NBD_OPT_ABORT`
- Commands: read, write, flush, FUA, trim, write zeroes, disconnect
- Several named exports; an empty export name selects the first one
- Read-only exports
- Data is streamed in chunks (4096 bytes by default), so requests can be any size

## Block devices

| Class | Header | Description |
|---|---|---|
| `MemoryBlockDevice` | `NBD.h` | RAM disk (PSRAM preferred on ESP32 and RP2350) or a user-provided / const buffer |
| `ESP32PartitionBlockDevice` | `NBD.h` | ESP32 flash data partition (e.g. `"spiffs"`, `"ffat"`) |
| `FileBlockDevice<FileT>` | `NBD.h` | Disk image file (any File-like class) |
| `SDRawBlockDevice<SDT>` | `NBD.h` | Whole SD card via `readRAW()`/`writeRAW()` |
| `SDBlockDevice` | `NBD_SD.h` | Whole SD card using the **SD** (SPI) library |
| `SDMMCBlockDevice` | `NBD_SDMMC.h` | Whole SD card using the **SD_MMC** library |
| `CachedBlockDevice` | `NBD.h` | Optional write-through LRU page cache around any device, allocated in PSRAM when available. See [Configuration](configuration.md#page-cache) |
| `FatIOBlockDevice` | `nbd-server/FatIOBlockDevice.h` | Exports any [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) `IO` driver (RAM, SD, SDMMC, SPI, file). Not included by `NBD.h`; needs the TinyFATFS `src` folder on the include path |

To add your own device, subclass `BlockDevice` (byte-addressed) or `SectorBlockDevice` (sector-addressed; unaligned access is handled with read-modify-write).

## Example

```cpp
#include <WiFi.h>
#include "NBD_SD.h"

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
SDBlockDevice sdDisk(SD);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);
  SD.begin();
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  nbd_server.addExport("sd", sdDisk);
  nbd_server.begin();
}

void loop() { nbd_server.loop(); }
```

See [examples/](../examples/) for the RAM disk, SD, SD_MMC, flash partition and image file sketches.

## Connecting Linux clients

On Linux, install the nbd client and load the kernel module:

```bash
sudo apt install nbd-client      # Debian / Ubuntu
sudo modprobe nbd
```

```bash
# List exports offered by the ESP32
sudo nbd-client -l 192.168.1.33

# Connect to the RAM disk
sudo nbd-client 192.168.1.33 10809 /dev/nbd0 -N ram

# Create a mount point
sudo mkdir -p /mnt/nbd

# If no format: Format as FAT32 ONLY if the disk is empty and disposable
sudo mkfs.vfat -F 32 /dev/nbd0

# Mount with permissions for your normal user
sudo mount -t vfat -o uid=$(id -u),gid=$(id -g) /dev/nbd0 /mnt/nbd

...
# Release MBD
sudo umount /mnt/nbd
sudo nbd-client -d /dev/nbd0

```

___Important___: Skip mkfs.vfat if you have already formatted the disk and want to preserve its files. Your earlier df output showed an approximately 1 MB filesystem.

The qemu tools (`qemu-img`, `qemu-nbd`, `qemu-io`) also work as clients. They are in the `qemu-utils` package and don't need the kernel module:

```bash
qemu-nbd -L -b 192.168.1.50                     # list exports
qemu-img info nbd://192.168.1.50:10809/sd
qemu-img convert -O raw nbd://192.168.1.50:10809/sd backup.img
```

To access the server from another Arduino, use the [nbd client](client.md).

## Related documents

- [Configuration](configuration.md): compile-time limits, buffer size, timeout and namespace options.
- [Usage Notes](notes.md): SD card access, flash wear, performance and security. Read this before using the library.
- [Accessing an Export from Several Machines](multiple-clients.md): what is safe when several clients share an export.
- [Running on the Desktop](desktop.md): running the server on Linux, macOS and Windows with the Arduino Emulator.
