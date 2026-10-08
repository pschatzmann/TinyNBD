#pragma once
#include <Arduino.h>
#include <Client.h>

#include "BlockDevice.h"
#include "../nbd-common/NBDLogger.h"
#include "../nbd-common/NBDProtocol.h"

namespace nbd {

/// An exported block device
struct NBDExport {
  const char* name = nullptr;
  const char* description = nullptr;
  BlockDevice* device = nullptr;
  bool read_only = false;
};

/**
 * @brief Implements the nbd protocol (fixed newstyle handshake and
 * transmission phase with simple replies) for a single connection on any
 * Arduino Client.
 */
class NBDSession {
 public:
  NBDSession() = default;
  NBDSession(const NBDSession&) = delete;
  NBDSession& operator=(const NBDSession&) = delete;
  ~NBDSession() { releaseBuffer(); }

  /// Defines the available exports
  void setExports(NBDExport* exports, int count) {
    this->exports = exports;
    export_count = count;
  }

  /// Defines the transfer chunk size (power of 2, default 4096)
  void setBufferSize(size_t size) {
    if (size != buffer_size) {
      releaseBuffer();
      buffer_size = size;
    }
  }

  /// Timeout in ms for receiving the remaining data of a message
  void setTimeout(uint32_t ms) { timeout_ms = ms; }

  /// Returns the active export (after a successful handshake)
  NBDExport* activeExport() { return current; }

  /**
   * @brief Performs the handshake and option haggling. Returns true if the
   * transmission phase was entered, false if the connection should be closed.
   */
  bool handshake(Client& client) {
    current = nullptr;
    if (!allocateBuffer()) {
      NBD_LOGE("Not enough memory for buffer of %u bytes",
               (unsigned)buffer_size);
      return false;
    }
    uint8_t hello[18];
    putBE64(hello, NBD_INIT_MAGIC);
    putBE64(hello + 8, NBD_OPTS_MAGIC);
    putBE16(hello + 16, NBD_FLAG_FIXED_NEWSTYLE | NBD_FLAG_NO_ZEROES);
    if (!writeFully(client, hello, sizeof(hello))) return false;

    uint8_t tmp[16];
    if (!readFully(client, tmp, 4)) return false;
    uint32_t client_flags = getBE32(tmp);
    if (client_flags & ~(NBD_FLAG_C_FIXED_NEWSTYLE | NBD_FLAG_C_NO_ZEROES)) {
      NBD_LOGE("Unsupported client flags: 0x%lx", (unsigned long)client_flags);
      return false;
    }
    no_zeroes = client_flags & NBD_FLAG_C_NO_ZEROES;

    while (true) {
      if (!readFully(client, tmp, 16)) return false;
      if (getBE64(tmp) != NBD_OPTS_MAGIC) {
        NBD_LOGE("Invalid option magic");
        return false;
      }
      uint32_t option = getBE32(tmp + 8);
      uint32_t len = getBE32(tmp + 12);
      NBD_LOGD("Option %lu, len %lu", (unsigned long)option,
               (unsigned long)len);

      // the option data is stored in the buffer
      if (len > buffer_size - 1) {
        if (!drain(client, len)) return false;
        if (!sendOptionReply(client, option, NBD_REP_ERR_TOO_BIG)) return false;
        continue;
      }
      uint8_t* data = buffer;
      if (!readFully(client, data, len)) return false;

      switch (option) {
        case NBD_OPT_EXPORT_NAME: {
          data[len] = 0;
          current = findExport((const char*)data);
          if (current == nullptr) {
            NBD_LOGE("Unknown export: '%s'", (const char*)data);
            return false;
          }
          uint8_t reply[10 + 124] = {0};
          putBE64(reply, current->device->size());
          putBE16(reply + 8, transmissionFlags());
          if (!writeFully(client, reply, no_zeroes ? 10 : sizeof(reply)))
            return false;
          logStart();
          return true;
        }

        case NBD_OPT_ABORT:
          sendOptionReply(client, option, NBD_REP_ACK);
          return false;

        case NBD_OPT_LIST:
          if (len != 0) {
            if (!sendOptionReply(client, option, NBD_REP_ERR_INVALID))
              return false;
            break;
          }
          if (!sendExportList(client)) return false;
          break;

        case NBD_OPT_INFO:
        case NBD_OPT_GO: {
          int rc = processInfo(client, option, data, len);
          if (rc < 0) return false;
          if (rc > 0 && option == NBD_OPT_GO) {
            logStart();
            return true;
          }
          break;
        }

        default:
          if (!sendOptionReply(client, option, NBD_REP_ERR_UNSUP))
            return false;
          break;
      }
    }
  }

