#pragma once
/**
 * @file NBD_LittleFS.h
 * @brief Mounts a remote nbd export as LittleFS: include this instead of NBD.h.
 * Requires the 107-Arduino-littlefs library, which provides the littlefs C API
 * (lfs.h).
 */
#include <string.h>

#include "107-Arduino-littlefs.h"
#include "NBD.h"

namespace nbd {

/**
 * @brief LittleFS on a remote nbd export opened with NBDClient. After begin()
 * the files are accessed with the littlefs C API (lfs_file_open() etc.)
 * using lfs().
 *
 * The NBDClient must be connected before begin() and must outlive this object.
 * A network disk does not need to be erased, so the erase callback does
 * nothing. Each cache miss is a network round trip: the caches have the size
 * of a block.
 */
class NBDLittleFS {
 public:
  /// @param blockSize LittleFS block size: must match an existing filesystem
  explicit NBDLittleFS(NBDClient& client, lfs_size_t blockSize = 4096)
      : client(client), block_size(blockSize) {}

  NBDLittleFS(const NBDLittleFS&) = delete;
  NBDLittleFS& operator=(const NBDLittleFS&) = delete;
  ~NBDLittleFS() { end(); }

  /// Mounts the filesystem: if formatIfNeeded is true, an export which can't
  /// be mounted is formatted (and loses its content!)
  bool begin(bool formatIfNeeded = false) {
    end();
    if (!client.isConnected()) {
      NBD_LOGE("nbd client is not connected");
      return false;
    }
    setupConfig();
    if (lfs_mount(&fs, &cfg) >= 0) {
      is_mounted = true;
      return true;
    }
    if (!formatIfNeeded || client.isReadOnly()) {
      NBD_LOGE("LittleFS mount failed");
      return false;
    }
    NBD_LOGI("Formatting export as LittleFS");
    return format() && lfs_mount(&fs, &cfg) >= 0 && (is_mounted = true);
  }

  /// Unmounts the filesystem (the nbd connection stays open)
  void end() {
    if (is_mounted) lfs_unmount(&fs);
    is_mounted = false;
  }

  /// Formats the export as LittleFS: the filesystem must not be mounted
  bool format() {
    if (is_mounted) return false;
    setupConfig();
    return lfs_format(&fs, &cfg) >= 0;
  }

  /// Returns true if the filesystem is mounted
  bool isMounted() const { return is_mounted; }

  /// Returns true if the export can not be written
  bool isReadOnly() { return client.isReadOnly(); }

  /// The littlefs instance for the lfs_* functions
  lfs_t* lfs() { return &fs; }

 protected:
  NBDClient& client;
  lfs_size_t block_size;
  lfs_t fs;
  struct lfs_config cfg;
  bool is_mounted = false;

  void setupConfig() {
    memset(&cfg, 0, sizeof(cfg));
    cfg.context = &client;
    cfg.read = read;
    cfg.prog = prog;
    cfg.erase = erase;
    cfg.sync = sync;
    cfg.read_size = 512;
    cfg.prog_size = 512;
    cfg.cache_size = block_size;
    cfg.block_size = block_size;
    cfg.block_count = client.size() / block_size;
    cfg.block_cycles = 500;
    cfg.lookahead_size = 32;
  }

  static NBDClient& clientOf(const struct lfs_config* c) {
    return *(NBDClient*)c->context;
  }

  static int read(const struct lfs_config* c, lfs_block_t block,
                  lfs_off_t off, void* buffer, lfs_size_t size) {
    uint64_t offset = (uint64_t)block * c->block_size + off;
    return clientOf(c).read(offset, (uint8_t*)buffer, size) ? 0 : LFS_ERR_IO;
  }

  static int prog(const struct lfs_config* c, lfs_block_t block,
                  lfs_off_t off, const void* buffer, lfs_size_t size) {
    uint64_t offset = (uint64_t)block * c->block_size + off;
    return clientOf(c).write(offset, (const uint8_t*)buffer, size)
               ? 0
               : LFS_ERR_IO;
  }

  static int erase(const struct lfs_config*, lfs_block_t) { return 0; }

  static int sync(const struct lfs_config* c) {
    NBDClient& client = clientOf(c);
    if (!client.canFlush()) return 0;
    return client.flush() ? 0 : LFS_ERR_IO;
  }
};

}  // namespace nbd
