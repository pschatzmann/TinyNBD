/**
 * Test server for tests/qemu-test.sh: exports a RAM disk, a sector based
 * device (read-modify-write), a read-only export and an image file.
 */
#include "WiFi.h"
#define NBD_MAX_EXPORTS 5
#include "NBD.h"

// sector device in RAM to test unaligned access
class RamSectorDevice : public SectorBlockDevice {
 public:
  uint32_t sectorSize() override { return 4096; }
  uint64_t sectorCount() override { return sizeof(mem) / 4096; }

 protected:
  uint8_t mem[1024 * 1024];
  bool readSectors(uint64_t sector, uint8_t* data, size_t count) override {
    memcpy(data, mem + sector * 4096, count * 4096);
    return true;
  }
  bool writeSectors(uint64_t sector, const uint8_t* data,
                    size_t count) override {
    memcpy(mem + sector * 4096, data, count * 4096);
    return true;
  }
};

WiFiServer wifiServer(NBD_DEFAULT_PORT);
NBDServer<WiFiServer> nbd_server(wifiServer);
MemoryBlockDevice ramDisk(4 * 1024 * 1024);
RamSectorDevice sectorDisk;
DesktopFileBlockDevice imageDisk("nbd-test.img", 4 * 1024 * 1024);
// page cache in front of the sector device
RamSectorDevice cachedBacking;
CachedBlockDevice cachedDisk(cachedBacking, 64 * 1024, 4096, false);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);
  nbd_server.addExport("ram", ramDisk, false, "RAM disk");
  nbd_server.addExport("sect", sectorDisk, false, "sector disk");
  nbd_server.addExport("ro", ramDisk, true);
  nbd_server.addExport("image", imageDisk, false, "image file");
  nbd_server.addExport("cached", cachedDisk, false, "cached sector disk");
  if (!nbd_server.begin()) {
    Serial.println("Could not start nbd server");
    exit(1);
  }
}

void loop() {
  nbd_server.loop();
  delay(1);
}
