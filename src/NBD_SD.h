#pragma once
/**
 * @file NBD_SD.h
 * @brief Support for the SD library (SPI): include this instead of NBD.h to
 * export a whole SD card with SDBlockDevice.
 */
#include <SD.h>

#include "NBD.h"

namespace nbd {

/// Raw sector access to an SD card driven by the SD (SPI) library
class SDBlockDevice : public SDRawBlockDevice<fs::SDFS> {
 public:
  explicit SDBlockDevice(fs::SDFS& sd = SD, bool readOnly = false)
      : SDRawBlockDevice<fs::SDFS>(sd, readOnly) {}
};

/// Disk image file on the SD card
using SDFileBlockDevice = FileBlockDevice<fs::File>;

}  // namespace nbd
