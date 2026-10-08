#pragma once
#include <filesystem>
#include <fstream>
#include <string>

#include "BlockDevice.h"

namespace nbd {

/**
 * @brief Desktop only: exports a disk image file using the C++ standard
 * library. If a size is given, a missing file is created (and an existing
 * smaller one is extended) to that size.
 */
class DesktopFileBlockDevice : public BlockDevice {
 public:
  explicit DesktopFileBlockDevice(const char* path, uint64_t size = 0,
                                  bool readOnly = false)
      : path(path), requested_size(size), read_only(readOnly) {}

  bool begin() override {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (requested_size > 0 && !read_only) {
      if (!fs::exists(path, ec)) std::ofstream(path, std::ios::binary).close();
      if (fs::file_size(path, ec) < requested_size)
        fs::resize_file(path, requested_size, ec);
      if (ec) return false;
    }
    file_size = fs::file_size(path, ec);
    if (ec) return false;
    std::ios::openmode mode = std::ios::in | std::ios::binary;
    if (!read_only) mode |= std::ios::out;
    file.open(path, mode);
    return file.is_open();
  }

  uint64_t size() override { return file_size; }

  bool read(uint64_t offset, uint8_t* data, size_t len) override {
    file.clear();
    file.seekg((std::streamoff)offset);
    file.read((char*)data, len);
    return (size_t)file.gcount() == len;
  }

  bool write(uint64_t offset, const uint8_t* data, size_t len) override {
    if (read_only) return false;
    file.clear();
    file.seekp((std::streamoff)offset);
    file.write((const char*)data, len);
    return file.good();
  }

  bool flush() override {
    file.flush();
    return file.good();
  }

  bool isReadOnly() override { return read_only; }

 protected:
  std::string path;
  uint64_t requested_size;
  uint64_t file_size = 0;
  bool read_only;
  std::fstream file;
};

}  // namespace nbd
