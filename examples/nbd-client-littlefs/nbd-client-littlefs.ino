/**
 * nbd-client-littlefs: mounts a remote nbd export as a LittleFS filesystem,
 * writes and reads a file on it and lists the root directory.
 *
 * Requires the 107-Arduino-littlefs library (Arduino library manager), which
 * provides the littlefs C API (lfs.h), in addition to this library.
 *
 * Set the WiFi credentials and the server address before uploading. The
 * export must already contain LittleFS with a block size of 4096 bytes,
 * unless you want this sketch to format it for you - see FORMAT_IF_NEEDED
 * below. The nbd-server-ramdisk example provides a suitable export.
 *
 * Note: 107-Arduino-littlefs uses littlefs v2.5 (on-disk version 2.0). It can't
 * mount a filesystem which was created with on-disk version 2.1, e.g. by the
 * LittleFS of the ESP32 core (nbd-server-flash-partition example).
 */
#include <WiFi.h>

#include "NBD_LittleFS.h"

// Set to true to format the export as LittleFS if it can't be mounted (a RAM
// disk is empty after each restart of the server). Set it to false for an
// export with valuable content: a mount error would erase it.
#define FORMAT_IF_NEEDED true

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* server_host = "192.168.1.10";  // nbd server address
const char* export_name = "ram";           // export name on the server

WiFiClient wifi;
NBDClient nbd_client(wifi);
NBDLittleFS lfs_fs(nbd_client);  // block size 4096

void listDir(const char* path) {
  lfs_dir_t dir;
  if (lfs_dir_open(lfs_fs.lfs(), &dir, path) < 0) {
    Serial.println("open dir failed");
    return;
  }
  struct lfs_info info;
  while (lfs_dir_read(lfs_fs.lfs(), &dir, &info) > 0) {
    if (info.type == LFS_TYPE_DIR)
      Serial.printf("  %s/\n", info.name);
    else
      Serial.printf("  %s (%u bytes)\n", info.name, (unsigned)info.size);
  }
  lfs_dir_close(lfs_fs.lfs(), &dir);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected");

  if (!nbd_client.connect(server_host, NBD_DEFAULT_PORT, export_name)) {
    Serial.println("nbd connect failed");
    return;
  }
  // mount the export as LittleFS
  if (!lfs_fs.begin(FORMAT_IF_NEEDED)) {
    Serial.println("mount failed");
    return;
  }

  lfs_t* lfs = lfs_fs.lfs();
  lfs_file_t file;
  // write to a file on the remote export
  if (!lfs_fs.isReadOnly()) {
    const char* text = "hello from nbd-client-littlefs\n";
    if (lfs_file_open(lfs, &file, "test.txt",
                      LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) >= 0) {
      lfs_file_write(lfs, &file, text, strlen(text));
      lfs_file_close(lfs, &file);  // also syncs the data to the server
    } else {
      Serial.println("open for write failed");
    }
  }

  // read it back
  if (lfs_file_open(lfs, &file, "test.txt", LFS_O_RDONLY) >= 0) {
    Serial.printf("test.txt size: %ld bytes\n", (long)lfs_file_size(lfs, &file));
    char buffer[64];
    lfs_ssize_t n;
    while ((n = lfs_file_read(lfs, &file, buffer, sizeof(buffer))) > 0) {
      Serial.write((const uint8_t*)buffer, n);
    }
    lfs_file_close(lfs, &file);
  } else {
    Serial.println("open for read failed");
  }

  Serial.println("Content of /:");
  listDir("/");

  lfs_fs.end();
  nbd_client.disconnect();
}

void loop() {}
