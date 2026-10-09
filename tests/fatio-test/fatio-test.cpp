/**
 * Desktop test for FatIOBlockDevice using TinyFATFS's RamIO driver.
 * Needs the TinyFATFS sources (see NBD_TINYFATFS_DIR in tests/CMakeLists.txt).
 */
#include <stdio.h>
#include <string.h>

#include "driver/RamIO.h"
#include "nbd-server/FatIOBlockDevice.h"

static int failures = 0;
#define CHECK(c)                                  \
  do {                                            \
    if (c) {                                      \
      printf("PASS: %s\n", #c);                   \
    } else {                                      \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      failures++;                                 \
    }                                             \
  } while (0)

int main() {
  fatfs::RamIO ram(64, 512);  // 64 sectors of 512 bytes
  nbd::FatIOBlockDevice dev(ram);
  CHECK(dev.begin());
  CHECK(dev.sectorSize() == 512);
  CHECK(dev.sectorCount() == 64);
  CHECK(dev.size() == 64 * 512);
  CHECK(!dev.isReadOnly());
  CHECK(dev.flush());

  // unaligned write spanning sectors, read back
  uint8_t out[1000], in[1000];
  for (int i = 0; i < 1000; i++) out[i] = (uint8_t)(i * 3 + 1);
  CHECK(dev.write(300, out, sizeof(out)));
  memset(in, 0, sizeof(in));
  CHECK(dev.read(300, in, sizeof(in)));
  CHECK(memcmp(out, in, sizeof(out)) == 0);

  // the data is visible through the driver itself (sector 0 contains 300..511)
  uint8_t sec[512];
  CHECK(ram.disk_read(0, sec, 0, 1) == fatfs::RES_OK);
  CHECK(memcmp(sec + 300, out, 512 - 300) == 0);

  // trim: only full sectors inside [512, 2048) are discarded
  CHECK(dev.trim(512, 1536));
  uint8_t zero[512];
  memset(zero, 0, sizeof(zero));
  CHECK(ram.disk_read(0, sec, 1, 1) == fatfs::RES_OK);
  CHECK(memcmp(sec, zero, 512) == 0);
  CHECK(ram.disk_read(0, sec, 3, 1) == fatfs::RES_OK);
  CHECK(memcmp(sec, zero, 512) == 0);
  // sector 0 was not inside the range and must keep its data
  CHECK(ram.disk_read(0, sec, 0, 1) == fatfs::RES_OK);
  CHECK(memcmp(sec + 300, out, 512 - 300) == 0);

  // out of range access fails
  CHECK(!dev.read(64 * 512 - 10, in, 100));

  // write zeroes through the default implementation
  uint8_t zero2[1024] = {0};
  CHECK(dev.writeZeroes(0, 1024));
  memset(in, 1, sizeof(in));
  CHECK(dev.read(0, in, 1024));
  CHECK(memcmp(in, zero2, 1024) == 0);

  // driver-written data is read back via the adapter at sector granularity
  uint8_t pat[512];
  memset(pat, 0xC3, sizeof(pat));
  CHECK(ram.disk_write(0, pat, 10, 1) == fatfs::RES_OK);
  uint8_t back[512];
  CHECK(dev.read(10 * 512, back, 512));
  CHECK(memcmp(back, pat, 512) == 0);

  printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
