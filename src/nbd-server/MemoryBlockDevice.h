#pragma once
#include "BlockDevice.h"
#if defined(ESP32)
#include "esp_heap_caps.h"
#endif

namespace nbd {

/**
 * @brief RAM disk. Either uses a buffer provided by the caller or allocates
 * the memory in begin() (on the ESP32 PSRAM is preferred if available).
 */
class MemoryBlockDevice : public BlockDevice {
 public:
  /// Allocates (zeroed) memory of the indicated size in begin()
  explicit MemoryBlockDevice(size_t size) : data_size(size) {}

  /// Uses the provided writable buffer
  MemoryBlockDevice(uint8_t* data, size_t size, bool readOnly = false)
      : data(data), data_size(size), read_only(readOnly) {}

  /// Exports a constant image (e.g. stored in flash) read-only
  MemoryBlockDevice(const uint8_t* data, size_t size)
      : data((uint8_t*)data), data_size(size), read_only(true) {}

  ~MemoryBlockDevice() override {
    if (owns_data) free(data);
  }

  bool begin() override {
    if (data == nullptr && data_size > 0) {
#if defined(ESP32)
      data = (uint8_t*)heap_caps_calloc(1, data_size,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
      if (data == nullptr) data = (uint8_t*)calloc(1, data_size);
      owns_data = data != nullptr;
    }
    return data != nullptr;
  }

  uint64_t size() override { return data == nullptr ? 0 : data_size; }

  bool read(uint64_t offset, uint8_t* out, size_t len) override {
    if (!isValid(offset, len)) return false;
    memcpy(out, data + offset, len);
    return true;
  }

  bool write(uint64_t offset, const uint8_t* in, size_t len) override {
    if (read_only || !isValid(offset, len)) return false;
    memcpy(data + offset, in, len);
    return true;
  }

  bool canTrim() override { return !read_only; }

  bool trim(uint64_t offset, uint64_t len) override {
    return writeZeroes(offset, len);
  }

  bool writeZeroes(uint64_t offset, uint64_t len) override {
    if (read_only || !isValid(offset, len)) return false;
    memset(data + offset, 0, len);
    return true;
  }

  bool isReadOnly() override { return read_only; }

  /// Provides access to the memory
  uint8_t* buffer() { return data; }

 protected:
  uint8_t* data = nullptr;
  size_t data_size = 0;
  bool read_only = false;
  bool owns_data = false;

  bool isValid(uint64_t offset, uint64_t len) {
    return data != nullptr && offset <= data_size && len <= data_size - offset;
  }
};

}  // namespace nbd