  /**
   * @brief Processes a single request of the transmission phase. Returns
   * false if the connection should be closed.
   */
  bool processRequest(Client& client) {
    if (current == nullptr) return false;
    uint8_t req[NBD_REQUEST_SIZE];
    if (!readFully(client, req, sizeof(req))) return false;
    if (getBE32(req) != NBD_REQUEST_MAGIC) {
      NBD_LOGE("Invalid request magic");
      return false;
    }
    uint16_t flags = getBE16(req + 4);
    uint16_t type = getBE16(req + 6);
    const uint8_t* handle = req + 8;
    uint64_t offset = getBE64(req + 16);
    uint32_t len = getBE32(req + 24);
    BlockDevice& dev = *current->device;
    uint64_t dev_size = dev.size();
    bool in_range = offset <= dev_size && len <= dev_size - offset;
    bool read_only = isReadOnly();
    NBD_LOGD("Cmd %u flags 0x%x offset %llu len %lu", type, flags,
             (unsigned long long)offset, (unsigned long)len);

    switch (type) {
      case NBD_CMD_READ:
        if (!in_range) return sendReply(client, handle, NBD_EINVAL);
        return processRead(client, handle, offset, len);

      case NBD_CMD_WRITE: {
        uint32_t error = NBD_OK;
        if (read_only)
          error = NBD_EPERM;
        else if (!in_range)
          error = NBD_ENOSPC;
        if (error != NBD_OK) {
          if (!drain(client, len)) return false;
          return sendReply(client, handle, error);
        }
        if (!receiveWrite(client, dev, offset, len, error)) return false;
        if (error == NBD_OK && (flags & NBD_CMD_FLAG_FUA) && !dev.flush())
          error = NBD_EIO;
        return sendReply(client, handle, error);
      }

      case NBD_CMD_DISC:
        NBD_LOGI("Client disconnected");
        dev.flush();
        return false;

      case NBD_CMD_FLUSH:
        return sendReply(client, handle, dev.flush() ? NBD_OK : NBD_EIO);

      case NBD_CMD_TRIM: {
        uint32_t error = NBD_OK;
        if (read_only)
          error = NBD_EPERM;
        else if (!dev.canTrim() || !in_range)
          error = NBD_EINVAL;
        else if (!dev.trim(offset, len))
          error = NBD_EIO;
        else if ((flags & NBD_CMD_FLAG_FUA) && !dev.flush())
          error = NBD_EIO;
        return sendReply(client, handle, error);
      }

      case NBD_CMD_WRITE_ZEROES: {
        uint32_t error = NBD_OK;
        if (read_only)
          error = NBD_EPERM;
        else if (!in_range)
          error = NBD_ENOSPC;
        else if (!dev.writeZeroes(offset, len))
          error = NBD_EIO;
        else if ((flags & NBD_CMD_FLAG_FUA) && !dev.flush())
          error = NBD_EIO;
        return sendReply(client, handle, error);
      }

      default:
        NBD_LOGE("Unsupported command: %u", type);
        return sendReply(client, handle, NBD_EINVAL);
    }
  }

  /// Flushes the active export
  void close() {
    if (current != nullptr) current->device->flush();
    current = nullptr;
  }

 protected:
  /// room for the reply header in front of the data
  static constexpr size_t HEADER_ROOM = NBD_SIMPLE_REPLY_SIZE;
  NBDExport* exports = nullptr;
  int export_count = 0;
  NBDExport* current = nullptr;
  uint8_t* buffer = nullptr;  // data area; header room is in front of it
  size_t buffer_size = 4096;
  uint32_t timeout_ms = 10000;
  bool no_zeroes = false;

  bool allocateBuffer() {
    if (buffer_size < 512) buffer_size = 512;
    if (buffer == nullptr) {
      uint8_t* mem = (uint8_t*)malloc(buffer_size + HEADER_ROOM);
      if (mem == nullptr) return false;
      buffer = mem + HEADER_ROOM;
    }
    return true;
  }

  // buffer points behind the header room: free the original pointer
  void releaseBuffer() {
    if (buffer != nullptr) free(buffer - HEADER_ROOM);
    buffer = nullptr;
  }

  bool isReadOnly() {
    return current->read_only || current->device->isReadOnly();
  }

  uint16_t transmissionFlags() {
    uint16_t flags = NBD_FLAG_HAS_FLAGS | NBD_FLAG_SEND_FLUSH |
                     NBD_FLAG_SEND_FUA;
    if (isReadOnly()) {
      flags |= NBD_FLAG_READ_ONLY;
    } else {
      flags |= NBD_FLAG_SEND_WRITE_ZEROES;
      if (current->device->canTrim()) flags |= NBD_FLAG_SEND_TRIM;
    }
    return flags;
  }

