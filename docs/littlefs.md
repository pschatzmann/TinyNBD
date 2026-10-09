# LittleFS

[LittleFS](https://github.com/littlefs-project/littlefs) is a small fail-safe file system designed for microcontrollers. TinyNBD does not include it. NBD only transports blocks, so LittleFS can be used on either side of the connection: on the device that holds the storage, or on the device that mounts the export.

## Overview

```
 NBD SERVER side                               NBD CLIENT side
 (device that owns the disk)                   (device that mounts the disk)

                                               Arduino: NBDLittleFS + lfs_* API
 ESP32 flash partition / RAM disk / ...        Linux:   littlefs-fuse (lfs)
        │                                              │
 ESP32PartitionBlockDevice ── blocks ──▶       NBDClient / nbd-client
 MemoryBlockDevice, ...    (NBD protocol)              │
        │
 NBDServer
```

| Side | Class / tool | Library | What it does |
|---|---|---|---|
| Server | `ESP32PartitionBlockDevice` | TinyNBD | Exports a flash partition with a LittleFS image that was uploaded with the sketch |
| Server | `MemoryBlockDevice` etc. | TinyNBD | Exports any storage; the client formats it as LittleFS |
| Client | `NBDLittleFS` | TinyNBD (`NBD_LittleFS.h`) + 107-Arduino-littlefs | Mounts an export as LittleFS on an Arduino |
| Client | `lfs` (littlefs-fuse) | [littlefs-fuse](https://github.com/littlefs-project/littlefs-fuse) | Mounts an export as LittleFS on Linux |

The server does not need to know that the export contains LittleFS.

## Server side: sharing static files from a LittleFS partition

The recommended way to share static files is to write a LittleFS image to an ESP32 flash partition together with the sketch, export the partition read-only, and mount it read-only on the clients. Because nobody writes to it, several clients can mount it at the same time and the flash does not wear out.

The [nbd-server-flash-partition](../examples/nbd-server-flash-partition/nbd-server-flash-partition.ino) example is set up this way:

- `partitions.csv` replaces the board's default partition table. It names the data partition `littlefs` (offset `0x290000`, size `0x160000`, 4 MB flash).
- The `data` folder of the sketch holds the files of the file system.
- The sketch checks that the partition contains LittleFS, but never formats it, and exports it read-only:

```cpp
ESP32PartitionBlockDevice flashDisk("littlefs", true);  // read-only device
nbd_server.addExport("flash", flashDisk, true, "ESP32 LittleFS partition");
```

### Uploading the data folder

Upload the sketch as usual, then write the LittleFS image built from the `data` folder:

- **Arduino IDE 2**: install the [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) plugin and run *Upload LittleFS to Pico/ESP8266/ESP32* from the command palette (Ctrl+Shift+P). Close the serial monitor first.
- **Command line**: run `./upload-data.sh /dev/ttyUSB0` in the sketch folder. The script builds the image with the core's `mklittlefs` (block size 4096, page size 256) and writes it to the partition offset with `esptool`:

```bash
mklittlefs -c data -b 4096 -p 256 -s 0x160000 littlefs.bin
esptool --port /dev/ttyUSB0 write-flash 0x290000 littlefs.bin
```

If you change the partition table, update the offset and size in the script. Uploading the image again replaces the whole file system.

### Mounting on Linux

Linux does not support LittleFS natively. Mount the export read-only with [littlefs-fuse](https://github.com/littlefs-project/littlefs-fuse), which has to be built from source:

```bash
sudo nbd-client <ip> 10809 /dev/nbd0 -N flash -readonly
sudo lfs --block_size=4096 -o ro /dev/nbd0 /mnt
...
sudo umount /mnt && sudo nbd-client -d /dev/nbd0
```

The sketch itself may read the files with the core's `LittleFS`, but must not change them while the partition is exported: the clients would not notice the changes. To make the export writable, remove both read-only flags; then only one client may mount it, and flash wear applies (see [notes](notes.md)).

## Client side: NBDLittleFS

`NBDLittleFS` runs on the device that **mounts** the export, next to `NBDClient`. It provides the block device callbacks that LittleFS needs and maps them to NBD requests.

- **Blocks**: block N starts at byte offset `N * blockSize` on the export. The block size is a constructor parameter (default 4096) and must match an existing file system. The block count is the export size divided by the block size.
- **Reads and writes**: one NBD request per call, in units of 512 bytes.
- **Erase**: does nothing, because a network disk does not need to be erased before it is written.
- **Sync**: sends an NBD flush if the server supports it.
- **Caches**: read and write caches have the size of a block, because each cache miss is a network round trip. LittleFS allocates them with `malloc()` (about 3 blocks plus one per open file).
- **Mounting**: `begin(formatIfNeeded)` mounts the file system. With `true`, an export that can't be mounted is formatted and loses its content. A read-only export is never formatted.

```cpp
#include <WiFi.h>
#include "NBD_LittleFS.h"

WiFiClient wifi;
NBDClient nbd_client(wifi);
NBDLittleFS lfs_fs(nbd_client);  // block size 4096

void setup() {
  // ... connect WiFi
  nbd_client.connect("192.168.1.10", NBD_DEFAULT_PORT, "ram");
  lfs_fs.begin(true);
  lfs_file_t file;
  lfs_file_open(lfs_fs.lfs(), &file, "test.txt", LFS_O_WRONLY | LFS_O_CREAT);
  lfs_file_write(lfs_fs.lfs(), &file, "hello", 5);
  lfs_file_close(lfs_fs.lfs(), &file);
  lfs_fs.end();
}
```

After `begin()` the files are accessed with the littlefs C API (`lfs_file_open()`, `lfs_dir_read()`, …), passing `lfs()`. The `NBDClient` must be connected before `begin()` and must outlive the `NBDLittleFS` object. Don't name the object `littlefs`: 107-Arduino-littlefs uses that name for its namespace.

See [nbd-client-littlefs](../examples/nbd-client-littlefs/nbd-client-littlefs.ino) for a full example that works with the `ram` export of the nbd-server-ramdisk example.

## Compatibility

A file system can only be mounted if both sides agree on these points:

- **Block size**: the ESP32 core and the examples use 4096 bytes. littlefs-fuse needs `--block_size=4096`.
- **On-disk version**: 107-Arduino-littlefs contains littlefs v2.5, which reads and writes on-disk version 2.0. Newer littlefs versions create file systems with on-disk version 2.1, which v2.5 can't mount. This includes the ESP32 core's LittleFS and its `mklittlefs` (version 4.0.2 writes images with on-disk version 2.1). So `NBDLittleFS` can't mount the partition of the nbd-server-flash-partition example: use littlefs-fuse on Linux for it, and `NBDLittleFS` for exports which it formatted itself. littlefs-fuse must support on-disk version 2.1 (littlefs v2.6 or newer).

## Requirements

- **`NBDLittleFS`**: the [107-Arduino-littlefs](https://github.com/107-systems/107-Arduino-littlefs) library (Arduino library manager: `arduino-cli lib install 107-Arduino-littlefs`). Include `NBD_LittleFS.h` instead of `NBD.h`. The library lists only rp2040 and renesas boards; the IDE warns that it may be incompatible with ESP32, but it compiles and the warning can be ignored.
- **Server side**: nothing beyond this library. To upload the `data` folder, either the arduino-littlefs-upload plugin (Arduino IDE 2) or the `mklittlefs` and `esptool` tools of the ESP32 core (used by `upload-data.sh`).

## Cautions

- Do not mount the same export with write access on two clients at the same time. See [multiple clients](multiple-clients.md).
- A failed mount with `formatIfNeeded` set to `true` erases the export. Use `false` for exports with valuable content.
- Flash wear applies to LittleFS partitions which are exported with write access. See [notes](notes.md).
- The example's `partitions.csv` is for 4 MB flash. The first upload with it changes the partition layout, so data in the old `spiffs` partition is lost.

## Related documents

- [Server](server.md): block devices and exports.
- [Client](client.md): the NBD client used by `NBDLittleFS`.
- [TinyFATFS](tinyfatfs.md): the same for FAT file systems.
