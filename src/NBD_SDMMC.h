#pragma once
/**
 * @file NBD_SDMMC.h
 * @brief Support for the SD_MMC library (SDMMC host): include this instead of
 * NBD.h to export a whole SD card with SDMMCBlockDevice.
 */
#include <SD_MMC.h>

#include "NBD.h"

namespace nbd {

/// Raw sector access to an SD card driven by the SD_MMC library
class SDMMCBlockDevice : public SDRawBlockDevice<fs::SDMMCFS> {
 public:
  explicit SDMMCBlockDevice(fs::SDMMCFS& sd = SD_MMC, bool readOnly = false)
      : SDRawBlockDevice<fs::SDMMCFS>(sd, readOnly) {}
};

/// Disk image file on the SD card
using SDMMCFileBlockDevice = FileBlockDevice<fs::File>;

}  // namespace nbd