  /// Finds an export by name: an empty name selects the first export
  NBDExport* findExport(const char* name) {
    if (export_count == 0) return nullptr;
    if (name == nullptr || *name == 0) return &exports[0];
    for (int j = 0; j < export_count; j++) {
      if (strcmp(exports[j].name, name) == 0) return &exports[j];
    }
    return nullptr;
  }

  void logStart() {
    NBD_LOGI("Export '%s' started: %llu bytes%s", current->name,
             (unsigned long long)current->device->size(),
             isReadOnly() ? " (read-only)" : "");
  }

  /// NBD_OPT_INFO / NBD_OPT_GO: returns 1 if ok, 0 on error reply, -1 to
  /// close the connection
  int processInfo(Client& client, uint32_t option, uint8_t* data,
                  uint32_t len) {
    if (len < 6) return replyResult(client, option, NBD_REP_ERR_INVALID);
    uint32_t name_len = getBE32(data);
    if (name_len > len - 6)
      return replyResult(client, option, NBD_REP_ERR_INVALID);
    uint16_t info_count = getBE16(data + 4 + name_len);
    if (4 + name_len + 2 + 2 * (uint32_t)info_count != len)
      return replyResult(client, option, NBD_REP_ERR_INVALID);

    // copy the requested info types before we terminate the name
    const uint8_t* infos = data + 4 + name_len + 2;
    bool want_name = false, want_desc = false, want_block = false;
    for (int j = 0; j < info_count; j++) {
      uint16_t info = getBE16(infos + 2 * j);
      if (info == NBD_INFO_NAME) want_name = true;
      if (info == NBD_INFO_DESCRIPTION) want_desc = true;
      if (info == NBD_INFO_BLOCK_SIZE) want_block = true;
    }
    char* name = (char*)data + 4;
    name[name_len] = 0;

    NBDExport* exp = findExport(name);
    if (exp == nullptr) {
      NBD_LOGE("Unknown export: '%s'", name);
      return replyResult(client, option, NBD_REP_ERR_UNKNOWN);
    }
    current = exp;

    uint8_t info[12];
    putBE16(info, NBD_INFO_EXPORT);
    putBE64(info + 2, exp->device->size());
    putBE16(info + 10, transmissionFlags());
    if (!sendOptionReply(client, option, NBD_REP_INFO, info, 12)) return -1;

    if (want_name && !sendInfoString(client, option, NBD_INFO_NAME, exp->name))
      return -1;
    if (want_desc && exp->description != nullptr &&
        !sendInfoString(client, option, NBD_INFO_DESCRIPTION,
                        exp->description))
      return -1;
    if (want_block) {
      uint32_t min_bs = exp->device->minBlockSize();
      uint32_t pref_bs = exp->device->preferredBlockSize();
      if (pref_bs < 512) pref_bs = 512;
      if (pref_bs < min_bs) pref_bs = min_bs;
      uint8_t bs[14];
      putBE16(bs, NBD_INFO_BLOCK_SIZE);
      putBE32(bs + 2, min_bs);
      putBE32(bs + 6, pref_bs);
      putBE32(bs + 10, 32UL * 1024 * 1024);  // max payload of 32 MiB
      if (!sendOptionReply(client, option, NBD_REP_INFO, bs, sizeof(bs)))
        return -1;
    }
    if (!sendOptionReply(client, option, NBD_REP_ACK)) return -1;
    if (option == NBD_OPT_INFO) current = nullptr;
    return 1;
  }

  int replyResult(Client& client, uint32_t option, uint32_t reply) {
    return sendOptionReply(client, option, reply) ? 0 : -1;
  }

  bool sendInfoString(Client& client, uint32_t option, uint16_t type,
                      const char* str) {
    size_t len = strlen(str);
    uint8_t hdr[2];
    putBE16(hdr, type);
    return sendOptionReply(client, option, NBD_REP_INFO, hdr, 2,
                           (const uint8_t*)str, len);
  }

  bool sendExportList(Client& client) {
    for (int j = 0; j < export_count; j++) {
      const char* name = exports[j].name;
      const char* desc = exports[j].description;
      size_t name_len = strlen(name);
      uint8_t len[4];
      putBE32(len, name_len);
      // reply data: name length, name, optional description
      if (!sendOptionReply(client, NBD_OPT_LIST, NBD_REP_SERVER, len, 4,
                           (const uint8_t*)name, name_len,
                           (const uint8_t*)desc, desc ? strlen(desc) : 0))
        return false;
    }
    return sendOptionReply(client, NBD_OPT_LIST, NBD_REP_ACK);
  }

