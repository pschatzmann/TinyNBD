/**
 * Desktop (Arduino Emulator): exports a 64 MB RAM disk on port 10809.
 * Linux: sudo nbd-client localhost 10809 /dev/nbd0 -N ram
 */
#include "WiFi.h"
#include "NBD.h"

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
MemoryBlockDevice ramDisk(64 * 1024 * 1024);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);
  nbd_server.addExport("ram", ramDisk, false, "RAM disk");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() {
  nbd_server.loop();
  delay(1);  // avoid 100% cpu
}
