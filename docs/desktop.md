# Running on the Desktop

The library also runs on Linux, macOS and Windows on top of the [Arduino Emulator](https://github.com/pschatzmann/Arduino-Emulator), which provides the Arduino API (including `WiFiServer`/`WiFiClient`) on the desktop. This is useful to:

- develop and debug sketches without flashing an ESP32,
- run the protocol tests,
- serve a disk image file from a PC.

## Requirements

- CMake 3.16 or newer and a C++17 compiler
- git: CMake downloads the Arduino Emulator automatically, and the TinyFATFS sources for the `fatio-block-device` test (see below)
- Optional, for the tests: `qemu-img` and `qemu-io` (Debian/Ubuntu package `qemu-utils`)

## Building

```bash
cmake -B build
cmake --build build
```

To use a local checkout of the emulator instead of downloading it:

```bash
cmake -B build -DFETCHCONTENT_SOURCE_DIR_ARDUINO_EMULATOR=/path/to/Arduino-Emulator
```

CMake options:

| Option | Default | Description |
|---|---|---|
| `NBD_BUILD_EXAMPLES` | `ON` | Build the desktop examples |
| `NBD_BUILD_TESTS` | `ON` | Build the test server and register the qemu test with ctest |

## Running the examples

Both examples listen on port 10809.

```bash
./build/examples/desktop/nbd-server-ramdisk-desktop      # 64 MB RAM disk, export "ram"
./build/examples/desktop/nbd-server-image-desktop        # disk.img (created with 64 MB), export "image"
```

Connect from the same machine:

```bash
qemu-img info nbd://localhost:10809/ram

sudo modprobe nbd
sudo nbd-client localhost 10809 /dev/nbd0 -N image
sudo mkfs.ext4 /dev/nbd0
sudo mount /dev/nbd0 /mnt
...
sudo umount /mnt
sudo nbd-client -d /dev/nbd0
```

## Running the tests

```bash
cd build
ctest --output-on-failure
```

The test script [tests/qemu-test.sh](../tests/qemu-test.sh) starts the test server [tests/nbd-test/nbd-test.ino](../tests/nbd-test/nbd-test.ino) and checks it with `qemu-img` and `qemu-io`. The tests cover:

- export size,
- writing and comparing a full image (RAM and file),
- unaligned writes on a sector device (read-modify-write),
- trim and write zeroes,
- read-only exports,
- unknown and default export names,
- the `cached` export (`CachedBlockDevice`).

The `fatio-block-device` test (`tests/fatio-test/`) checks `FatIOBlockDevice` against TinyFATFS's `RamIO` driver. It needs the TinyFATFS sources from <https://github.com/pschatzmann/TinyFATFS>. CMake looks for them in this order:

1. `-DNBD_TINYFATFS_DIR=/path/to/TinyFATFS`
2. a sibling `../TinyFATFS` checkout
3. a download from GitHub (`FetchContent`)

If none of these work, the test is skipped with a CMake message.

If qemu isn't installed, the test isn't registered and CMake prints a message.

## Writing a desktop sketch

Desktop sketches look the same as on the ESP32. The emulator's `WiFiServer` needs no WiFi connection:

```cpp
#include "WiFi.h"
#include "NBD.h"

WiFiServer wifiServer(NBD_DEFAULT_PORT); // 10809
NBDServer<WiFiServer> nbd_server(wifiServer);
DesktopFileBlockDevice imageDisk("disk.img", 64 * 1024 * 1024);

void setup() {
  Serial.begin(115200);
  NBDLogger::begin(Serial, NBDLogLevel::Info);
  nbd_server.addExport("image", imageDisk);
  nbd_server.begin();
}

void loop() {
  nbd_server.loop();
  delay(1);  // the emulator calls loop() continuously: avoid 100% cpu
}
```

`DesktopFileBlockDevice(path, size, readOnly)` exports a file through the C++ standard library. If you give a size, a missing file is created with that size and a smaller one is extended. If the size is 0, the file must already exist.

## Using the library in your own CMake project

```cmake
add_subdirectory(path/to/arduino-nbd)

add_executable(my-server my-server.cpp)
target_link_libraries(my-server PRIVATE arduino-nbd)
```

The `arduino-nbd` target only provides headers. Linking against it adds the include path, the `IS_DESKTOP` definition and the `arduino_emulator` library. If your project already defines an `arduino_emulator` target, that one is used and nothing is downloaded.

## Differences from the ESP32 build

- `IS_DESKTOP` is defined. It enables `DesktopFileBlockDevice` and sets the client timeout to 0 when a client connects; otherwise the emulator's `available()` waits up to 200 ms for data, which slows down every request.
- The ESP32-specific devices (`ESP32PartitionBlockDevice`, `SDBlockDevice`, `SDMMCBlockDevice`) aren't available.
- The desktop examples in `examples/desktop/` also appear in the Arduino IDE's example list, but `nbd-server-image-desktop` only compiles on the desktop.
