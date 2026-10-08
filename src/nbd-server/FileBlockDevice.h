#pragma once
#include "BlockDevice.h"

namespace nbd {

/**
 * @brief Exports a disk image file. Works with any Arduino File like class
 * (fs::File for SD, SD_MMC, LittleFS, FFat; SdFat FsFile ...) which provides
 * seek(), read(), write(), flush() and size(). The file must already exist
 * with the final size and must be opened for read/write (e.g. "r+").
 * The file object is referenced and must stay valid while the server runs.
 */
template <class FileT>
class FileBlockDevice : public BlockDevice {
 public:
  explicit FileBlockDevice(FileT& file, bool readOnly = false)
      : file(file), read_only(readOnly) {}

  bool begin() override { return (bool)file; }

  uint64_t size() override { return file.size(); }

  bool read(uint64_t offset, uint8_t* data, size_t len) override {
    if (!file.seek(offset)) return false;
    while (len > 0) {
      int n = file.read(data, len);
      if (n <= 0) return false;
      data += n;
      len -= n;
    }
    return true;
  }

  bool write(uint64_t offset, const uint8_t* data, size_t len) override {
    if (read_only || !file.seek(offset)) return false;
    return file.write(data, len) == len;
  }

  bool flush() override {
    file.flush();
    return true;
  }

  bool isReadOnly() override { return read_only; }

 protected:
  FileT& file;
  bool read_only;
};

}  // namespace nbd