  bool sendOptionReply(Client& client, uint32_t option, uint32_t type,
                       const uint8_t* d1 = nullptr, size_t l1 = 0,
                       const uint8_t* d2 = nullptr, size_t l2 = 0,
                       const uint8_t* d3 = nullptr, size_t l3 = 0) {
    uint8_t hdr[20];
    putBE64(hdr, NBD_REP_MAGIC);
    putBE32(hdr + 8, option);
    putBE32(hdr + 12, type);
    putBE32(hdr + 16, l1 + l2 + l3);
    return writeFully(client, hdr, sizeof(hdr)) &&
           writeFully(client, d1, l1) && writeFully(client, d2, l2) &&
           writeFully(client, d3, l3);
  }

  /// Simple reply without payload
  bool sendReply(Client& client, const uint8_t* handle, uint32_t error) {
    uint8_t hdr[NBD_SIMPLE_REPLY_SIZE];
    setupReply(hdr, handle, error);
    if (error != NBD_OK) NBD_LOGE("Request failed with error %lu",
                                  (unsigned long)error);
    return writeFully(client, hdr, sizeof(hdr));
  }

  void setupReply(uint8_t* hdr, const uint8_t* handle, uint32_t error) {
    putBE32(hdr, NBD_SIMPLE_REPLY_MAGIC);
    putBE32(hdr + 4, error);
    memcpy(hdr + 8, handle, 8);
  }

  /// Size of the next chunk, so that chunks are aligned to the buffer size
  size_t chunkSize(uint64_t offset, uint32_t remaining) {
    size_t n = buffer_size - (offset % buffer_size);
    return remaining < n ? remaining : n;
  }

  bool processRead(Client& client, const uint8_t* handle, uint64_t offset,
                   uint32_t len) {
    BlockDevice& dev = *current->device;
    // read the first chunk before sending the header so that we can still
    // report an error
    size_t n = chunkSize(offset, len);
    if (!dev.read(offset, buffer, n)) return sendReply(client, handle, NBD_EIO);
    // send the header together with the first chunk
    uint8_t* hdr = buffer - HEADER_ROOM;
    setupReply(hdr, handle, NBD_OK);
    if (!writeFully(client, hdr, HEADER_ROOM + n)) return false;
    offset += n;
    len -= n;
    while (len > 0) {
      n = chunkSize(offset, len);
      if (!dev.read(offset, buffer, n)) {
        // header was already sent: we can only disconnect
        NBD_LOGE("Read failed at %llu", (unsigned long long)offset);
        return false;
      }
      if (!writeFully(client, buffer, n)) return false;
      offset += n;
      len -= n;
    }
    return true;
  }

  /// Receives the write payload: returns false if the connection failed
  bool receiveWrite(Client& client, BlockDevice& dev, uint64_t offset,
                    uint32_t len, uint32_t& error) {
    while (len > 0) {
      size_t n = chunkSize(offset, len);
      if (!readFully(client, buffer, n)) return false;
      if (error == NBD_OK && !dev.write(offset, buffer, n)) error = NBD_EIO;
      offset += n;
      len -= n;
    }
    return true;
  }

  /// Reads and ignores len bytes
  bool drain(Client& client, uint32_t len) {
    while (len > 0) {
      size_t n = len < buffer_size ? len : buffer_size;
      if (!readFully(client, buffer, n)) return false;
      len -= n;
    }
    return true;
  }

  bool readFully(Client& client, uint8_t* data, size_t len) {
    uint32_t start = millis();
    while (len > 0) {
      int avail = client.available();
      if (avail > 0) {
        size_t req = (size_t)avail < len ? (size_t)avail : len;
        int n = client.read(data, req);
        if (n > 0) {
          data += n;
          len -= n;
          start = millis();
          continue;
        }
      }
      if (!client.connected()) return false;
      if (millis() - start > timeout_ms) {
        NBD_LOGE("Read timeout");
        return false;
      }
      delay(1);
    }
    return true;
  }

  bool writeFully(Client& client, const uint8_t* data, size_t len) {
    uint32_t start = millis();
    while (len > 0) {
      size_t n = client.write(data, len);
      if (n > 0) {
        data += n;
        len -= n;
        start = millis();
        continue;
      }
      if (!client.connected()) return false;
      if (millis() - start > timeout_ms) {
        NBD_LOGE("Write timeout");
        return false;
      }
      delay(1);
    }
    return true;
  }
};

}  // namespace nbd
