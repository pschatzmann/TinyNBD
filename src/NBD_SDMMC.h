#pragma once
/**
 * @file NBD_SDMMC.h
 * @brief Support for the SD_MMC library (SDMMC host): include this instead of
 * NBD.h to export a whole SD card with SDMMCBlockDevice.
 */
#include <SD_MMC.h>

#include "NBD.h"
#include "sdmmc_cmd.h"

namespace nbd {

namespace internal {
// provides access to the protected card handle of SDMMCFS
struct SDMMCCardAccess : fs::SDMMCFS {
  static sdmmc_card_t* card(fs::SDMMCFS& sd) {
    return sd.*(&SDMMCCardAccess::_card);
  }
};
}  // namespace internal

/**
 * @brief Raw sector access to an SD card driven by the SD_MMC library.
 * Consecutive sectors are transferred with a single multi-block command and
 * the size is the capacity of the whole card.
 */
class SDMMCBlockDevice : public SDRawBlockDevice<fs::SDMMCFS> {
 public:
  explicit SDMMCBlockDevice(fs::SDMMCFS& sd = SD_MMC, bool readOnly = false)
      : SDRawBlockDevice<fs::SDMMCFS>(sd, readOnly) {}

 protected:
  sdmmc_card_t* card() { return internal::SDMMCCardAccess::card(sd); }

  // SD_MMC.numSectors() returns the size of the FAT data area only
  uint64_t querySectorCount() override {
    sdmmc_card_t* c = card();
    return c != nullptr ? (uint64_t)c->csd.capacity : 0;
  }

  bool readSectors(uint64_t sector, uint8_t* data, size_t count) override {
    sdmmc_card_t* c = card();
    if (c == nullptr) return false;
    return sdmmc_read_sectors(c, data, (size_t)sector, count) == ESP_OK;
  }

  bool writeSectors(uint64_t sector, const uint8_t* data,
                    size_t count) override {
    if (read_only) return false;
    sdmmc_card_t* c = card();
    if (c == nullptr) return false;
    return sdmmc_write_sectors(c, data, (size_t)sector, count) == ESP_OK;
  }
};

/// Disk image file on the SD card
using SDMMCFileBlockDevice = FileBlockDevice<fs::File>;

}  // namespace nbd
