/**
 * Exports the whole SD card (SDMMC host, SD_MMC library) as block device.
 * Linux: sudo nbd-client <ip> 10809 /dev/nbd0 -N sd
 *        sudo mount /dev/nbd0p1 /mnt
 * Do not use the SD file system from the sketch while the card is exported!
 */

#include <WiFi.h>
#include "NBD_SDMMC.h"

const char* ssid = "your-ssid";
const char* password = "your-password";

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
SDMMCBlockDevice sdDisk(SD_MMC);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  // define the pins with SD_MMC.setPins(...) if your board needs it
  if (!SD_MMC.begin()) {
    Serial.println("SD_MMC initialization failed");
    while (true) delay(1000);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("sd", sdDisk, false, "SD card (SDMMC)");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
