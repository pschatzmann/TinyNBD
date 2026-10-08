/**
 * nbd-client-tinyfatfs: mounts a remote nbd export as a FAT filesystem using
 * TinyFATFS's NBDClientIO driver, then reads and writes a file on it.
 *
 * Requires the TinyFATFS library (https://github.com/pschatzmann/TinyFATFS)
 * with its src folder on the include path, in addition to this library.
 *
 * Set the WiFi credentials and the server address before uploading. The
 * export must already contain a FAT filesystem (format it once with
 * f_mkfs()/SD.format() if it's empty), unless you want this sketch to format
 * it for you - see FORMAT_IF_NEEDED below.
 */
#include <WiFi.h>

#include "fatfs.h"
#include "driver/NBDClientIO.h"

// Set to true to format the export as FAT the first time it is mounted.
// Leave false once the export already has a filesystem on it, or you will
// lose its contents on every boot.
#define FORMAT_IF_NEEDED false

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* server_host = "192.168.1.10";  // nbd server address
const char* export_name = "ram";           // export name on the server

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);
// host string must stay valid while the driver is in use
NBDClientIO drv{nbd_client, server_host, nbd::NBD_DEFAULT_PORT, export_name};
File file;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected");

  // connects to the nbd server and mounts the export as a FAT filesystem
  if (!SD.begin(drv)) {
    Serial.println("mount failed");
#if FORMAT_IF_NEEDED
    Serial.println("formatting export as FAT...");
    if (!SD.format(drv) || !SD.begin(drv)) {
      Serial.println("format/mount failed");
      return;
    }
#else
    return;
#endif
  }

  // write to a file on the remote export
  file = SD.open("/test.txt", FILE_WRITE);
  if (file) {
    file.println("hello from nbd-client-tinyfatfs");
    file.close();
  } else {
    Serial.println("open for write failed");
  }

  // read it back
  file = SD.open("/test.txt");
  if (file) {
    Serial.printf("test.txt size: %u bytes\n", (unsigned)file.size());
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();
  } else {
    Serial.println("open for read failed");
  }
}

void loop() {}
