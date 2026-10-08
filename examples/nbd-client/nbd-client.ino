/**
 * nbd client example: connects to an nbd server, reads the first sector and
 * writes a test pattern to the second sector.
 *
 * Set the WiFi credentials and the server address before uploading.
 * Use a read-only export or a scratch export: the example writes data.
 */
#include <WiFi.h>

#include "NBD.h"

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* server_host = "192.168.1.10";  // nbd server address
const char* export_name = "ram";           // export name on the server

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected");

  if (!nbd_client.connect(server_host, nbd::NBD_DEFAULT_PORT, export_name)) {
    Serial.println("nbd connect failed");
    return;
  }
  Serial.printf("Export size: %llu bytes\n",
                (unsigned long long)nbd_client.size());

  // read the first 512 bytes
  uint8_t sector[512];
  if (nbd_client.read(0, sector, sizeof(sector))) {
    Serial.printf("First byte: 0x%02x\n", sector[0]);
  } else {
    Serial.println("read failed");
  }

  // write a pattern to the second sector and make it durable
  if (!nbd_client.isReadOnly()) {
    for (int j = 0; j < 512; j++) sector[j] = (uint8_t)j;
    if (!nbd_client.write(512, sector, sizeof(sector))) {
      Serial.println("write failed");
    }
    if (!nbd_client.flush()) Serial.println("flush failed");
  }

  nbd_client.disconnect();
}

void loop() {}
