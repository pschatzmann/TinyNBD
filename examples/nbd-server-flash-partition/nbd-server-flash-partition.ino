/**
 * Exports the "spiffs" data partition of the ESP32 flash.
 * Linux: sudo nbd-client <ip> 10809 /dev/nbd0 -N flash
 *        sudo mkfs.vfat /dev/nbd0
 * Note: flash has a limited number of erase cycles!
 */

#include <WiFi.h>
#include "NBD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";

WiFiServer wifiServer(NBD_DEFAULT_PORT); // // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
ESP32PartitionBlockDevice flashDisk("spiffs");

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("flash", flashDisk, false, "ESP32 flash partition");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
