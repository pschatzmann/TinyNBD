/**
 * Desktop test for NBDClient and NBDClientSectorBlockDevice. Needs the nbd-test
 * server running on localhost (see client-test.sh). Exits with 0 on success.
 */
#include <stdio.h>
#include <string.h>

#include "NBD.h"
#include "WiFi.h"

static const char* SERVER_HOST = "127.0.0.1";
static const uint16_t SERVER_PORT = nbd::NBD_DEFAULT_PORT;
static int failures = 0;

#define CHECK(cond)                                    \
  do {                                                 \
    if (cond) {                                        \
      printf("PASS: %s\n", #cond);                     \
    } else {                                           \
      printf("FAIL line %d: %s\n", __LINE__, #cond);   \
      failures++;                                      \
    }                                                  \
  } while (0)

static uint8_t big[1024 * 1024];
static uint8_t out[10000];
static uint8_t in[10000];

static void fill(uint8_t* p, size_t n, uint8_t seed) {
  for (size_t j = 0; j < n; j++) p[j] = (uint8_t)(j * 7 + seed);
}

void setup() {
  // default export ("" = first export) with metadata and block sizes
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT, "ram"));
    CHECK(c.isConnected());
    CHECK(c.size() == 4u * 1024 * 1024);
    CHECK(!c.isReadOnly());
    CHECK(c.canFlush());
    CHECK(c.preferredBlockSize() == 4096);
    CHECK(c.minBlockSize() == 1);
    CHECK(c.maxPayload() == 32u * 1024 * 1024);

    // unaligned write and read back
    fill(out, sizeof(out), 1);
    CHECK(c.write(1000, out, sizeof(out)));
    memset(in, 0, sizeof(in));
    CHECK(c.read(1000, in, sizeof(in)));
    CHECK(memcmp(out, in, sizeof(out)) == 0);
    CHECK(c.flush());
    c.disconnect();
    CHECK(!c.isConnected());
  }

  // empty export name selects the first export
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT));
    CHECK(c.size() == 4u * 1024 * 1024);
    c.disconnect();
  }

  // read-only export: reads work, writes are refused by the client
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT, "ro"));
    CHECK(c.isReadOnly());
    CHECK(c.read(0, in, 512));
    CHECK(!c.write(0, in, 512));
    c.disconnect();
  }

  // unknown export: connect fails
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(!c.connect(SERVER_HOST, SERVER_PORT, "does-not-exist"));
    CHECK(!c.isConnected());
  }

  // sector device through the BlockDevice adapter
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT, "sect"));
    nbd::NBDClientSectorBlockDevice dev(c);
    CHECK(dev.begin());
    CHECK(dev.size() == 1024u * 1024);
    CHECK(dev.preferredBlockSize() == 4096);
    CHECK(!dev.canTrim());

    fill(out, 5000, 3);
    CHECK(dev.write(3000, out, 5000));
    memset(in, 0, 5000);
    CHECK(dev.read(3000, in, 5000));
    CHECK(memcmp(out, in, 5000) == 0);

    CHECK(dev.writeZeroes(3500, 1000));
    memset(out, 0, 1000);
    CHECK(dev.read(3500, in, 1000));
    CHECK(memcmp(out, in, 1000) == 0);
    CHECK(dev.flush());
    c.disconnect();
  }

  // 1 MiB transfer through the adapter on the image export
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT, "image"));
    nbd::NBDClientSectorBlockDevice dev(c);
    fill(big, sizeof(big), 5);
    CHECK(dev.write(0, big, sizeof(big)));
    memset(in, 0, sizeof(in));
    static uint8_t back[1024 * 1024];
    memset(back, 0, sizeof(back));
    CHECK(dev.read(0, back, sizeof(back)));
    CHECK(memcmp(big, back, sizeof(big)) == 0);
    c.disconnect();
  }

  // cached export: write, overwrite and read back through the server cache
  {
    WiFiClient wifi;
    nbd::NBDClient c(wifi);
    CHECK(c.connect(SERVER_HOST, SERVER_PORT, "cached"));
    nbd::NBDClientSectorBlockDevice dev(c);
    fill(out, 5000, 9);
    CHECK(dev.write(7000, out, 5000));
    memset(in, 0, 5000);
    CHECK(dev.read(7000, in, 5000));
    CHECK(memcmp(out, in, 5000) == 0);
    // read again: served from cache pages, must still match
    memset(in, 0, 5000);
    CHECK(dev.read(7000, in, 5000));
    CHECK(memcmp(out, in, 5000) == 0);
    c.disconnect();
  }

  printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
  exit(failures ? 1 : 0);
}

void loop() {}
