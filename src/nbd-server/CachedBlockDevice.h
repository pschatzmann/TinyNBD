#pragma once
/**
 * @file CachedBlockDevice.h
 * @brief Optional write-through LRU page cache which wraps any BlockDevice.
 * The cache can be allocated in PSRAM on boards which provide it.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "BlockDevice.h"

#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
#include <esp32-hal-psram.h>
#elif defined(ARDUINO_ARCH_RP2040) && defined(RP2350_PSRAM_CS)
#include <Arduino.h>  // pmalloc() on RP2350 boards with PSRAM
#endif

namespace nbd {

/**
 * @brief Decorator which caches pages of a wrapped BlockDevice.
 *
 * - Reads are served from cached pages; missing pages are loaded from the
 *   backend (LRU eviction).
 * - Writes are write-through: they are always forwarded to the backend. Cached
 *   pages that overlap the write are updated; writes don't allocate new pages.
 * - trim() and writeZeroes() invalidate the affected pages.
 * - If the cache memory can't be allocated, all calls are passed through.
 *
 * The backend must outlive this object. The cache memory is allocated lazily
 * on first use.
 */
class CachedBlockDevice : public BlockDevice {
 public:
  /**
   * @param backend the device to cache
   * @param cache_bytes total cache size in bytes (less than one page disables
   * caching)
   * @param page_size size of a cache page in bytes
   * @param use_psram allocate the cache in PSRAM if available
   */
  CachedBlockDevice(BlockDevice& backend, size_t cache_bytes,
                    size_t page_size = 4096, bool use_psram = true)
      : backend(backend), page_size(page_size), use_psram(use_psram) {
    slot_count = page_size > 0 ? cache_bytes / page_size : 0;
  }

  CachedBlockDevice(const CachedBlockDevice&) = delete;
  CachedBlockDevice& operator=(const CachedBlockDevice&) = delete;

  ~CachedBlockDevice() override {
    if (slots != nullptr) {
      free(pool);
      free(slots);
    }
  }

  bool begin() override { return backend.begin(); }
  uint64_t size() override { return backend.size(); }

  bool read(uint64_t offset, uint8_t* data, size_t len) override {
    if (!ensureCache()) return backend.read(offset, data, len);
    while (len > 0) {
      uint64_t page = offset / page_size;
      size_t in_page = offset % page_size;
      size_t n = page_size - in_page;
      if (n > len) n = len;
      Slot* s = lookup(page);
      if (s == nullptr) {
        misses++;
        s = load(page);
        // could not load the page: read the remaining range from the backend
        if (s == nullptr) return backend.read(offset, data, len);
      } else {
        hits++;
      }
      touch(s);
      memcpy(data, s->data + in_page, n);
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  bool write(uint64_t offset, const uint8_t* data, size_t len) override {
    if (!backend.write(offset, data, len)) {
      invalidate(offset, len);
      return false;
    }
    if (slots == nullptr) return true;
    // keep cached pages coherent with the backend
    while (len > 0) {
      uint64_t page = offset / page_size;
      size_t in_page = offset % page_size;
      size_t n = page_size - in_page;
      if (n > len) n = len;
      Slot* s = lookup(page);
      if (s != nullptr) {
        memcpy(s->data + in_page, data, n);
        touch(s);
      }
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  bool flush() override { return backend.flush(); }

  bool canTrim() override { return backend.canTrim(); }

  bool trim(uint64_t offset, uint64_t len) override {
    invalidate(offset, len);
    return backend.trim(offset, len);
  }

  bool writeZeroes(uint64_t offset, uint64_t len) override {
    invalidate(offset, len);
    return backend.writeZeroes(offset, len);
  }

  bool isReadOnly() override { return backend.isReadOnly(); }
  uint32_t minBlockSize() override { return backend.minBlockSize(); }
  uint32_t preferredBlockSize() override {
    return backend.preferredBlockSize();
  }

  /// Drops all cached pages
  void clear() {
    if (slots == nullptr) return;
    for (size_t i = 0; i < slot_count; i++) slots[i].valid = false;
  }

  /// Number of page reads served from the cache
  uint32_t hitCount() const { return hits; }
  /// Number of page reads which had to be loaded from the backend
  uint32_t missCount() const { return misses; }
  /// Number of cache pages (0 if caching is disabled)
  size_t pageCount() const { return slot_count; }
  /// Size of a cache page in bytes
  size_t pageSize() const { return page_size; }

 protected:
  struct Slot {
    uint64_t page = 0;
    bool valid = false;
    uint32_t last_used = 0;
    uint8_t* data = nullptr;
  };

  BlockDevice& backend;
  size_t page_size;
  bool use_psram;
  size_t slot_count = 0;
  Slot* slots = nullptr;
  uint8_t* pool = nullptr;
  uint32_t tick = 0;
  uint32_t hits = 0;
  uint32_t misses = 0;

  void* allocPool(size_t n) {
#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
    if (use_psram) return ps_malloc(n);
#elif defined(ARDUINO_ARCH_RP2040) && defined(RP2350_PSRAM_CS)
    // free() releases PSRAM blocks as well
    if (use_psram) return pmalloc(n);
#endif
    (void)use_psram;
    return malloc(n);
  }

  /// Allocates the cache on first use. Returns false if not available.
  bool ensureCache() {
    if (slots != nullptr) return true;
    if (slot_count == 0) return false;
    Slot* s = (Slot*)calloc(slot_count, sizeof(Slot));
    if (s == nullptr) return false;
    uint8_t* p = (uint8_t*)allocPool(slot_count * page_size);
    if (p == nullptr) {
      free(s);
      return false;
    }
    for (size_t i = 0; i < slot_count; i++) {
      s[i].data = p + i * page_size;
    }
    slots = s;
    pool = p;
    return true;
  }

  Slot* lookup(uint64_t page) {
    for (size_t i = 0; i < slot_count; i++) {
      if (slots[i].valid && slots[i].page == page) return &slots[i];
    }
    return nullptr;
  }

  /// Loads a page from the backend into a free or least recently used slot
  Slot* load(uint64_t page) {
    Slot* victim = &slots[0];
    for (size_t i = 0; i < slot_count; i++) {
      if (!slots[i].valid) {
        victim = &slots[i];
        break;
      }
      if (slots[i].last_used < victim->last_used) victim = &slots[i];
    }
    uint64_t start = page * page_size;
    uint64_t dev_size = backend.size();
    if (start >= dev_size) return nullptr;
    uint64_t n = dev_size - start;
    if (n > page_size) n = page_size;
    victim->valid = false;
    if (!backend.read(start, victim->data, (size_t)n)) return nullptr;
    victim->page = page;
    victim->valid = true;
    return victim;
  }

  void touch(Slot* s) { s->last_used = ++tick; }

  /// Invalidates all cached pages which overlap the byte range
  void invalidate(uint64_t offset, uint64_t len) {
    if (slots == nullptr || len == 0) return;
    uint64_t end = offset + len;
    for (size_t i = 0; i < slot_count; i++) {
      if (!slots[i].valid) continue;
      uint64_t p_start = slots[i].page * page_size;
      uint64_t p_end = p_start + page_size;
      if (p_start < end && offset < p_end) slots[i].valid = false;
    }
  }
};

}  // namespace nbd
