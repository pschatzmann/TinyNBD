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

  bool begin() override { return sectorSize() > 0 && sectorCount() > 0; }

  uint32_t sectorSize() override { return (uint32_t)sd.sectorSize(); }

  uint64_t sectorCount() override { return (uint64_t)sd.numSectors(); }

  bool isReadOnly() override { return read_only; }

 protected:
  SDT& sd;
  bool read_only;

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
