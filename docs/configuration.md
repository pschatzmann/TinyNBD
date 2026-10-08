# Configuration

- `NBD_MAX_EXPORTS` (default 4) and `NBD_MAX_CLIENTS` (default 3): define these before including `NBD.h`.
- `setBufferSize()`: transfer chunk size per client.
- `setTimeout()`: timeout in ms for the handshake and for receiving a message.
- `NBD_NO_USING_NAMESPACE`: define this to stop `NBD.h` from adding `using namespace nbd`.

## Page cache

`CachedBlockDevice` wraps any `BlockDevice` with a write-through LRU page cache. It is optional and not used unless you create it:

```cpp
// 256 KB cache in 4 KB pages, allocated in PSRAM if the board has it
CachedBlockDevice cached(sdDevice, 256 * 1024, 4096, true);
exports[0].device = &cached;
```

- Reads are served from cached pages when possible.
- Writes go to the backend first; cached pages are updated, not allocated.
- `trim()` and `writeZeroes()` invalidate the affected pages.
- `hitCount()` and `missCount()` report cache effectiveness.
- The cache can serve stale data if another client changes the same export (see `multiple-clients.md`).
