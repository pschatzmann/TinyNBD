# NBD Client

`NBDClient` connects to an NBD server (for example another Arduino running this library, or `nbd-server` on a desktop) and reads and writes its exports. It works with any Arduino `Client` (`WiFiClient`, `NetworkClient`, `EthernetClient`).

## Usage

```cpp
#include "NBD.h"

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);

// export name "" selects the first export
if (nbd_client.connect("192.168.1.10", nbd::NBD_DEFAULT_PORT, "ram")) {
  uint8_t buf[512];
  nbd_client.read(0, buf, sizeof(buf));
  nbd_client.write(512, buf, sizeof(buf));
  nbd_client.flush();
  nbd_client.disconnect();
}
```

## API

| Method | Description |
| --- | --- |
| `connect(host, port, export)` / `connect(IPAddress, port, export)` | Opens the TCP connection and selects the export |
| `isConnected()` | True while the export is open |
| `size()` | Export size in bytes |
| `isReadOnly()`, `canFlush()`, `canTrim()`, `canWriteZeroes()` | Capabilities reported by the server |
| `read(offset, data, len)` | Reads `len` bytes |
| `write(offset, data, len, fua = false)` | Writes `len` bytes; `fua` requests a durable write |
| `flush()` | Commits pending writes on the server |
| `trim(offset, len)`, `writeZeroes(offset, len)` | Discard or zero a range, if supported |
| `disconnect()` | Sends `NBD_CMD_DISC` and closes the connection |
| `lastError()` | nbd error code of the last failed request |
| `setTimeout(ms)` | Timeout for waiting on the server (default 10000 ms) |

## Notes

- Requests are synchronous: each call waits for its reply before returning.
- The client supports the `NBD_OPT_EXPORT_NAME` handshake, which every server version accepts.
- Call the client from the Arduino `loop()` or `setup()`; it doesn't run in the background.
- There is no TLS or authentication, the same as the server.

## Block device adapter

Two adapters wrap a connected `NBDClient` so that a remote export can be used wherever a local device is accepted (for example to copy an export into a `FileBlockDevice`, or to re-export it with `NBDServer`):

- `NBDClientSectorBlockDevice`: a `SectorBlockDevice` with 512-byte sectors. Unaligned access is handled by read-modify-write.
- `NBDClient` block API: `blockSize()` (the server's block size, 4096 if unknown) and `blockCount()` describe the export in blocks. `readBlocks`, `writeBlocks`, `trimBlocks`, and `writeZeroesBlocks` take a starting block number and a block count, and return `false` if the range is outside the export. `flush()` is the regular `NBDClient` method.

```cpp
nbd::NBDClient nbd_client(wifi);
nbd_client.connect("192.168.1.10", nbd::NBD_DEFAULT_PORT, "ram");
nbd::NBDClientSectorBlockDevice remote(nbd_client);
```

The adapter doesn't own the client, so keep the `NBDClient` alive while the device is used. `flush()` does nothing if the server doesn't support it. Reads and writes are split to the server's maximum payload (`maxPayload()`), and `minBlockSize()` and `preferredBlockSize()` return the values the server reports.

`connect()` requests the block sizes with `NBD_OPT_GO`. Servers that refuse it are handled with `NBD_OPT_EXPORT_NAME`, and the block sizes then keep their defaults (1 and 4096).

## Errors

- `connect()` returns false if the TCP connection, the handshake or the export selection fails. The client closes the connection in that case.
- `read()`, `write()`, `flush()`, `trim()` and `writeZeroes()` return false on failure. If the server replied with an error, `lastError()` returns its nbd error code (for example `NBD_EIO`, `NBD_EINVAL`, `NBD_EPERM`, `NBD_ENOSPC`, defined in `NBDProtocol.h`).
- A timeout, a lost connection or an invalid reply closes the connection; check `isConnected()` before the next request and call `connect()` again.
- `write()` does nothing and returns false on a read-only export.
- `trim()` and `writeZeroes()` return false if the server doesn't advertise support (`canTrim()`, `canWriteZeroes()`).

## Limits

- Request lengths are `uint32_t`, so a single request can be at most 4 GiB. The practical limit is the memory and the time you can spend on the request.
- `write()` takes its data from the caller's buffer, so the buffer must hold the complete payload until the call returns.
- The client doesn't use `NBD_OPT_GO`, `NBD_OPT_INFO` or structured replies. It only needs `NBD_OPT_EXPORT_NAME`.

## Testing against a server

- Desktop: start `nbd-server-ramdisk-desktop` (see [desktop.md](desktop.md)) and connect the client to `localhost`.
- Linux: `nbd-client -l <host>` lists the exports of a server; `qemu-io -f raw nbd://<host>:10809/<export>` can read and write them for comparison.
- Use a scratch export when testing writes.

## Example

See `examples/nbd-client/nbd-client.ino`.

## Using a remote export as a FAT filesystem (TinyFATFS)

The [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) library provides an `NBDClientIO` driver that mounts an nbd export as a FAT filesystem, using `NBDClient` as the transport. This lets an Arduino read and write files on a remote export with the familiar `SD`/`File` API, instead of raw sector access.

```cpp
#include <WiFi.h>
#include "fatfs.h"
#include "driver/NBDClientIO.h"

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);
// host string must stay valid while the driver is in use
NBDClientIO drv{nbd_client, "192.168.1.10", nbd::NBD_DEFAULT_PORT, "ram"};
File file;

void setup() {
  SD.begin(drv);  // connects and mounts the export
  file = SD.open("/test.txt", FILE_WRITE);
  file.println("hello");
  file.close();
}

void loop() {}
```

The export must already contain a FAT filesystem; format it once (e.g. with `SD.format(drv)` or `mkfs.vfat` through `nbd-client` on Linux) before mounting an empty export. A read-only export is reported as write protected and file writes fail. See `examples/nbd-client-tinyfatfs/nbd-client-tinyfatfs.ino` for a full example, and the TinyFATFS README for its other drivers.

## Related documents

- [NBD Server](server.md): exporting storage from an Arduino.
- [Usage Notes](notes.md): security and performance notes that apply to both sides.

