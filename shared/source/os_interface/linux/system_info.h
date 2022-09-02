/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <vector>

namespace NEO {
struct HardwareInfo;

struct SystemInfo {

    SystemInfo(const std::vector<uint8_t> &inputData);

    ~SystemInfo() = default;

    uint32_t getMaxSlicesSupported() const { return maxSlicesSupported; }
    uint32_t getMaxDualSubSlicesSupported() const { return maxDualSubSlicesSupported; }
    uint32_t getMaxEuPerDualSubSlice() const { return maxEuPerDualSubSlice; }
    uint32_t getMemoryType() const { return memoryType; }
    uint32_t getMaxMemoryChannels() const { return maxMemoryChannels; }
    uint32_t getNumThreadsPerEu() const { return numThreadsPerEu; }
    uint32_t getTotalVsThreads() const { return totalVsThreads; }
    uint32_t getTotalHsThreads() const { return totalHsThreads; }
    uint32_t getTotalDsThreads() const { return totalDsThreads; }
    uint32_t getTotalGsThreads() const { return totalGsThreads; }
    uint32_t getTotalPsThreads() const { return totalPsThreads; }
    uint32_t getMaxRCS() const { return maxRCS; }
    uint32_t getMaxCCS() const { return maxCCS; }
    uint32_t getL3BankSizeInKb() const { return l3BankSizeInKb; }

    void checkSysInfoMismatch(HardwareInfo *hwInfo);

  protected:
    void parseDeviceBlob(const std::vector<uint8_t> &inputData);

    uint32_t maxSlicesSupported = 0;
    uint32_t maxDualSubSlicesSupported = 0;
    uint32_t maxEuPerDualSubSlice = 0;
    uint32_t memoryType = 0;
    uint32_t maxMemoryChannels = 0;
    uint32_t numThreadsPerEu = 0;
    uint32_t totalVsThreads = 0;
    uint32_t totalHsThreads = 0;
    uint32_t totalDsThreads = 0;
    uint32_t totalGsThreads = 0;
    uint32_t totalPsThreads = 0;
    uint32_t maxRCS = 0;
    uint32_t maxCCS = 0;
    uint32_t l3BankSizeInKb = 0;
};

} // namespace NEO
