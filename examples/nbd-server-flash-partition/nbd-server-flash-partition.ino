/**
 * Exports the LittleFS data partition of the ESP32 flash as read-only block
 * device: an easy way to share static files with several clients. The
 * partition is called "littlefs" in the partitions.csv of this sketch, which
 * replaces the default partition table of the board.
 *
 * Define the content of the file system with the files in the data folder and
 * write it to the flash together with the sketch:
 * - Arduino IDE 2: "Upload LittleFS to Pico/ESP8266/ESP32" command of the
 *   arduino-littlefs-upload plugin
 *   (https://github.com/earlephilhower/arduino-littlefs-upload)
 * - Command line: ./upload-data.sh /dev/ttyUSB0 (builds the image with
 *   mklittlefs and writes it with esptool)
 *
 * Linux does not support LittleFS natively: mount it read-only with
 * littlefs-fuse (https://github.com/littlefs-project/littlefs-fuse):
 *        sudo nbd-client <ip> 10809 /dev/nbd0 -N flash -readonly
 *        sudo lfs --block_size=4096 -o ro /dev/nbd0 /mnt
 * Unmount with: sudo umount /mnt && sudo nbd-client -d /dev/nbd0
 *
 * The sketch itself may read the files, but must not change them while the
 * partition is exported.
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

  // check that the LittleFS image was uploaded (no formatting!)
  if (!LittleFS.begin(false, "/littlefs", 5, partitionLabel)) {
    Serial.println("No LittleFS found: upload the data folder");
    while (true) delay(1000);
  }
  Serial.printf("LittleFS: %u of %u bytes used\n",
                (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  LittleFS.end();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("flash", flashDisk, true, "ESP32 LittleFS partition");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
