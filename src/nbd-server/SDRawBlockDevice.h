#pragma once
#include "BlockDevice.h"

namespace nbd {

/**
 * @brief Exports a whole SD card on sector level (the client sees the
 * partition table and file systems). Works with any SD driver which provides
 * readRAW(uint8_t*, uint32_t sector), writeRAW(uint8_t*, uint32_t sector),
 * numSectors() and sectorSize() - e.g. the ESP32 SD and SD_MMC libraries.
 *
 * The SD driver must be started (e.g. SD.begin()) before NBDServer::begin().
 * Do not access the card's file system from the sketch while a client has
 * it mounted!
 */
template <class SDT>
class SDRawBlockDevice : public SectorBlockDevice {
 public:
  explicit SDRawBlockDevice(SDT& sd, bool readOnly = false)
      : sd(sd), read_only(readOnly) {}

  bool begin() override {
    // the geometry is queried once: the driver calls can be slow (e.g.
    // SD_MMC.numSectors() queries the file system) and size() is needed for
    // each request
    sector_size = (uint32_t)sd.sectorSize();
    sector_count = querySectorCount();
    return sector_size > 0 && sector_count > 0;
  }

  uint32_t sectorSize() override {
    return sector_size > 0 ? sector_size : (uint32_t)sd.sectorSize();
  }

  uint64_t sectorCount() override {
    return sector_count > 0 ? sector_count : querySectorCount();
  }

  bool isReadOnly() override { return read_only; }

 protected:
  SDT& sd;
  bool read_only;
  uint32_t sector_size = 0;
  uint64_t sector_count = 0;

  /// Number of sectors reported by the driver
  virtual uint64_t querySectorCount() { return (uint64_t)sd.numSectors(); }

  bool readSectors(uint64_t sector, uint8_t* data, size_t count) override {
    const uint32_t ss = sectorSize();
    for (size_t j = 0; j < count; j++) {
      if (!sd.readRAW(data + j * ss, (uint32_t)(sector + j))) return false;
    }
    return true;
  }

  bool writeSectors(uint64_t sector, const uint8_t* data,
                    size_t count) override {
    if (read_only) return false;
    const uint32_t ss = sectorSize();
    for (size_t j = 0; j < count; j++) {
      if (!sd.writeRAW((uint8_t*)data + j * ss, (uint32_t)(sector + j)))
        return false;
    }
    return true;
  }
};

}  // namespace nbd
