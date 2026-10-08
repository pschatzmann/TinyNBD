/**
 * Exports a disk image file stored on the SD card. The file is created if it
 * does not exist yet.
 * Linux: sudo nbd-client <ip> 10809 /dev/nbd0 -N image
 *        sudo mkfs.ext4 /dev/nbd0
 */
#include <WiFi.h>

#include "NBD_SD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* imagePath = "/disk.img";
const size_t imageSize = 8 * 1024 * 1024;

WiFiServer wifiServer(NBD_DEFAULT_PORT);
NBDServer<WiFiServer> nbd_server(wifiServer);
File imageFile;
SDFileBlockDevice imageDisk(imageFile);

void createImage() {
  if (SD.exists(imagePath)) return;
  Serial.println("Creating image file...");
  File file = SD.open(imagePath, FILE_WRITE);
  static uint8_t zeros[4096] = {0};
  for (size_t written = 0; written < imageSize; written += sizeof(zeros)) {
    file.write(zeros, sizeof(zeros));
  }
  file.close();
}

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD card initialization failed");
    while (true) delay(1000);
  }
  createImage();
  imageFile = SD.open(imagePath, "r+");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  WiFi.setSleep(false);
  Serial.print("nbd server: ");
  Serial.println(WiFi.localIP());

  nbd_server.addExport("image", imageDisk, false, "Image file on SD");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() { nbd_server.loop(); }
