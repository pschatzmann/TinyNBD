#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace nbd {

/**
 * @brief Abstract byte addressable block device which is exported by the
 * nbd server. All offsets and lengths are in bytes.
 */
class BlockDevice {
 public:
  virtual ~BlockDevice() = default;

  /// Initialize the device: called by NBDServer::begin()
  virtual bool begin() { return true; }
  /// Size of the device in bytes
  virtual uint64_t size() = 0;
  /// Reads len bytes from offset
  virtual bool read(uint64_t offset, uint8_t* data, size_t len) = 0;
  /// Writes len bytes to offset
  virtual bool write(uint64_t offset, const uint8_t* data, size_t len) = 0;
  /// Commits all pending writes to persistent storage
  virtual bool flush() { return true; }
  /// Returns true if trim() is supported
  virtual bool canTrim() { return false; }
  /// Discards the content of the indicated range (content becomes undefined)
  virtual bool trim(uint64_t /*offset*/, uint64_t /*len*/) { return false; }
  /// Writes zeroes to the indicated range
  virtual bool writeZeroes(uint64_t offset, uint64_t len) {
    uint8_t zeros[256] = {0};
    while (len > 0) {
      size_t n = len < sizeof(zeros) ? (size_t)len : sizeof(zeros);
      if (!write(offset, zeros, n)) return false;
      offset += n;
      len -= n;
    }
    return true;
  }
  /// Returns true if the device can not be written
  virtual bool isReadOnly() { return false; }
  /// Minimum block size: we use 1 because unaligned requests are supported
  virtual uint32_t minBlockSize() { return 1; }
  /// Preferred block size (power of 2, >= 512)
  virtual uint32_t preferredBlockSize() { return 4096; }
};

/**
 * @brief Base class for devices which can only access whole sectors (SD
 * cards, flash). Unaligned byte access is implemented with
 * read-modify-write using a single sector buffer.
 */
class SectorBlockDevice : public BlockDevice {
 public:
  ~SectorBlockDevice() override { free(sector_buffer); }

  /// Size of a sector in bytes
  virtual uint32_t sectorSize() = 0;
  /// Number of sectors
  virtual uint64_t sectorCount() = 0;

  uint64_t size() override { return sectorCount() * sectorSize(); }

  uint32_t preferredBlockSize() override {
    uint32_t ss = sectorSize();
    return ss < 512 ? 512 : ss;
  }

  bool read(uint64_t offset, uint8_t* data, size_t len) override {
    const uint32_t ss = sectorSize();
    if (ss == 0) return false;
    while (len > 0) {
      uint64_t sector = offset / ss;
      uint32_t in_sector = offset % ss;
      size_t n;
      if (in_sector == 0 && len >= ss) {
        size_t count = len / ss;
        if (!readSectors(sector, data, count)) return false;
        n = count * ss;
      } else {
        uint8_t* buf = sectorBuffer();
        if (buf == nullptr || !readSectors(sector, buf, 1)) return false;
        n = ss - in_sector;
        if (n > len) n = len;
        memcpy(data, buf + in_sector, n);
      }
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  bool write(uint64_t offset, const uint8_t* data, size_t len) override {
    const uint32_t ss = sectorSize();
    if (ss == 0) return false;
    while (len > 0) {
      uint64_t sector = offset / ss;
      uint32_t in_sector = offset % ss;
      size_t n;
      if (in_sector == 0 && len >= ss) {
        size_t count = len / ss;
        if (!writeSectors(sector, data, count)) return false;
        n = count * ss;
      } else {
        // read-modify-write of a partial sector
        uint8_t* buf = sectorBuffer();
        if (buf == nullptr || !readSectors(sector, buf, 1)) return false;
        n = ss - in_sector;
        if (n > len) n = len;
        memcpy(buf + in_sector, data, n);
        if (!writeSectors(sector, buf, 1)) return false;
      }
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

 protected:
  uint8_t* sector_buffer = nullptr;

  /// Reads count consecutive sectors
  virtual bool readSectors(uint64_t sector, uint8_t* data, size_t count) = 0;
  /// Writes count consecutive sectors
  virtual bool writeSectors(uint64_t sector, const uint8_t* data,
                            size_t count) = 0;

  uint8_t* sectorBuffer() {
    if (sector_buffer == nullptr) {
      sector_buffer = (uint8_t*)malloc(sectorSize());
    }
    return sector_buffer;
  }
};

}  // namespace nbd
