# Configuration

- `NBD_MAX_EXPORTS` (default 4) and `NBD_MAX_CLIENTS` (default 3): define these before including `NBD.h`.
- `setBufferSize()`: transfer chunk size per client.
- `setTimeout()`: timeout in ms for the handshake and for receiving a message.
- `NBD_NO_USING_NAMESPACE`: define this to stop `NBD.h` from adding `using namespace nbd`.
