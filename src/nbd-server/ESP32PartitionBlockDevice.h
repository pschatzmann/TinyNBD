#pragma once
#if defined(ESP32)
#include "BlockDevice.h"
#include "esp_partition.h"

namespace nbd {

/**
 * @brief Exports an ESP32 flash data partition (e.g. "spiffs" or "ffat").
 * Writes are done with erase + write of 4096 byte flash sectors, so keep in
 * mind that flash has a limited number of erase cycles.
 */
class ESP32PartitionBlockDevice : public SectorBlockDevice {
 public:
  /// Uses the first data partition with the indicated label
  explicit ESP32PartitionBlockDevice(const char* label, bool readOnly = false)
      : label(label), read_only(readOnly) {}

  explicit ESP32PartitionBlockDevice(const esp_partition_t* partition,
                                     bool readOnly = false)
      : partition(partition), read_only(readOnly) {}

  bool begin() override {
    if (partition == nullptr) {
      partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                           ESP_PARTITION_SUBTYPE_ANY, label);
    }
    return partition != nullptr;
  }

  uint32_t sectorSize() override { return 4096; }

  uint64_t sectorCount() override {
    return partition == nullptr ? 0 : partition->size / sectorSize();
  }

  bool isReadOnly() override { return read_only; }

  bool canTrim() override { return !read_only; }

  /// Erases all complete flash sectors in the range
  bool trim(uint64_t offset, uint64_t len) override {
    if (read_only || partition == nullptr) return false;
    const uint32_t ss = sectorSize();
    uint64_t start = (offset + ss - 1) / ss * ss;
    uint64_t end = (offset + len) / ss * ss;
    if (end <= start) return true;
    return esp_partition_erase_range(partition, start, end - start) == ESP_OK;
  }

  const esp_partition_t* getPartition() { return partition; }

 protected:
  const char* label = nullptr;
  const esp_partition_t* partition = nullptr;
  bool read_only;

  bool readSectors(uint64_t sector, uint8_t* data, size_t count) override {
    if (partition == nullptr) return false;
    const uint32_t ss = sectorSize();
    return esp_partition_read(partition, sector * ss, data, count * ss) ==
           ESP_OK;
  }

  bool writeSectors(uint64_t sector, const uint8_t* data,
                    size_t count) override {
    if (read_only || partition == nullptr) return false;
    const uint32_t ss = sectorSize();
    if (esp_partition_erase_range(partition, sector * ss, count * ss) !=
        ESP_OK)
      return false;
    return esp_partition_write(partition, sector * ss, data, count * ss) ==
           ESP_OK;
  }
};

}  // namespace nbd
#endif
