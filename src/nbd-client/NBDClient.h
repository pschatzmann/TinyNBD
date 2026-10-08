#pragma once
/**
 * @file NBDClient.h
 * @brief nbd (Network Block Device) client for any Arduino Client (e.g.
 * WiFiClient, NetworkClient, EthernetClient).
 */
#include <Arduino.h>
#include <Client.h>
#include <string.h>

#include "../nbd-common/NBDLogger.h"
#include "../nbd-common/NBDProtocol.h"

namespace nbd {

/**
 * @brief Implements the nbd client side of the protocol (fixed newstyle
 * handshake with NBD_OPT_EXPORT_NAME, and the transmission phase with simple
 * replies). All calls are synchronous and must be made from the Arduino loop().
 *
 * Usage:
 * @code
 * WiFiClient wifi;
 * NBDClient nbd(wifi);
 * if (nbd.connect("192.168.1.10", NBD_DEFAULT_PORT, "ram")) {
 *   uint8_t buf[512];
 *   nbd.read(0, buf, sizeof(buf));
 *   nbd.disconnect();
 * }
 * @endcode
 */
class NBDClient {
 public:
  /// The Client must stay valid as long as this object is used
  explicit NBDClient(Client& client) : client(client) {}
  NBDClient(const NBDClient&) = delete;
  NBDClient& operator=(const NBDClient&) = delete;

  /// Timeout in ms for waiting on the server
  void setTimeout(uint32_t ms) { timeout_ms = ms; }

  /// Connects to a host name and negotiates the export (empty name = first)
  bool connect(const char* host, uint16_t port = NBD_DEFAULT_PORT,
               const char* export_name = "") {
    if (!client.connect(host, port)) {
      NBD_LOGE("Could not connect to %s:%u", host, (unsigned)port);
      return false;
    }
    return negotiate(export_name);
  }

  /// Connects to an IP address and negotiates the export (empty name = first)
  bool connect(IPAddress ip, uint16_t port = NBD_DEFAULT_PORT,
               const char* export_name = "") {
    if (!client.connect(ip, port)) {
      NBD_LOGE("Could not connect to server");
      return false;
    }
    return negotiate(export_name);
  }

  /// True if the export is negotiated and the connection is still open
  bool isConnected() { return active && client.connected(); }

  /// Size of the export in bytes (valid after a successful connect)
  uint64_t size() const { return export_size; }

  /// True if the server exports the device read-only
  bool isReadOnly() const { return (transmission_flags & NBD_FLAG_READ_ONLY) != 0; }

  /// True if the server supports flush()
  bool canFlush() const { return (transmission_flags & NBD_FLAG_SEND_FLUSH) != 0; }

  /// True if the server supports trim()
  bool canTrim() const { return (transmission_flags & NBD_FLAG_SEND_TRIM) != 0; }

  /// True if the server supports writeZeroes()
  bool canWriteZeroes() const {
    return (transmission_flags & NBD_FLAG_SEND_WRITE_ZEROES) != 0;
  }

  /// Error code of the last failed request (NBD_OK if none)
  uint32_t lastError() const { return last_error; }

  /// Smallest block size of the export (1 if the server didn't report it)
  uint32_t minBlockSize() const { return min_block_size; }

  /// Preferred block size of the export (4096 if the server didn't report it)
  uint32_t preferredBlockSize() const { return preferred_block_size; }

  /// Largest request payload the server accepts in bytes (0 = no limit
  /// reported)
  uint32_t maxPayload() const { return max_payload; }

  /// Reads len bytes from offset into data
  bool read(uint64_t offset, uint8_t* data, size_t len) {
    if (!isConnected()) return false;
    uint64_t handle = sendRequest(NBD_CMD_READ, 0, offset, len, nullptr);
    if (handle == 0) return false;
    return receiveReply(handle, data, len);
  }

  /// Writes len bytes from data to offset. fua requests a durable write.
  bool write(uint64_t offset, const uint8_t* data, size_t len,
             bool fua = false) {
    if (!isConnected() || isReadOnly()) return false;
    uint16_t flags = fua ? NBD_CMD_FLAG_FUA : 0;
    uint64_t handle = sendRequest(NBD_CMD_WRITE, flags, offset, len, data);
    if (handle == 0) return false;
    return receiveReply(handle, nullptr, 0);
  }

  /// Commits all pending writes on the server
  bool flush() {
    if (!isConnected()) return false;
    uint64_t handle = sendRequest(NBD_CMD_FLUSH, 0, 0, 0, nullptr);
    if (handle == 0) return false;
    return receiveReply(handle, nullptr, 0);
  }

