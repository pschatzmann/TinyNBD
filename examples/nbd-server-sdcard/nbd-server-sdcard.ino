/**
 * Exports the whole SD card (SPI, SD library) as block device.
 * Linux: sudo nbd-client <ip> 10809 /dev/nbd0 -N sd
 *        sudo mount /dev/nbd0p1 /mnt
 * Do not use the SD file system from the sketch while the card is exported!
 */
#include <WiFi.h>

#include "NBD_SD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";
const uint8_t sdChipSelectPin = 13;

WiFiServer wifiServer(NBD_DEFAULT_PORT);
NBDServer<WiFiServer> nbd_server(wifiServer);
SDBlockDevice sdDisk(SD);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  // Adapt the SPI pins to your board if the default pins are not usable (e.g. because
  SPI.begin(14, 2, 15, sdChipSelectPin);  // SCK, MISO, MOSI, CS

  if (!SD.begin(sdChipSelectPin, SPI, 20000000)) {
    Serial.println("SD card initialization failed");
    while (true) delay(1000);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("sd", sdDisk, false, "SD card (SPI)");
  nbd_server.setBufferSize(16384); 
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
