/**
 * Exports the LittleFS data partition of the ESP32 flash as read-only block
 * device. The partition is called "littlefs" in the partitions.csv of this
 * sketch, which replaces the default partition table of the board. At startup
 * the partition is formatted with LittleFS if it does not contain a valid file
 * system yet.
 *
 * Linux does not support LittleFS natively: mount it with littlefs-fuse
 * (https://github.com/littlefs-project/littlefs-fuse):
 *        sudo nbd-client <ip> 10809 /dev/nbd0 -N flash
 *        sudo lfs --block_size=4096 /dev/nbd0 /mnt
 * Unmount with: sudo umount /mnt && sudo nbd-client -d /dev/nbd0
 *
 * Do not write to LittleFS from the sketch while the partition is exported!
 * To allow writes from the client, remove the readOnly flag (flash has a
 * limited number of erase cycles!).
 */

#include <WiFi.h>
#include <LittleFS.h>
#include "NBD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* partitionLabel = "littlefs";

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
ESP32PartitionBlockDevice flashDisk(partitionLabel, true);  // read-only

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  // make sure that the partition contains LittleFS (format if necessary)
  if (!LittleFS.begin(true, "/littlefs", 5, partitionLabel)) {
    Serial.println("LittleFS initialization failed");
    while (true) delay(1000);
  }
  // do not use the file system from the sketch while the partition is exported
  LittleFS.end();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("flash", flashDisk, false, "ESP32 LittleFS partition");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