  /// Discards the content of the range (server must support trim)
  bool trim(uint64_t offset, uint32_t len) {
    if (!isConnected() || isReadOnly() || !canTrim()) return false;
    uint64_t handle = sendRequest(NBD_CMD_TRIM, 0, offset, len, nullptr);
    if (handle == 0) return false;
    return receiveReply(handle, nullptr, 0);
  }

  /// Writes zeroes to the range (server must support write zeroes)
  bool writeZeroes(uint64_t offset, uint32_t len) {
    if (!isConnected() || isReadOnly() || !canWriteZeroes()) return false;
    uint64_t handle =
        sendRequest(NBD_CMD_WRITE_ZEROES, 0, offset, len, nullptr);
    if (handle == 0) return false;
    return receiveReply(handle, nullptr, 0);
  }

  /// Sends NBD_CMD_DISC and closes the connection
  void disconnect() {
    if (active) {
      sendRequest(NBD_CMD_DISC, 0, 0, 0, nullptr);
    }
    client.stop();
    active = false;
  }

  /// Block size in bytes used by the block methods (the server's preferred
  /// size, 4096 if unknown)
  uint32_t blockSize() const {
    return preferred_block_size > 0 ? preferred_block_size : 4096;
  }

  /// Number of whole blocks in the export. A trailing partial block is not
  /// counted.
  uint64_t blockCount() const { return export_size / blockSize(); }

  /// Reads count blocks starting at block into data (count * blockSize()
  /// bytes). Requests larger than maxPayload() are split.
  bool readBlocks(uint64_t block, uint8_t* data, uint32_t count) {
    if (!inBlockRange(block, count)) return false;
    return readChunks(block * blockSize(), data,
                      (size_t)count * blockSize());
  }

  /// Writes count blocks from data (count * blockSize() bytes) at block.
  /// Requests larger than maxPayload() are split.
  bool writeBlocks(uint64_t block, const uint8_t* data, uint32_t count) {
    if (!inBlockRange(block, count)) return false;
    return writeChunks(block * blockSize(), data,
                       (size_t)count * blockSize());
  }

  /// Discards count blocks starting at block (requires canTrim())
  bool trimBlocks(uint64_t block, uint32_t count) {
    if (!inBlockRange(block, count) || !canTrim()) return false;
    return forEachChunk(block * blockSize(), (uint64_t)count * blockSize(),
                        [this](uint64_t o, uint32_t n) {
                          return trim(o, n);
                        });
  }

  /// Writes zeroes to count blocks starting at block (requires
  /// canWriteZeroes())
  bool writeZeroesBlocks(uint64_t block, uint32_t count) {
    if (!inBlockRange(block, count) || !canWriteZeroes()) return false;
    return forEachChunk(block * blockSize(), (uint64_t)count * blockSize(),
                        [this](uint64_t o, uint32_t n) {
                          return writeZeroes(o, n);
                        });
  }

 protected:
  Client& client;
  uint32_t timeout_ms = 10000;
  bool active = false;
  uint64_t export_size = 0;
  uint16_t transmission_flags = 0;
  uint64_t handle_counter = 0;
  uint32_t last_error = NBD_OK;
  bool no_zeroes = false;
  uint32_t min_block_size = 1;
  uint32_t preferred_block_size = 4096;
  uint32_t max_payload = 0;

  bool fail(const char* msg) {
    NBD_LOGE("%s", msg);
    client.stop();
    active = false;
    return false;
  }

