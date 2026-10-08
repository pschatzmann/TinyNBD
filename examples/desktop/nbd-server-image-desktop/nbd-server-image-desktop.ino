/**
 * Desktop (Arduino Emulator): exports the image file disk.img (created with
 * 64 MB if it does not exist) on port 10809.
 * Linux: sudo nbd-client localhost 10809 /dev/nbd0 -N image
 */
#include "WiFi.h"
#include "NBD.h"

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
DesktopFileBlockDevice imageDisk("disk.img", 64 * 1024 * 1024);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);
  nbd_server.addExport("image", imageDisk, false, "Image file");
  if (!nbd_server.begin()) Serial.println("Could not start nbd server");
}

void loop() {
  nbd_server.loop();
  delay(1);  // avoid 100% cpu
}
