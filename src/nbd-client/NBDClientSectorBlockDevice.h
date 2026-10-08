#pragma once
/**
 * @file NBDClientSectorBlockDevice.h
 * @brief BlockDevice adapter for a remote nbd export, so that the export can
 * be used wherever a local BlockDevice is accepted.
 */
#include <stdint.h>

#include "../nbd-server/BlockDevice.h"
#include "NBDClient.h"

namespace nbd {

/**
 * @brief Exposes an nbd export opened with NBDClient as a SectorBlockDevice.
 *
 * The NBDClient must already be connected (see NBDClient::connect()). The
 * adapter doesn't own the client: it must stay valid while the device is used.
 *
 * Definitions used in this class:
 * - **Sector**: the fixed 512 byte unit of the sector interface (sectorSize()).
 *   Sector N starts at byte offset N * 512. sectorCount() is the number of
 *   whole sectors in the export; a trailing partial sector is not counted.
 *   Unaligned byte access is handled by SectorBlockDevice using
 *   read-modify-write on whole sectors.
 * - **Block**: the unit the server uses to align requests, reported by the
 *   server during negotiation. minBlockSize() is the smallest unit the
 *   server accepts (1 if unknown) and preferredBlockSize() is the unit it
 *   recommends for best performance (4096 if unknown). Blocks are a hint for
 *   the caller; sectors are what this adapter actually transfers.
 */
class NBDClientSectorBlockDevice : public SectorBlockDevice {
 public:
  /// Wraps a connected NBDClient. The client must outlive this object.
  explicit NBDClientSectorBlockDevice(NBDClient& client) : client(client) {}

  /// Returns true if the export is connected
  bool begin() override { return client.isConnected(); }

  /// Size of the export in bytes
  uint64_t size() override { return client.size(); }

  /// Sector size of the sector interface: nbd has no sector size, so 512
  /// bytes are used. Unaligned access is handled by SectorBlockDevice.
  uint32_t sectorSize() override { return 512; }

  /// Number of whole sectors in the export
  uint64_t sectorCount() override { return client.size() / 512; }

  /// Smallest block size reported by the server (1 if unknown)
  uint32_t minBlockSize() override { return client.minBlockSize(); }

  /// Preferred block size reported by the server (4096 if unknown)
  uint32_t preferredBlockSize() override {
    return client.preferredBlockSize();
  }

  /// Without FLUSH support on the server there is nothing to commit
  bool flush() override { return client.canFlush() ? client.flush() : true; }

  /// Returns true if the server supports trim()
  bool canTrim() override { return client.canTrim(); }

  /// Discards the content of the range, split into 32 bit chunks if needed
  bool trim(uint64_t offset, uint64_t len) override {
    return forEachChunk(offset, len, [this](uint64_t o, uint32_t n) {
      return client.trim(o, n);
    });
  }

  /// Writes zeroes to the range. Falls back to writing zero buffers if the
  /// server doesn't support write zeroes.
  bool writeZeroes(uint64_t offset, uint64_t len) override {
    if (!client.canWriteZeroes()) return BlockDevice::writeZeroes(offset, len);
    return forEachChunk(offset, len, [this](uint64_t o, uint32_t n) {
      return client.writeZeroes(o, n);
    });
  }

  /// Returns true if the export is read-only
  bool isReadOnly() override { return client.isReadOnly(); }

 protected:
  NBDClient& client;

  /// Largest single request: the server's maximum payload, or the protocol
  /// limit of 32 bit if the server doesn't report one
  size_t chunkLimit() {
    uint32_t m = client.maxPayload();
    return m > 0 ? (size_t)m : (size_t)0xFFFFFFFFUL;
  }

  /// Reads len bytes at offset, one request per maximum payload
  bool readChunks(uint64_t offset, uint8_t* data, size_t len) {
    size_t max = chunkLimit();
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!client.read(offset, data, n)) return false;
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  /// Writes len bytes at offset, one request per maximum payload
  bool writeChunks(uint64_t offset, const uint8_t* data, size_t len) {
    size_t max = chunkLimit();
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!client.write(offset, data, n)) return false;
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  /// Called by SectorBlockDevice for whole-sector access
  bool readSectors(uint64_t sector, uint8_t* data, size_t count) override {
    return readChunks(sector * 512, data, count * 512);
  }

  /// Called by SectorBlockDevice for whole-sector access
  bool writeSectors(uint64_t sector, const uint8_t* data,
                    size_t count) override {
    return writeChunks(sector * 512, data, count * 512);
  }

  /// The protocol length field is 32 bit: split larger ranges into chunks
  template <class F>
  bool forEachChunk(uint64_t offset, uint64_t len, F op) {
    const uint64_t max_len = 0xFFFFFFFFULL;
    while (len > 0) {
      uint32_t n = len < max_len ? (uint32_t)len : (uint32_t)max_len;
      if (!op(offset, n)) return false;
      offset += n;
      len -= n;
    }
    return true;
  }
};

}  // namespace nbd
