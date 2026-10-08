# Usage Notes

## SD cards

- Don't use a card's file system from the sketch while a client has the raw device mounted (`SDBlockDevice`, `SDMMCBlockDevice`). The client doesn't notice changes made by the sketch, and the sketch doesn't notice changes made by the client. Both sides can corrupt the file system.
- Start the SD driver (`SD.begin()` or `SD_MMC.begin()`) before calling `NBDServer::begin()`. Otherwise the card reports a size of 0 and the export fails to start.

## Sharing an export

Several machines can safely share an export only if it is read-only. See [multiple-clients.md](multiple-clients.md).

## Flash partitions

Flash has a limited number of erase cycles. `ESP32PartitionBlockDevice` erases a whole 4 KB sector for every write that covers only part of a sector, so small, scattered writes wear the flash faster than you might expect. Use flash partitions for data that changes rarely, and prefer an SD card for frequent writes.

## Performance

`WiFi.setSleep(false)` improves throughput a lot. With WiFi power saving enabled, every request can wait for the next WiFi wake-up.

## Security

There is no authentication or TLS: anyone who can reach the port can read the exports, and write to those that aren't read-only. Only use this on trusted networks.