  /// Fixed newstyle handshake: tries NBD_OPT_GO (export info and block sizes)
  /// and falls back to NBD_OPT_EXPORT_NAME if the server refuses it
  bool negotiate(const char* export_name) {
    active = false;
    if (export_name == nullptr) export_name = "";

    // server hello: "NBDMAGIC", "IHAVEOPT", handshake flags
    uint8_t hello[18];
    if (!readFully(hello, sizeof(hello))) return fail("No server handshake");
    if (getBE64(hello) != NBD_INIT_MAGIC) return fail("Invalid init magic");
    if (getBE64(hello + 8) != NBD_OPTS_MAGIC)
      return fail("Server does not support newstyle options");
    uint16_t server_flags = getBE16(hello + 16);
    if (!(server_flags & NBD_FLAG_FIXED_NEWSTYLE))
      return fail("Server does not support fixed newstyle");
    no_zeroes = (server_flags & NBD_FLAG_NO_ZEROES) != 0;

    // client flags
    uint8_t cflags[4];
    putBE32(cflags, NBD_FLAG_C_FIXED_NEWSTYLE |
                        (no_zeroes ? NBD_FLAG_C_NO_ZEROES : 0));
    if (!writeFully(cflags, sizeof(cflags)))
      return fail("Could not send client flags");

    size_t name_len = strlen(export_name);
    if (name_len > NBD_MAX_NAME_LEN) return fail("Export name too long");

    // defaults for servers which don't report block sizes
    min_block_size = 1;
    preferred_block_size = 4096;
    max_payload = 0;

    int rc = negotiateGo(export_name, name_len);
    if (rc < 0) return fail("Export negotiation failed");
    if (rc == 0) {
      // the server refused NBD_OPT_GO: the connection is still in the
      // option phase, so use NBD_OPT_EXPORT_NAME
      NBD_LOGI("NBD_OPT_GO refused, using NBD_OPT_EXPORT_NAME");
      if (!negotiateExportName(export_name, name_len))
        return fail("Export negotiation failed");
    }

    handle_counter = 0;
    last_error = NBD_OK;
    active = true;
    NBD_LOGI("Connected to export '%s': %llu bytes, block size %lu..%lu",
             export_name, (unsigned long long)export_size,
             (unsigned long)min_block_size,
             (unsigned long)preferred_block_size);
    return true;
  }

  /// NBD_OPT_GO: selects the export and requests its block sizes. Returns 1
  /// on success, 0 if the server replied with an error, -1 on protocol or
  /// connection failure
  int negotiateGo(const char* export_name, size_t name_len) {
    // option data: name length, name, number of info requests, info types
    uint8_t opt[16];
    putBE64(opt, NBD_OPTS_MAGIC);
    putBE32(opt + 8, NBD_OPT_GO);
    putBE32(opt + 12, (uint32_t)(4 + name_len + 2 + 2));
    uint8_t name_len_field[4];
    putBE32(name_len_field, (uint32_t)name_len);
    uint8_t info_req[4];
    putBE16(info_req, 1);  // one info request
    putBE16(info_req + 2, NBD_INFO_BLOCK_SIZE);
    if (!writeFully(opt, sizeof(opt)) ||
        !writeFully(name_len_field, sizeof(name_len_field)) ||
        !writeFully((const uint8_t*)export_name, name_len) ||
        !writeFully(info_req, sizeof(info_req)))
      return -1;

    // replies: NBD_REP_INFO messages, then NBD_REP_ACK (or an error)
    bool got_export = false;
    while (true) {
      uint8_t buf[16];
      uint32_t type = 0;
      size_t payload_len = 0;
      if (!readOptionReply(NBD_OPT_GO, type, buf, sizeof(buf), payload_len))
        return -1;
      if (type == NBD_REP_ACK) return got_export ? 1 : -1;
      if (type == NBD_REP_INFO) {
        if (payload_len < 2) return -1;
        uint16_t info = getBE16(buf);
        if (info == NBD_INFO_EXPORT && payload_len >= 12) {
          export_size = getBE64(buf + 2);
          transmission_flags = getBE16(buf + 10);
          got_export = true;
        } else if (info == NBD_INFO_BLOCK_SIZE && payload_len >= 14) {
          min_block_size = getBE32(buf + 2);
          preferred_block_size = getBE32(buf + 6);
          max_payload = getBE32(buf + 10);
        }
        continue;
      }
      if (type & NBD_REP_FLAG_ERROR) return 0;
      return -1;
    }
  }

  /// NBD_OPT_EXPORT_NAME: selects the export. The reply has no option header
  /// and doesn't report block sizes.
  bool negotiateExportName(const char* export_name, size_t name_len) {
    uint8_t opt[16];
    putBE64(opt, NBD_OPTS_MAGIC);
    putBE32(opt + 8, NBD_OPT_EXPORT_NAME);
    putBE32(opt + 12, (uint32_t)name_len);
    if (!writeFully(opt, sizeof(opt)) ||
        !writeFully((const uint8_t*)export_name, name_len))
      return false;

    // reply: size (8), transmission flags (2), optional 124 zero bytes
    uint8_t reply[10 + 124];
    size_t reply_len = no_zeroes ? 10 : sizeof(reply);
    if (!readFully(reply, reply_len)) return false;
    export_size = getBE64(reply);
    transmission_flags = getBE16(reply + 8);
    return true;
  }

