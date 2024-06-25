/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <vector>

namespace NEO {
struct HardwareInfo;

namespace DeviceBlobConstants {
constexpr uint32_t maxSlicesSupported = 1;
constexpr uint32_t maxDualSubSlicesSupported = 2;
constexpr uint32_t maxEuPerDualSubSlice = 3;
constexpr uint32_t maxMemoryChannels = 10;
constexpr uint32_t memoryType = 11;
constexpr uint32_t numThreadsPerEu = 15;
constexpr uint32_t maxRcs = 23;
constexpr uint32_t maxCcs = 24;
constexpr uint32_t l3BankSizeInKb = 64;
constexpr uint32_t maxSubSlicesSupported = 70;
constexpr uint32_t maxEuPerSubSlice = 71;

enum MemoryType {
    lpddr4,
    lpddr5,
    hbm2,
    hbm2e,
    gddr6
};
} // namespace DeviceBlobConstants

struct SystemInfo {

    SystemInfo(const std::vector<uint32_t> &inputData);

    ~SystemInfo() = default;

    uint32_t getMaxSlicesSupported() const { return maxSlicesSupported; }
    uint32_t getMaxDualSubSlicesSupported() const { return maxDualSubSlicesSupported; }
    uint32_t getMaxEuPerDualSubSlice() const { return maxEuPerDualSubSlice; }
    uint32_t getMemoryType() const { return memoryType; }
    uint32_t getMaxMemoryChannels() const { return maxMemoryChannels; }
    uint32_t getNumThreadsPerEu() const { return numThreadsPerEu; }
    uint32_t getMaxRCS() const { return maxRCS; }
    uint32_t getMaxCCS() const { return maxCCS; }
    uint32_t getL3BankSizeInKb() const { return l3BankSizeInKb; }

    void checkSysInfoMismatch(HardwareInfo *hwInfo);

  protected:
    void parseDeviceBlob(const std::vector<uint32_t> &inputData);

    uint32_t maxSlicesSupported = 0;
    uint32_t maxDualSubSlicesSupported = 0;
    uint32_t maxEuPerDualSubSlice = 0;
    uint32_t memoryType = 0;
    uint32_t maxMemoryChannels = 0;
    uint32_t numThreadsPerEu = 0;
    uint32_t maxRCS = 0;
    uint32_t maxCCS = 0;
    uint32_t l3BankSizeInKb = 0;
};

} // namespace NEO
