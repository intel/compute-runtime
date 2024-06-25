/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <chrono>
#include <map>
#include <string>
const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("unknown");
const std::string intelPciId("0x8086");
const std::string guid64BitMemoryCounters("0xb15a0ede");
constexpr uint32_t mbpsToBytesPerSecond = 125000;
constexpr double milliVoltsFactor = 1000.0;
constexpr uint32_t maxRasErrorCategoryCount = 7;
constexpr uint32_t maxRasErrorCategoryExpCount = 10;

constexpr double maxPerformanceFactor = 100;
constexpr double halfOfMaxPerformanceFactor = 50;
constexpr double minPerformanceFactor = 0;

constexpr uint32_t numSocTemperatureEntries = 7;     // entries would be PCH or GT_TEMP, DRAM, SA, PSF, DE, PCIE, TYPEC
constexpr uint32_t numCoreTemperatureEntries = 4;    // entries would be CORE0, CORE1, CORE2, CORE3
constexpr uint32_t numComputeTemperatureEntries = 3; // entries would be IA, GT and LLC
constexpr uint32_t invalidMaxTemperature = 125;
constexpr uint32_t invalidMinTemperature = 10;

namespace L0 {
namespace Sysman {
struct SteadyClock {
    typedef std::chrono::duration<uint64_t, std::milli> duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::time_point<SteadyClock> time_point;
    static time_point now() noexcept {
        static auto epoch = std::chrono::steady_clock::now();
        return time_point(std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - epoch));
    }
};

} // namespace Sysman
} // namespace L0

namespace PciLinkSpeeds {
constexpr double pci2Dot5GigaTransfersPerSecond = 2.5;
constexpr double pci5GigaTransfersPerSecond = 5.0;
constexpr double pci8GigaTransfersPerSecond = 8.0;
constexpr double pci16GigaTransfersPerSecond = 16.0;
constexpr double pci32GigaTransfersPerSecond = 32.0;

} // namespace PciLinkSpeeds
enum PciGenerations {
    PciGen1 = 1,
    PciGen2,
    PciGen3,
    PciGen4,
    PciGen5,
};

constexpr uint8_t maxPciBars = 6;
// Linux kernel would report 255 link width, as an indication of unknown.
constexpr uint32_t unknownPcieLinkWidth = 255u;

constexpr uint32_t microSecondsToNanoSeconds = 1000u;

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint64_t minTimeoutModeHeartbeat = 5000u;
constexpr uint64_t minTimeoutInMicroSeconds = 1000u;
constexpr uint16_t milliSecsToMicroSecs = 1000;
constexpr uint32_t milliFactor = 1000u;
constexpr uint32_t microFacor = milliFactor * milliFactor;
constexpr uint64_t gigaUnitTransferToUnitTransfer = 1000 * 1000 * 1000;

constexpr int32_t memoryBusWidth = 128; // bus width in bytes
constexpr int32_t numMemoryChannels = 8;
constexpr uint32_t unknownMemoryType = UINT32_MAX;
#define BITS(x, at, width) (((x) >> (at)) & ((1 << (width)) - 1))

const std::map<std::string, zes_engine_type_flag_t> sysfsEngineMapToLevel0EngineType = {
    {"rcs", ZES_ENGINE_TYPE_FLAG_RENDER},
    {"ccs", ZES_ENGINE_TYPE_FLAG_COMPUTE},
    {"bcs", ZES_ENGINE_TYPE_FLAG_DMA},
    {"vcs", ZES_ENGINE_TYPE_FLAG_MEDIA},
    {"vecs", ZES_ENGINE_TYPE_FLAG_OTHER}};