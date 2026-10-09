# TinyFATFS

[TinyFATFS](https://github.com/pschatzmann/TinyFATFS) is a separate library that provides a FAT file system on top of a block driver. TinyNBD does not include it. The two libraries meet in two classes, one on each side of the network connection.

## Overview

```
 NBD SERVER side                               NBD CLIENT side
 (device that owns the disk)                   (device that mounts the disk)

 FAT drive (RamIO, SD, SDMMC, ...)             FAT file system (SD.begin / File)
        │                                              │
 FatIOBlockDevice   ── exports sectors ──▶      NBDClientIO
        │             (NBD protocol)                   │
 NBDServer                                     NBDClient
```

| Side | Class | Library | What it does |
|---|---|---|---|
| Server | `FatIOBlockDevice` | TinyNBD (`src/nbd-server/`) | Exports the sectors of any TinyFATFS driver as an NBD export |
| Client | `NBDClientIO` | TinyFATFS (`src/driver/`) | Uses a remote NBD export as the disk of a FAT volume |

The two classes do not depend on each other. The NBD protocol is the only link between them.

## Server side: FatIOBlockDevice

Runs on the device that **holds** the disk, next to the NBD server. It turns a TinyFATFS `fatfs::IO` driver into an NBD block device.

- **Geometry**: sector size and count are read from the driver (`GET_SECTOR_SIZE`, `GET_SECTOR_COUNT`). If `GET_SECTOR_SIZE` is not implemented, `FF_MAX_SS` is used.
- **Reads and writes**: passed to `disk_read` and `disk_write`. Unaligned requests use read-modify-write.
- **Flush**: `CTRL_SYNC` of the driver.
- **Trim**: `CTRL_TRIM` for the full sectors inside the range. Drivers without trim support ignore it.
- **Read-only**: the export is read-only if the driver reports `STA_PROTECT`.

```cpp
#include "nbd-server/FatIOBlockDevice.h"  // TinyFATFS src on the include path

fatfs::RamIO ram(2048, 512);
nbd::FatIOBlockDevice dev(ram);
nbd_server.addExport("fat", dev, false, "TinyFATFS drive");
```

The NBD client sees a raw disk. It does not see the FAT structures, which are whatever the driver has written.

## Client side: NBDClientIO

Runs on the device that **mounts** the disk, next to the NBD client. It is a TinyFATFS driver whose storage is a remote NBD export.

- **Sectors**: fixed at 512 bytes. Sector N starts at byte offset `N * 512` on the export.
- **Connection**: `disk_initialize()` connects to the server if it is not connected. If the connection drops, the next read or write tries to reconnect once.
- **Large requests**: split into chunks no larger than the server's maximum payload.
- **Flush**: `CTRL_SYNC` sends an NBD flush if the server supports it.
- **Trim**: `CTRL_TRIM` sends an NBD trim if the server supports it, otherwise nothing happens.
- **Read-only**: if the export is read-only, `disk_status()` reports `STA_PROTECT` and writes return `RES_WRPRT`.

```cpp
#include "fatfs.h"
#include "driver/NBDClientIO.h"

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);
NBDClientIO drv{nbd_client, "192.168.1.10", nbd::NBD_DEFAULT_PORT, "ram"};

SD.begin(drv);  // connects to the export and mounts the FAT volume
```

The host string and export name are stored by pointer, so they must remain valid while the driver is used.

## Which side do I need?

| Goal | Server side | Client side |
|---|---|---|
| Serve a RAM, SD or flash FAT drive to other devices over NBD | `FatIOBlockDevice` | — |
| Mount a remote NBD export as files on an Arduino | — | `NBDClientIO` |
| Serve a plain disk (no FAT driver involved) | Use `SDBlockDevice`, `MemoryBlockDevice`, etc. (see [server](server.md)) | — |

## Requirements

- **Server side (`FatIOBlockDevice`)**: TinyFATFS `src` folder on the include path. Not included by `NBD.h`.
- **Client side (`NBDClientIO`)**: TinyFATFS `src` folder and this library's `src` folder on the include path. The driver includes `nbd-client/NBDClient.h`.
- **Desktop test**: `tests/fatio-test` is built when TinyFATFS is found. See [desktop](desktop.md) for the search order.

## Cautions

- Running both sides on one device gives two file system layers. The NBD client sees a FAT volume inside a raw disk, and the FAT inside the export is unaware of the client.
- Do not mount the same export on two clients at the same time. See [multiple clients](multiple-clients.md).
- Flash and SD wear apply to FAT volumes stored on those media. See [notes](notes.md).

## Related documents

- [Server](server.md): block devices and exports.
- [Client](client.md): the NBD client used by `NBDClientIO`.
- [Configuration](configuration.md): compile-time limits such as the maximum payload.
