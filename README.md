# Tiny Network Block Device (NBD)

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![Build: CMake](https://img.shields.io/badge/Build-CMake-064F8C.svg?logo=cmake)](CMakeLists.txt)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

__NBD__ is a simple TCP protocol that makes a remote disk look like a local one. The server exports raw storage as a list of fixed-size blocks, and the client reads and writes these blocks by offset. Unlike a network file system such as SMB or NFS, the server knows nothing about files: the client sees a plain disk (e.g. `/dev/nbd0` on Linux) and can partition it, format it and mount it with any file system. nbd is built into the Linux kernel and is supported by qemu and nbdkit.

This library implements a header-only [NBD (Network Block Device)](https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md) server for Arduino. It uses only the Arduino networking API (`Server`/`Client`), so it works with `WiFiServer`, `NetworkServer`, `EthernetServer` and other servers that provide `accept()`. The main target is the ESP32.

With it, a Linux machine (or qemu, nbdkit, …) can use storage attached to the microcontroller as a local block device, and an Arduino can read and write an nbd export on another machine.

This makes nbd an easy and efficient way to share static file content over the network: export the storage read-only, and several clients can mount it at the same time. The microcontroller only serves blocks, while each client does the file system work and caches what it has read. Only one client at a time may mount an export with write access, because clients don't know about each other's changes. [LittleFS](docs/littlefs.md) describes a ready-made setup that shares files uploaded with the sketch.

- [NBD Server](docs/server.md): exporting storage from an Arduino, block devices, example and Linux/qemu client usage.
- [NBD Client](docs/client.md): reading and writing an nbd export from an Arduino.

## Installation

### Arduino IDE

Download the library as a zip file and add it with *Sketch → Include Library → Add .ZIP Library…*, or clone it into your Arduino libraries folder:

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-nbd.git
```

I recommend git, because you can update to the latest version with `git pull` in the library folder.

The library is header-only and has no dependencies besides the Arduino core. It was tested with the ESP32 Arduino core 3.3. The SD card support uses the core's `SD` and `SD_MMC` libraries.

The optional `FatIOBlockDevice` (`src/nbd-server/FatIOBlockDevice.h`) depends on the [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) library. Install it next to this library and put its `src` folder on the include path. It is not included by `NBD.h`.

### PlatformIO

```ini
lib_deps = https://github.com/pschatzmann/arduino-nbd.git
```

## Usage

Server and client examples are in [examples/](examples/). See the documents below for details. [examples/nbd-client-tinyfatfs](examples/nbd-client-tinyfatfs/nbd-client-tinyfatfs.ino) mounts a remote export as a FAT filesystem using the [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) library's `NBDClientIO` driver, and [examples/nbd-client-littlefs](examples/nbd-client-littlefs/nbd-client-littlefs.ino) mounts one as a LittleFS filesystem with `NBDLittleFS` (`NBD_LittleFS.h`, requires the 107-Arduino-littlefs library).

## Documentation

- [NBD Server](docs/server.md): exporting storage, block devices, server example and Linux/qemu clients.
- [NBD Client](docs/client.md): API and example for reading and writing an export from an Arduino.
- [Configuration](docs/configuration.md): compile-time limits, buffer size, timeout and namespace options.
- [TinyFATFS](docs/tinyfatfs.md): how the TinyFATFS library relates to this one: `NBDClientIO` (NBD as a FAT drive) and `FatIOBlockDevice` (a FAT drive as an NBD export).
- [LittleFS](docs/littlefs.md): sharing static files from a LittleFS flash partition that is uploaded with the sketch (recommended, read-only), and mounting an export with `NBDLittleFS` or littlefs-fuse.
- [Usage Notes](docs/notes.md): SD card access, flash wear, performance and security. Read this before using the library.
- [Accessing an Export from Several Machines](docs/multiple-clients.md): what is safe when several clients share an export, and how to set this up.
- [Running on the Desktop](docs/desktop.md): running the library on Linux, macOS and Windows with the Arduino Emulator; CMake build, desktop examples, tests and `DesktopFileBlockDevice`.
