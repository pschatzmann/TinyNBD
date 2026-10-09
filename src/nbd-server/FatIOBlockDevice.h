#pragma once
/**
 * @file FatIOBlockDevice.h
 * @brief BlockDevice adapter for a TinyFATFS IO driver. Any TinyFATFS drive
 * (RAM, SD, SDMMC, SPI, file, ...) can be exported through NBD with it.
 *
 * Needs the TinyFATFS library (https://github.com/pschatzmann/TinyFATFS) on
 * the include path (the directory that contains `driver/IO.h`). This header
 * is not included by NBD.h.
 */
#include <stdint.h>

#include "driver/IO.h"
#include "BlockDevice.h"

namespace nbd {

/**
 * @brief Exports a TinyFATFS IO driver as a sector based BlockDevice.
 *
 * **Dependency:** this class requires the TinyFATFS library
 * (https://github.com/pschatzmann/TinyFATFS). Its `src` folder must be on the
 * include path, so that `driver/IO.h` and `ff/ff.h` resolve. TinyFATFS is not
 * bundled with this library and is not included by `NBD.h`.
 *
 * - Sector size and count are reported by the driver (GET_SECTOR_SIZE and
 *   GET_SECTOR_COUNT) and cached when the device is initialized.
 * - Unaligned access is handled by SectorBlockDevice (read-modify-write).
 * - flush() uses CTRL_SYNC.
 * - trim() uses CTRL_TRIM for the full sectors inside the range. If the driver
 *   does not support trim (RES_PARERR), the request succeeds without action.
 * - isReadOnly() reports STA_PROTECT of the driver, and writes are refused
 *   when FF_IO_USE_WRITE is 0.
 *
 * The driver must outlive this object. Call begin() (done by NBDServer::begin())
 * before using the device, or the first size query initializes it.
 */
class FatIOBlockDevice : public SectorBlockDevice {
 public:
  /// Wraps a driver. pdrv is the physical drive number of the driver.
  explicit FatIOBlockDevice(fatfs::IO& driver, BYTE pdrv = 0)
      : driver(driver), pdrv(pdrv) {}

  /// Initializes the driver and reads its geometry. Returns false on error.
  bool begin() override {
    ready = false;
    if (driver.disk_initialize(pdrv) & fatfs::STA_NOINIT) return false;
    if (!queryGeometry()) return false;
    ready = true;
    return true;
  }

  /// Sector size reported by the driver (512 if unknown)
  uint32_t sectorSize() override {
    if (!ready) begin();
    return ss;
  }

  /// Number of sectors reported by the driver (0 if unknown)
  uint64_t sectorCount() override {
    if (!ready) begin();
    return count;
  }

  /// Commits pending writes of the driver
  bool flush() override {
#if FF_IO_USE_IOCTL
    return driver.disk_ioctl(pdrv, fatfs::CTRL_SYNC, nullptr) == fatfs::RES_OK;
#else
    return true;
#endif
  }

  /// Trim is always offered: unsupported drivers just ignore it
  bool canTrim() override { return true; }

  /// Discards the full sectors inside the range (partial sectors are kept)
  bool trim(uint64_t offset, uint64_t len) override {
#if FF_IO_USE_IOCTL
    uint32_t sz = sectorSize();
    if (sz == 0) return false;
    uint64_t first = (offset + sz - 1) / sz;  // first full sector
    uint64_t end = (offset + len) / sz;       // one past the last full sector
    if (end <= first) return true;
    LBA_t range[2] = {(LBA_t)first, (LBA_t)(end - 1)};
    fatfs::DRESULT res = driver.disk_ioctl(pdrv, fatfs::CTRL_TRIM, range);
    return res == fatfs::RES_OK || res == fatfs::RES_PARERR;
#else
    (void)offset;
    (void)len;
    return true;
#endif
  }

  /// Read only if the driver is write protected or can't write at all
  bool isReadOnly() override {
#if FF_IO_USE_WRITE
    return (driver.disk_status(pdrv) & fatfs::STA_PROTECT) != 0;
#else
    return true;
#endif
  }

  /// Driver used by this device
  fatfs::IO& getDriver() { return driver; }

 protected:
  fatfs::IO& driver;
  BYTE pdrv;
  bool ready = false;
  uint32_t ss = 512;
  uint64_t count = 0;

  bool readSectors(uint64_t sector, uint8_t* data, size_t n) override {
    return driver.disk_read(pdrv, data, (LBA_t)sector, (UINT)n) ==
           fatfs::RES_OK;
  }

  bool writeSectors(uint64_t sector, const uint8_t* data, size_t n) override {
#if FF_IO_USE_WRITE
    return driver.disk_write(pdrv, data, (LBA_t)sector, (UINT)n) ==
           fatfs::RES_OK;
#else
    (void)sector;
    (void)data;
    (void)n;
    return false;
#endif
  }

  /// Reads sector size and count. LBA_t is used for the count, which is
  /// little endian and zero initialized, so 32 bit drivers work as well.
  bool queryGeometry() {
#if FF_IO_USE_IOCTL
    // FatFs only needs GET_SECTOR_SIZE if FF_MIN_SS != FF_MAX_SS, so drivers
    // may not implement it: fall back to the fixed size in that case
    WORD sz = 0;
    if (driver.disk_ioctl(pdrv, fatfs::GET_SECTOR_SIZE, &sz) == fatfs::RES_OK)
      ss = sz;
    else
      ss = FF_MAX_SS;
    if (ss < 512) return false;
    LBA_t n = 0;
    if (driver.disk_ioctl(pdrv, fatfs::GET_SECTOR_COUNT, &n) != fatfs::RES_OK)
      return false;
    count = (uint64_t)n;
    return true;
#else
    return false;
#endif
  }
};

}  // namespace nbd