  /// Reads an option reply header and up to buf_size bytes of its payload;
  /// the rest of the payload is discarded. payload_len is the full length.
  bool readOptionReply(uint32_t option, uint32_t& type, uint8_t* buf,
                       size_t buf_size, size_t& payload_len) {
    uint8_t hdr[20];
    if (!readFully(hdr, sizeof(hdr))) return false;
    if (getBE64(hdr) != NBD_REP_MAGIC) return false;
    if (getBE32(hdr + 8) != option) return false;
    type = getBE32(hdr + 12);
    payload_len = getBE32(hdr + 16);
    size_t keep = payload_len < buf_size ? payload_len : buf_size;
    if (keep > 0 && !readFully(buf, keep)) return false;
    return skip(payload_len - keep);
  }

  /// Reads and discards len bytes
  bool skip(size_t len) {
    uint8_t tmp[32];
    while (len > 0) {
      size_t n = len < sizeof(tmp) ? len : sizeof(tmp);
      if (!readFully(tmp, n)) return false;
      len -= n;
    }
    return true;
  }

  /// Sends a request header and the optional payload; returns its handle
  /// (0 on failure)
  uint64_t sendRequest(uint16_t type, uint16_t flags, uint64_t offset,
                       uint64_t len, const uint8_t* payload) {
    if (len > 0xFFFFFFFFULL) return 0;
    uint64_t handle = ++handle_counter;
    uint8_t req[NBD_REQUEST_SIZE];
    putBE32(req, NBD_REQUEST_MAGIC);
    putBE16(req + 4, flags);
    putBE16(req + 6, type);
    putBE64(req + 8, handle);
    putBE64(req + 16, offset);
    putBE32(req + 24, (uint32_t)len);
    if (!writeFully(req, sizeof(req))) {
      NBD_LOGE("Could not send request");
      active = false;
      return 0;
    }
    // only write requests carry data
    if (type == NBD_CMD_WRITE && len > 0 && payload != nullptr) {
      if (!writeFully(payload, (size_t)len)) {
        NBD_LOGE("Could not send write data");
        active = false;
        return 0;
      }
    }
    return handle;
  }

  /// Reads the simple reply for handle and, for reads, the data
  bool receiveReply(uint64_t handle, uint8_t* data, size_t len) {
    uint8_t hdr[NBD_SIMPLE_REPLY_SIZE];
    if (!readFully(hdr, sizeof(hdr))) {
      NBD_LOGE("No reply from server");
      active = false;
      return false;
    }
    if (getBE32(hdr) != NBD_SIMPLE_REPLY_MAGIC) {
      NBD_LOGE("Invalid reply magic");
      active = false;
      return false;
    }
    if (getBE64(hdr + 8) != handle) {
      NBD_LOGE("Unexpected reply handle");
      active = false;
      return false;
    }
    last_error = getBE32(hdr + 4);
    if (last_error != NBD_OK) {
      NBD_LOGE("Server returned error %lu", (unsigned long)last_error);
      return false;
    }
    // the server only sends data for successful reads
    if (data != nullptr && len > 0) {
      if (!readFully(data, len)) {
        NBD_LOGE("Could not read data");
        active = false;
        return false;
      }
    }
    return true;
  }

  /// True if the block range [block, block + count) lies inside the export
  bool inBlockRange(uint64_t block, uint32_t count) const {
    return count > 0 && block <= blockCount() && count <= blockCount() - block;
  }

  /// Reads len bytes at offset, one request per maximum payload
  bool readChunks(uint64_t offset, uint8_t* data, size_t len) {
    size_t max = maxPayload() > 0 ? (size_t)maxPayload() : (size_t)0xFFFFFFFFUL;
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!read(offset, data, n)) return false;
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  /// Writes len bytes at offset, one request per maximum payload
  bool writeChunks(uint64_t offset, const uint8_t* data, size_t len) {
    size_t max = maxPayload() > 0 ? (size_t)maxPayload() : (size_t)0xFFFFFFFFUL;
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!write(offset, data, n)) return false;
      offset += n;
      data += n;
      len -= n;
    }
    return true;
  }

  /// The protocol length field is 32 bit: split larger ranges into chunks
  template <class F>
  bool forEachChunk(uint64_t offset, uint64_t len, F op) {
    const uint64_t max_len = 0xFFFFFFFFULL;
    while (len > 0) {
      uint32_t n = len < max_len ? (uint32_t)len : (uint32_t)max_len;
      if (!op(offset, n)) return false;
      offset += n;
      len -= n;
    }
    return true;
  }

  bool readFully(uint8_t* data, size_t len) {
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

  bool writeFully(const uint8_t* data, size_t len) {
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
