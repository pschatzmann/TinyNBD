#pragma once
#include "NBDSession.h"

#ifndef NBD_MAX_EXPORTS
#define NBD_MAX_EXPORTS 4
#endif

#ifndef NBD_MAX_CLIENTS
#define NBD_MAX_CLIENTS 3
#endif

namespace nbd {

namespace internal {
template <class T>
T&& declval();

// calls setNoDelay(true) if the client supports it
template <class C>
auto setNoDelay(C& client, int) -> decltype(client.setNoDelay(true), void()) {
  client.setNoDelay(true);
}
template <class C>
void setNoDelay(C&, long) {}

// uses hasClient() if the server supports it, so that we only call a
// potentially slow accept() when there is a pending connection
template <class S>
auto hasClient(S& server, int) -> decltype((bool)server.hasClient()) {
  return server.hasClient();
}
template <class S>
bool hasClient(S&, long) {
  return true;
}
}  // namespace internal

/**
 * @brief nbd server for any Arduino Server which provides accept() (e.g.
 * WiFiServer, NetworkServer, EthernetServer). Call loop() from the Arduino
 * loop().
 *
 * @tparam ServerT Arduino server class
 * @tparam ClientT client class returned by ServerT::accept()
 */
template <class ServerT,
          class ClientT = decltype(internal::declval<ServerT&>().accept())>
class NBDServer {
 public:
  explicit NBDServer(ServerT& server) : server(server) {}

  /// Adds an export: the first export is also used for an empty export name
  bool addExport(const char* name, BlockDevice& device, bool readOnly = false,
                 const char* description = nullptr) {
    if (export_count >= NBD_MAX_EXPORTS) return false;
    NBDExport& exp = exports[export_count++];
    exp.name = name;
    exp.description = description;
    exp.device = &device;
    exp.read_only = readOnly;
    return true;
  }

  /// Transfer chunk size (power of 2, default 4096): call before begin()
  void setBufferSize(size_t size) { buffer_size = size; }

  /// Timeout in ms for the handshake and for receiving a message
  void setTimeout(uint32_t ms) { timeout_ms = ms; }

  /// Starts the block devices and the server
  bool begin() {
    for (int j = 0; j < export_count; j++) {
      if (!exports[j].device->begin()) {
        NBD_LOGE("Could not start device of export '%s'", exports[j].name);
        return false;
      }
      NBD_LOGI("Export '%s': %llu bytes", exports[j].name,
               (unsigned long long)exports[j].device->size());
    }
    for (auto& slot : slots) {
      slot.session.setExports(exports, export_count);
      slot.session.setBufferSize(buffer_size);
      slot.session.setTimeout(timeout_ms);
    }
    server.begin();
    is_active = true;
    return true;
  }

  /// Disconnects all clients
  void end() {
    for (auto& slot : slots) closeSlot(slot);
    is_active = false;
  }

  /// Accepts new connections and processes requests
  void loop() {
    if (!is_active) return;
    processClients();
    // accept after processing, so that a pending NBD_CMD_DISC frees its slot
    acceptClient();
  }

  /// Number of connected clients
  int clientCount() {
    int result = 0;
    for (auto& slot : slots)
      if (slot.active) result++;
    return result;
  }

 protected:
  struct Slot {
    ClientT client;
    NBDSession session;
    bool active = false;
  };
  ServerT& server;
  NBDExport exports[NBD_MAX_EXPORTS];
  int export_count = 0;
  Slot slots[NBD_MAX_CLIENTS];
  size_t buffer_size = 4096;
  uint32_t timeout_ms = 10000;
  bool is_active = false;

  void processClients() {
    for (auto& slot : slots) {
      if (!slot.active) continue;
      // process the available requests (limited to stay responsive)
      for (int j = 0; j < 16 && slot.client.available() > 0; j++) {
        if (!slot.session.processRequest(slot.client)) {
          closeSlot(slot);
          break;
        }
      }
      if (slot.active && !slot.client.connected() &&
          slot.client.available() <= 0) {
        NBD_LOGI("Connection lost");
        closeSlot(slot);
      }
    }
  }

  Slot* freeSlot() {
    for (auto& slot : slots) {
      if (!slot.active) return &slot;
    }
    return nullptr;
  }

  void acceptClient() {
    if (!internal::hasClient(server, 0)) return;
    ClientT client = server.accept();
    if (!client) return;
    Slot* free_slot = freeSlot();
    if (free_slot == nullptr) {
      // a pending NBD_CMD_DISC might free a slot
      processClients();
      free_slot = freeSlot();
    }
    if (free_slot == nullptr) {
      NBD_LOGE("Too many clients: connection rejected");
      client.stop();
      return;
    }
    NBD_LOGI("New connection");
    internal::setNoDelay(client, 0);
#if defined(IS_DESKTOP)
    // the emulator's available() would otherwise wait for data
    client.setTimeout(0);
#endif
    free_slot->client = client;
    if (free_slot->session.handshake(free_slot->client)) {
      free_slot->active = true;
    } else {
      NBD_LOGI("Handshake ended without transmission");
      free_slot->client.stop();
    }
  }

  void closeSlot(Slot& slot) {
    if (!slot.active) return;
    slot.session.close();
    slot.client.stop();
    slot.active = false;
  }
};

}  // namespace nbd
