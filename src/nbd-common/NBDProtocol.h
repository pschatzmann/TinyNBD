#pragma once
/**
 * @file NBDProtocol.h
 * @brief Constants and big-endian helpers for the nbd (Network Block Device)
 * protocol. See https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md
 */
#include <stddef.h>
#include <stdint.h>

namespace nbd {

/// Default TCP port of the nbd protocol
static constexpr uint16_t NBD_DEFAULT_PORT = 10809;

// ---- Magic numbers
static constexpr uint64_t NBD_INIT_MAGIC = 0x4e42444d41474943ULL;  // "NBDMAGIC"
static constexpr uint64_t NBD_OPTS_MAGIC = 0x49484156454F5054ULL;  // "IHAVEOPT"
static constexpr uint64_t NBD_REP_MAGIC = 0x0003e889045565a9ULL;
static constexpr uint32_t NBD_REQUEST_MAGIC = 0x25609513;
static constexpr uint32_t NBD_SIMPLE_REPLY_MAGIC = 0x67446698;

// ---- Handshake flags (server)
static constexpr uint16_t NBD_FLAG_FIXED_NEWSTYLE = 1 << 0;
static constexpr uint16_t NBD_FLAG_NO_ZEROES = 1 << 1;

// ---- Client flags
static constexpr uint32_t NBD_FLAG_C_FIXED_NEWSTYLE = 1 << 0;
static constexpr uint32_t NBD_FLAG_C_NO_ZEROES = 1 << 1;

// ---- Transmission flags
static constexpr uint16_t NBD_FLAG_HAS_FLAGS = 1 << 0;
static constexpr uint16_t NBD_FLAG_READ_ONLY = 1 << 1;
static constexpr uint16_t NBD_FLAG_SEND_FLUSH = 1 << 2;
static constexpr uint16_t NBD_FLAG_SEND_FUA = 1 << 3;
static constexpr uint16_t NBD_FLAG_ROTATIONAL = 1 << 4;
static constexpr uint16_t NBD_FLAG_SEND_TRIM = 1 << 5;
static constexpr uint16_t NBD_FLAG_SEND_WRITE_ZEROES = 1 << 6;
static constexpr uint16_t NBD_FLAG_SEND_DF = 1 << 7;
static constexpr uint16_t NBD_FLAG_CAN_MULTI_CONN = 1 << 8;
static constexpr uint16_t NBD_FLAG_SEND_RESIZE = 1 << 9;
static constexpr uint16_t NBD_FLAG_SEND_CACHE = 1 << 10;
static constexpr uint16_t NBD_FLAG_SEND_FAST_ZERO = 1 << 11;

// ---- Options
enum NBDOption : uint32_t {
  NBD_OPT_EXPORT_NAME = 1,
  NBD_OPT_ABORT = 2,
  NBD_OPT_LIST = 3,
  NBD_OPT_PEEK_EXPORT = 4,
  NBD_OPT_STARTTLS = 5,
  NBD_OPT_INFO = 6,
  NBD_OPT_GO = 7,
  NBD_OPT_STRUCTURED_REPLY = 8,
  NBD_OPT_LIST_META_CONTEXT = 9,
  NBD_OPT_SET_META_CONTEXT = 10,
  NBD_OPT_EXTENDED_HEADERS = 11,
};

// ---- Option reply types
static constexpr uint32_t NBD_REP_ACK = 1;
static constexpr uint32_t NBD_REP_SERVER = 2;
static constexpr uint32_t NBD_REP_INFO = 3;
static constexpr uint32_t NBD_REP_FLAG_ERROR = 1UL << 31;
static constexpr uint32_t NBD_REP_ERR_UNSUP = NBD_REP_FLAG_ERROR | 1;
static constexpr uint32_t NBD_REP_ERR_POLICY = NBD_REP_FLAG_ERROR | 2;
static constexpr uint32_t NBD_REP_ERR_INVALID = NBD_REP_FLAG_ERROR | 3;
static constexpr uint32_t NBD_REP_ERR_PLATFORM = NBD_REP_FLAG_ERROR | 4;
static constexpr uint32_t NBD_REP_ERR_TLS_REQD = NBD_REP_FLAG_ERROR | 5;
static constexpr uint32_t NBD_REP_ERR_UNKNOWN = NBD_REP_FLAG_ERROR | 6;
static constexpr uint32_t NBD_REP_ERR_SHUTDOWN = NBD_REP_FLAG_ERROR | 7;
static constexpr uint32_t NBD_REP_ERR_BLOCK_SIZE_REQD = NBD_REP_FLAG_ERROR | 8;
static constexpr uint32_t NBD_REP_ERR_TOO_BIG = NBD_REP_FLAG_ERROR | 9;

// ---- Info types (NBD_OPT_INFO / NBD_OPT_GO)
static constexpr uint16_t NBD_INFO_EXPORT = 0;
static constexpr uint16_t NBD_INFO_NAME = 1;
static constexpr uint16_t NBD_INFO_DESCRIPTION = 2;
static constexpr uint16_t NBD_INFO_BLOCK_SIZE = 3;

// ---- Commands
enum NBDCommand : uint16_t {
  NBD_CMD_READ = 0,
  NBD_CMD_WRITE = 1,
  NBD_CMD_DISC = 2,
  NBD_CMD_FLUSH = 3,
  NBD_CMD_TRIM = 4,
  NBD_CMD_CACHE = 5,
  NBD_CMD_WRITE_ZEROES = 6,
  NBD_CMD_BLOCK_STATUS = 7,
  NBD_CMD_RESIZE = 8,
};

// ---- Command flags
static constexpr uint16_t NBD_CMD_FLAG_FUA = 1 << 0;
static constexpr uint16_t NBD_CMD_FLAG_NO_HOLE = 1 << 1;
static constexpr uint16_t NBD_CMD_FLAG_DF = 1 << 2;
static constexpr uint16_t NBD_CMD_FLAG_REQ_ONE = 1 << 3;
static constexpr uint16_t NBD_CMD_FLAG_FAST_ZERO = 1 << 4;

// ---- Error values used in replies
static constexpr uint32_t NBD_OK = 0;
static constexpr uint32_t NBD_EPERM = 1;
static constexpr uint32_t NBD_EIO = 5;
static constexpr uint32_t NBD_ENOMEM = 12;
static constexpr uint32_t NBD_EINVAL = 22;
static constexpr uint32_t NBD_ENOSPC = 28;
static constexpr uint32_t NBD_EOVERFLOW = 75;
static constexpr uint32_t NBD_ENOTSUP = 95;
static constexpr uint32_t NBD_ESHUTDOWN = 108;

/// Size of a request header in bytes
static constexpr size_t NBD_REQUEST_SIZE = 28;
/// Size of a simple reply header in bytes
static constexpr size_t NBD_SIMPLE_REPLY_SIZE = 16;
/// Max export name length accepted (the protocol allows up to 4096)
static constexpr size_t NBD_MAX_NAME_LEN = 4096;

// ---- Big-endian helpers
inline void putBE16(uint8_t* p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v;
}
inline void putBE32(uint8_t* p, uint32_t v) {
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}
inline void putBE64(uint8_t* p, uint64_t v) {
  putBE32(p, (uint32_t)(v >> 32));
  putBE32(p + 4, (uint32_t)v);
}
inline uint16_t getBE16(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}
inline uint32_t getBE32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}
inline uint64_t getBE64(const uint8_t* p) {
  return ((uint64_t)getBE32(p) << 32) | getBE32(p + 4);
}

}  // namespace nbd
