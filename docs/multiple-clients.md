# Accessing an Export from Several Machines

Several machines can read the same export at the same time without problems. As soon as anyone writes, every client that has the device mounted is at risk of seeing stale or corrupted data.

## Number of clients

By default the server accepts up to 3 clients at the same time and rejects any further connections. To change the limit, define `NBD_MAX_CLIENTS` before including the library:

```cpp
#define NBD_MAX_CLIENTS 1   // e.g. allow only a single client
#include "NBD.h"
```

Each connected client uses its own transfer buffer (4 KB by default, see `setBufferSize()`). The buffer is allocated when the client connects, so unused slots cost almost no memory.

Because multiple clients are allowed by default, nothing stops a second machine from connecting to a writable export. Read the sections below before sharing an export.

## What the server guarantees

`NBDServer::loop()` runs on a single thread and handles one request at a time: a request is fully finished before the next one starts. Requests from different clients never interleave, so a client never sees half of another client's write. This also covers the shared sector buffer that `SectorBlockDevice` uses for unaligned access.

## What the server cannot guarantee: cache coherence

nbd does not keep clients' caches in sync. Every Linux client caches blocks and file system metadata in its own memory, and it isn't told when another client changes the device.

If one machine writes while another has the device mounted, the reading machine:

- keeps serving outdated data from its cache,
- may see a file system that looks corrupt (for example, metadata that points to blocks which have changed since it cached them),
- can end up with errors or a kernel panic in the file system driver.

This is the same as attaching one disk to two computers at once. The nbd server can't prevent it. Only a cluster file system (for example OCFS2 or GFS2) is designed for shared write access, and those aren't practical here.

The same applies to the sketch itself: if the ESP32 writes to the SD card or flash partition while it is exported, the clients don't notice.

## Recommended setup for sharing

1. **Export read-only on the server**:

   ```cpp
   nbd_server.addExport("sd", sdDisk, true);   // readOnly = true
   ```

   The server then refuses all writes and tells clients the export is read-only (`NBD_FLAG_READ_ONLY`). Linux marks the device read-only as well.

2. **Mount read-only on every client**:

   ```bash
   sudo nbd-client 192.168.1.50 10809 /dev/nbd0 -N sd
   sudo mount -o ro /dev/nbd0p1 /mnt
   ```

   For ext3 or ext4 that wasn't unmounted cleanly, add `noload`, because a read-only device can't replay the journal:

   ```bash
   sudo mount -o ro,noload /dev/nbd0p1 /mnt
   ```

3. **Don't write from the sketch** to the exported card or partition while clients are connected.

If you need write access, give it to only one machine at a time, and make sure no other machine has the device connected while it writes.

## Performance

- All clients share the bandwidth of the ESP32.
- A large request from one client (for example a 2 MB read) holds up the other clients until it is done.
- While a new client connects, the other sessions pause briefly. If the new client is slow, the pause can last up to the timeout set with `setTimeout()` (10 seconds by default).

## Summary

| Scenario | Safe? |
|---|---|
| Several clients, read-only export, mounted read-only | Yes |
| One client reading and writing, nobody else connected | Yes |
| One client writing, others reading at the same time | No: readers see stale or corrupt data |
| Several clients writing | No: file system corruption |
| Sketch writes to the device while it is exported | No |
