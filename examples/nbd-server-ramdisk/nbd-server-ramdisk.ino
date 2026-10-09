/**
 * Exports a 1 MB RAM disk (PSRAM is used if available).
 * Linux: sudo nbd-client <ip> 10809 /dev/nbd0 -N ram
 */

#include <WiFi.h>
#include "NBD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";
const uint8_t sdChipSelectPin = 13;

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
MemoryBlockDevice ramDisk(1024 * 1024);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);  // much better throughput
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("ram", ramDisk, false, "RAM disk");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
