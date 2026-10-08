#pragma once
/**
 * @file NBD.h
 * @brief Header-only nbd (Network Block Device) server for Arduino.
 *
 * For SD card support include NBD_SD.h (SD library) or NBD_SDMMC.h (SD_MMC
 * library) instead.
 */
#include "nbd-server/BlockDevice.h"
#include "nbd-server/ESP32PartitionBlockDevice.h"
#include "nbd-server/FileBlockDevice.h"
#include "nbd-server/MemoryBlockDevice.h"
#include "nbd-common/NBDLogger.h"
#include "nbd-common/NBDProtocol.h"
#include "nbd-client/NBDClient.h"
#include "nbd-client/NBDClientSectorBlockDevice.h"
#include "nbd-server/NBDServer.h"
#include "nbd-server/NBDSession.h"
#include "nbd-server/SDRawBlockDevice.h"
#if defined(IS_DESKTOP)
#include "nbd-server/DesktopFileBlockDevice.h"
#endif

#ifndef NBD_NO_USING_NAMESPACE
using namespace nbd;
#endif
